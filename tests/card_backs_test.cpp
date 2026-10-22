#include "CardArt.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace ttcg;
int main(){
 namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/("ttcg-back-fixtures-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root/"backs/custom");
 auto touch=[&](std::string name){std::ofstream(root/fs::path(std::u8string(name.begin(),name.end())))<<"image fixture";};
 for(const auto* name:{"mosaic","dragon","celestial"})touch(std::string("backs/")+name+".webp");
 refreshCardBacks(root);assert(cardBacks.size()==3);
 assert(cardBacks[0].id=="mosaic"&&cardBacks[1].id=="dragon"&&cardBacks[2].id=="celestial");
 for(const auto& back:cardBacks)assert(!back.custom);
 std::string persisted,notice;int writes=0;bool writable=true;
 auto save=[&](std::string_view id){++writes;if(!writable)return false;persisted=cardBackSetting(id);return true;};
 auto command=[&](const std::string& text){std::istringstream in(text);return cardBackCommand(in,notice,save,root);};
 assert(command("select dragon"));assert(cardBackID=="dragon"&&cardBack=="backs/dragon.webp"&&writes==1);
 assert(command("select dragon")&&writes==1);
 assert(!command("refresh extra")&&!command("select")&&!command("unknown"));
 assert(command("select ../../outside")&&!notice.empty()&&writes==1&&cardBackID=="dragon");
 writable=false;assert(command("select celestial"));assert(!notice.empty()&&cardBackID=="dragon"&&cardBackPreference=="dragon");writable=true;
 touch("backs/custom/Moon & Star's_夜 100%.PNG");touch("backs/custom/mosaic.webp");
 touch("backs/Extra.jpg");touch("backs/dragon.JPG");touch("backs/custom/ignored.gif");touch("backs/custom/README.txt");
 touch("backs/custom/ broken.png");touch("backs/custom/bad\nname.png");
 fs::create_directories(root/"backs/custom/nested.png");touch("backs/custom/nested.png/hidden.png");
 const auto version=cardBackVersion;assert(command("refresh")&&notice.empty()&&cardBackVersion>version);
 assert(cardBacks.size()==6&&cardBack=="backs/dragon.JPG");
 assert(findCardBack("custom/mosaic")->custom&&findCardBack("extra")->custom);
 const std::string id="custom/moon & star's_夜 100%";
 const auto* custom=findCardBack(id);assert(custom&&custom->name=="Moon & Star's 夜 100%"&&custom->custom);
 assert(command("select "+id));assert(cardBackPreference==id&&cardBackID==id);
 assert(persisted.find("%20")!=persisted.npos&&persisted.find("%25")!=persisted.npos);
 assert(std::all_of(persisted.begin(),persisted.end(),[](unsigned char c){return c<128;}));
 cardBackPreference="mosaic";applyCardBackPreference();assert(cardBackID=="mosaic");
 cardBackPreference=cardBackFromSetting(persisted);refreshCardBacks(root);assert(cardBackID==id);
 fs::remove(root/fs::path(u8"backs/custom/Moon & Star's_夜 100%.PNG"));
 assert(command("refresh"));assert(cardBackID=="mosaic"&&cardBackPreference==id);
 touch("backs/custom/Moon & Star's_夜 100%.webp");assert(command("refresh"));
 assert(cardBackID==id&&cardBack.ends_with(".webp"));
 // Long UTF-8 names exceed the old generic command limit but still round-trip.
 std::string longName;for(int i=0;i<60;++i)longName+="夜";
 touch("backs/custom/"+longName+".png");assert(command("refresh"));
 assert(command("select custom/"+longName)&&cardBackID=="custom/"+longName);
 assert(cardBackFromSetting(cardBackSetting(cardBackID))==cardBackID);
 assert(cardBackFromSetting("")=="mosaic"&&cardBackFromSetting("mosaic")=="mosaic");
 fs::remove_all(root);cardBackPreference="mosaic";refreshCardBacks("TesseraUI/art");
 assert(cardBacks.size()>=3);for(const auto& back:cardBacks)assert(fs::is_regular_file(fs::path("TesseraUI/art")/back.art));
 std::cout<<"Card backs: built-ins, custom scanning, Unicode/long names, extension priority, persistent selection, failed saves and missing/restored images passed\n";
}
