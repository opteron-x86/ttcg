#include "CardArt.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(){
 namespace fs=std::filesystem;
 const auto first=std::string(ttcg::cards[0].name),second=std::string(ttcg::cards[1].name),third=std::string(ttcg::cards[2].name);
 const auto root=fs::temp_directory_path()/"ttcg-card-art-roster-test";
 assert(!fs::exists(root));
 for(const auto* folder:{"cards","thumbs","backs"})fs::create_directories(root/folder);
 auto touch=[&](const std::string& name){std::ofstream(root/name)<<"fixture";};
 touch(std::string("cards/")+first+".webp");touch(std::string("thumbs/")+first+".webp");touch("backs/mosaic.webp");
 ttcg::refreshCardArt(root);
 assert(ttcg::cardImages[0].art==std::string("cards/")+first+".webp");
 assert(ttcg::cardImages[0].thumbnail==std::string("thumbs/")+first+".webp");
 touch(std::string("cards/")+first+".JPG");touch(std::string("thumbs/")+first+".png");touch("backs/mosaic.png");
 ttcg::refreshCardArt(root);
 assert(ttcg::cardImages[0].art==std::string("cards/")+first+".JPG");
 assert(ttcg::cardImages[0].thumbnail==std::string("thumbs/")+first+".png");
 assert(ttcg::cardBack=="backs/mosaic.png");
 touch(std::string("cards/")+first+".png");touch(std::string("cards/")+second+".jpeg");touch(std::string("cards/")+third+".gif");
 ttcg::refreshCardArt(root);
 assert(ttcg::cardImages[0].art==std::string("cards/")+first+".png");
 assert(ttcg::cardImages[1].art==std::string("cards/")+second+".jpeg");
 assert(ttcg::cardImages[1].thumbnail==ttcg::cardImages[1].art);
 assert(ttcg::cardImages[2].art==std::string("cards/")+third+".webp");
 fs::remove_all(root);
 ttcg::refreshCardArt("TesseraUI/art");
 for(const auto& card:ttcg::cardImages){assert(fs::is_regular_file(fs::path("TesseraUI/art")/card.art));assert(fs::is_regular_file(fs::path("TesseraUI/art")/card.thumbnail));}
 std::cout<<"Card art: all current filenames, PNG/JPG/JPEG/WebP priorities, case handling, thumbnail fallback and back overrides passed\n";
}
