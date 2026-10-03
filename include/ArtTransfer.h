#pragma once
#include "CardArt.h"
#include <charconv>
#include <cstdint>
#include <fstream>
#include <optional>

namespace ttcg {
inline constexpr std::size_t maxTransferredArtBytes=16*1024*1024;
struct ArtRequest {std::uint64_t id;std::string path;};
inline std::optional<ArtRequest> parseArtRequest(std::string_view text){
 if(text.size()>2048)return {};
 const auto end=text.find('\n');if(end==std::string_view::npos||end==0||end+1==text.size())return {};
 std::uint64_t id=0;const auto parsed=std::from_chars(text.data(),text.data()+end,id);
 if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+end||!id||id>9007199254740991ull)return {};
 return ArtRequest{id,std::string(text.substr(end+1))};
}
inline bool transferableArtPath(std::string_view path){
 if(path.empty()||path.size()>1024||path.front()=='/'||path.back()=='/')return false;
 for(unsigned char c:path)if(c<32||c==127||std::string_view("\\:*?\"<>|").find(c)!=std::string_view::npos)return false;
 for(std::size_t start=0;start<path.size();){
  const auto end=path.find('/',start);const auto part=path.substr(start,end==std::string_view::npos?path.size()-start:end-start);
  if(part.empty()||part=="."||part==".."||part.back()=='.'||part.back()==' ')return false;
  if(end==std::string_view::npos)break;start=end+1;
 }
 return std::any_of(cardImages.begin(),cardImages.end(),[&](const auto& image){return path==image.art||path==image.thumbnail;})||
        std::any_of(cardBacks.begin(),cardBacks.end(),[&](const auto& back){return path==back.art;});
}
inline std::string artBase64(std::string_view bytes){
 constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 std::string out;out.reserve((bytes.size()+2)/3*4);
 for(std::size_t i=0;i<bytes.size();i+=3){
  const auto a=static_cast<unsigned char>(bytes[i]);
  const auto b=i+1<bytes.size()?static_cast<unsigned char>(bytes[i+1]):0;
  const auto c=i+2<bytes.size()?static_cast<unsigned char>(bytes[i+2]):0;
  out+=alphabet[a>>2];out+=alphabet[((a&3)<<4)|(b>>4)];
  out+=i+1<bytes.size()?alphabet[((b&15)<<2)|(c>>6)]:'=';out+=i+2<bytes.size()?alphabet[c&63]:'=';
 }
 return out;
}
struct ArtTransfer {std::string data,error;};
inline ArtTransfer readCardArt(std::string_view path,const std::filesystem::path& root="Data/MeridianUI/ttcg/art"){
 // Only catalogued raster assets are readable. Open the virtual Data path
 // directly: canonicalizing it can resolve MO2 files into different mod roots.
 if(!transferableArtPath(path))return {{},"unregistered artwork"};
 const auto full=root/std::filesystem::path(std::u8string(path.begin(),path.end()));
 const auto extension=artLower(artUTF8(full.extension()));
 const std::string mime=extension==".png"?"image/png":extension==".jpg"||extension==".jpeg"?"image/jpeg":extension==".webp"?"image/webp":"";
 if(mime.empty())return {{},"unsupported image format"};
 std::ifstream file(full,std::ios::binary|std::ios::ate);
 if(!file)return {{},"artwork file missing or unreadable"};
 const auto size=file.tellg();
 if(size<=0||size>std::streamoff(maxTransferredArtBytes))return {{},"artwork size outside supported range"};
 std::string bytes(static_cast<std::size_t>(size),'\0');file.seekg(0);
 if(!file.read(bytes.data(),static_cast<std::streamsize>(bytes.size())))return {{},"artwork read failed"};
 const bool png=bytes.starts_with(std::string_view("\x89PNG\r\n\x1a\n",8));
 const bool jpeg=bytes.starts_with(std::string_view("\xff\xd8\xff",3));
 const bool webp=bytes.size()>=12&&bytes.starts_with("RIFF")&&bytes.substr(8,4)=="WEBP";
 if(!(mime=="image/png"?png:mime=="image/jpeg"?jpeg:webp))return {{},"image contents do not match file format"};
 return {"data:"+mime+";base64,"+artBase64(bytes),{}};
}
}
