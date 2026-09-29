#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace ttcg {
struct MusicScan {
 std::vector<std::filesystem::path> files;
 std::vector<std::filesystem::path> rejected;
 unsigned duplicates=0;
};
// XWM is a RIFF/XWMA container. Reject truncated chunks and malformed format
// headers before passing user files to Skyrim's native decoder.
inline bool validXwm(const std::filesystem::path& path) {
 std::ifstream in(path,std::ios::binary);
 auto u32=[](const unsigned char* p) { return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24); };
 std::array<unsigned char,12> header{};
 if(!in.read(reinterpret_cast<char*>(header.data()),12)) return false;
 if(std::string(reinterpret_cast<char*>(header.data()),4)!="RIFF"||std::string(reinterpret_cast<char*>(header.data()+8),4)!="XWMA") return false;
 in.seekg(0,std::ios::end); auto bytes=static_cast<std::uint64_t>(in.tellg());
 const std::uint64_t end=std::uint64_t(u32(header.data()+4))+8;
 if(end!=bytes) return false;
 bool format=false,data=false,packets=false;
 std::uint64_t pos=12;
 while(pos+8<=end) {
   in.seekg(static_cast<std::streamoff>(pos));
   std::array<unsigned char,8> chunk{};
   if(!in.read(reinterpret_cast<char*>(chunk.data()),8)) return false;
   std::string tag(reinterpret_cast<char*>(chunk.data()),4);
   auto size=u32(chunk.data()+4);
   if(pos+8+size+(size&1)>end) return false;
   if(tag=="fmt ") {
     if(format||size<18||size>44) return false;
     std::array<unsigned char,44> fmt{};
     if(!in.read(reinterpret_cast<char*>(fmt.data()),size)) return false;
     unsigned codec=fmt[0]|unsigned(fmt[1])<<8,channels=fmt[2]|unsigned(fmt[3])<<8;
     unsigned extra=fmt[16]|unsigned(fmt[17])<<8;
     if((codec!=0x161&&codec!=0x162)||channels<1||channels>2||u32(fmt.data()+4)==0||extra+18>size) return false;
     format=true;
   } else if(tag=="data") data=size>0;
   else if(tag=="dpds") packets=size>0&&size%4==0;
   pos+=8+size+(size&1);
 }
 return pos==end&&format&&data&&packets;
}
inline MusicScan scanMusic(const std::filesystem::path& folder) {
 MusicScan result;
 std::error_code error;
 std::vector<std::filesystem::path> candidates;
 std::filesystem::recursive_directory_iterator it(folder,std::filesystem::directory_options::skip_permission_denied,error),end;
 for(;!error&&it!=end;it.increment(error)) {
   if(!it->is_regular_file(error)) continue;
   auto ext=it->path().extension().string();
   std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){ return char(std::tolower(c)); });
   if(ext==".xwm") candidates.push_back(it->path());
 }
 std::sort(candidates.begin(),candidates.end());
 std::set<std::pair<std::uint64_t,std::uint64_t>> seen;
 for(const auto& file:candidates) {
   if(!validXwm(file)) { result.rejected.push_back(file); continue; }
   std::ifstream input(file,std::ios::binary);
   std::uint64_t hash=14695981039346656037ull,size=0;
   std::array<char,32768> buffer{};
   while(input.read(buffer.data(),buffer.size())||input.gcount()) {
     size+=input.gcount();
     for(std::streamsize i=0;i<input.gcount();++i) { hash^=static_cast<unsigned char>(buffer[i]); hash*=1099511628211ull; }
   }
   if(input.bad()) { result.rejected.push_back(file); continue; }
   if(seen.emplace(size,hash).second) result.files.push_back(file);
   else ++result.duplicates;
 }
 return result;
}

// Backend controls actual sound handles. No game music-queue updates are needed
// while the match menu pauses the world. Times are monotonic milliseconds.
class FolderPlaylist {
 std::vector<std::filesystem::path> files;
 std::size_t next=0,failed=0;
 std::uint64_t submittedAt=0;
 bool running=false,audible=false;
 std::mt19937 random{std::random_device{}()};
 template<class Audio> bool submit(Audio& audio,std::uint64_t now) {
   while(failed<files.size()) {
     if(next==files.size()) { next=0; std::shuffle(files.begin(),files.end(),random); }
     auto path=files[next++];
     if(audio.play(path)) { submittedAt=now; audible=false; return true; }
     audio.stop();
     ++failed;
   }
   stop(audio); return false;
 }
public:
 bool active() const { return running; }
 template<class Audio> bool start(std::vector<std::filesystem::path> paths,Audio& audio,std::uint64_t now) {
   stop(audio); files=std::move(paths); next=failed=0;
   if(files.empty()) return false;
   std::shuffle(files.begin(),files.end(),random);
   running=true; return submit(audio,now);
 }
 template<class Audio> void stop(Audio& audio) {
   if(!running) return;
   running=false; audible=false; audio.stop(); audio.restore();
 }
 template<class Audio> void tick(Audio& audio,std::uint64_t now) {
   if(!running) return;
   if(audio.playing()) {
     if(!audible) { audible=true; failed=0; audio.suspend(); }
     return;
   }
   if(!audible&&now-submittedAt<3000) return; // Asynchronous decoder startup.
   if(!audible) { ++failed; audio.failed(); }
   audio.stop(); submit(audio,now);
 }
};
}
