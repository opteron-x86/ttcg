#pragma once
#include "Cards.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <istream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ttcg {
struct CardImages { std::string art, thumbnail; };
inline std::array<CardImages,cards.size()> cardImages=[] {
 std::array<CardImages,cards.size()> result{};
 for(std::size_t i=0;i<cards.size();++i)
   result[i]={cards[i].art,"thumbs/"+std::string(cards[i].name)+".webp"};
 return result;
}();
inline std::string cardBack="backs/mosaic.png";
struct CardBackImage {std::string id,name,art;bool custom=false;};
inline std::vector<CardBackImage> cardBacks{{"mosaic","Mosaic","backs/mosaic.png"}};
inline std::string cardBackPreference="mosaic",cardBackID="mosaic";
inline unsigned cardBackVersion=0;
inline std::string artUTF8(const std::filesystem::path& path){const auto text=path.u8string();return {text.begin(),text.end()};}
inline std::string artLower(std::string value) {
 std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
 return value;
}
// Scan the game's virtual Data directory once per menu opening. An alternate
// extension takes priority over the shipped format, including in MO2 overlays.
inline std::map<std::string,std::string> scanArt(const std::filesystem::path& root,const std::string& folder,bool thumbs=false) {
 const std::vector<std::string> formats=thumbs?std::vector<std::string>{".png",".jpg",".jpeg",".webp"}:std::vector<std::string>{".webp",".jpg",".jpeg",".png"};
 std::map<std::string,std::pair<std::size_t,std::string>> selected;
 std::error_code error;
 std::filesystem::directory_iterator it(root/folder,std::filesystem::directory_options::skip_permission_denied,error),end;
 for(;!error&&it!=end;it.increment(error)) {
   if(!it->is_regular_file(error))continue;
   const auto path=it->path();const auto extension=artLower(artUTF8(path.extension()));
   auto format=std::find(formats.begin(),formats.end(),extension);if(format==formats.end())continue;
   const auto key=artLower(artUTF8(path.stem()));
   const auto candidate=std::make_pair(std::size_t(format-formats.begin()),folder+"/"+artUTF8(path.filename()));
   auto existing=selected.find(key);
   if(existing==selected.end()||candidate<existing->second)selected[key]=candidate;
 }
 std::map<std::string,std::string> result;
 for(const auto& [key,value]:selected)result[key]=value.second;
 return result;
}
inline const CardBackImage* findCardBack(std::string_view id){
 const auto it=std::find_if(cardBacks.begin(),cardBacks.end(),[&](const auto& b){return b.id==id;});
 return it==cardBacks.end()?nullptr:&*it;
}
inline void applyCardBackPreference(){
 const auto* chosen=findCardBack(cardBackPreference);if(!chosen)chosen=findCardBack("mosaic");
 cardBackID=chosen?chosen->id:"mosaic";cardBack=chosen?chosen->art:"backs/mosaic.png";
}
inline void refreshCardBacks(const std::filesystem::path& root="Data/MeridianUI/ttcg/art"){
 const auto shipped=scanArt(root,"backs"),custom=scanArt(root,"backs/custom");
 cardBacks.clear();
 // Keep the default first, and use filename stems as stable preferences when
 // a replacer changes extension. Custom names are scoped separately.
 const std::pair<std::string,std::string> builtins[]={{"mosaic","Mosaic"},{"dragon","Dragon"},{"celestial","Celestial"}};
 for(const auto& [id,name]:builtins){
   const auto found=shipped.find(id);
   if(found!=shipped.end())cardBacks.push_back({id,name,found->second});
   else if(id=="mosaic")cardBacks.push_back({id,name,"backs/mosaic.png"});
 }
 auto add=[&](const auto& files,bool isCustom){for(const auto& [stem,path]:files){
   if(!isCustom&&std::any_of(std::begin(builtins),std::end(builtins),[&](const auto& b){return b.first==stem;}))continue;
   // Keep preferences round-trippable through the Windows INI and bridge.
   if(stem.empty()||stem.size()>768||std::isspace(static_cast<unsigned char>(stem.front()))||
      std::isspace(static_cast<unsigned char>(stem.back()))||
      std::any_of(stem.begin(),stem.end(),[](unsigned char c){return c<32||std::string_view("\\/:*?\"<>|").find(c)!=std::string_view::npos;}))continue;
   auto name=artUTF8(std::filesystem::path(std::u8string(path.begin(),path.end())).stem());
   std::replace(name.begin(),name.end(),'_',' ');
   cardBacks.push_back({(isCustom?"custom/":"")+stem,name,path,true});
 }};
 add(shipped,false);add(custom,true);++cardBackVersion;applyCardBackPreference();
}
template<class Save> bool selectCardBack(std::string_view id,Save save){
 if(!findCardBack(id))return false;
 if(cardBackPreference!=id&&!save(id))return false;
 cardBackPreference=id;applyCardBackPreference();return true;
}
// Existing INIs are often ANSI. Store UTF-8 filename bytes as ASCII so custom
// names survive Windows profile API writes without changing the file encoding.
inline std::string cardBackSetting(std::string_view id){
 constexpr char hex[]="0123456789ABCDEF";std::string out;
 for(unsigned char c:id){if(c<=32||c>=127||c=='%'){out+='%';out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}
 return out;
}
inline std::string cardBackFromSetting(std::string_view value){
 auto digit=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;};
 std::string out;
 for(std::size_t i=0;i<value.size();++i){
   if(value[i]=='%'&&i+2<value.size()&&digit(value[i+1])>=0&&digit(value[i+2])>=0){out+=char(digit(value[i+1])*16+digit(value[i+2]));i+=2;}
   else out+=value[i];
 }
 return out.empty()?"mosaic":out;
}
template<class Save> bool cardBackCommand(std::istream& input,std::string& notice,Save save,const std::filesystem::path& root="Data/MeridianUI/ttcg/art"){
 std::string action,id;if(!(input>>action))return false;
 if(action=="refresh"){
   if(input>>id)return false;
   refreshCardBacks(root);notice.clear();return true;
 }
 if(action!="select"||!std::getline(input>>std::ws,id))return false;
 if(!findCardBack(id)){notice="Card back unavailable. Refresh the list.";return true;}
 notice=selectCardBack(id,save)?"":"Couldn't save the card back.";return true;
}
inline void refreshCardArt(const std::filesystem::path& root="Data/MeridianUI/ttcg/art") {
 const auto full=scanArt(root,"cards"),thumbs=scanArt(root,"thumbs",true);
 for(std::size_t i=0;i<cards.size();++i) {
   const auto key=artLower(cards[i].name);auto face=full.find(key),thumb=thumbs.find(key);
   cardImages[i].art=face==full.end()?cards[i].art:face->second;
   cardImages[i].thumbnail=thumb==thumbs.end()?cardImages[i].art:thumb->second;
 }
 refreshCardBacks(root);
}
}
