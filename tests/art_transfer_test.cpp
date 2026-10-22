#include "ArtTransfer.h"
#include <cassert>
#include <chrono>
#include <iostream>
int main(){
 using namespace ttcg;namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/("ttcg-art-transfer-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));assert(!fs::exists(root));
 fs::create_directories(root/"main/art");fs::create_directories(root/"separate-art/cards");fs::create_directories(root/"separate-art/thumbs");
 fs::create_directory_symlink(root/"separate-art/cards",root/"main/art/cards");
 fs::create_directory_symlink(root/"separate-art/thumbs",root/"main/art/thumbs");
 const std::string name=cards[0].name;
 const auto png=std::string("\x89PNG\r\n\x1a\n",8)+"fixture";
 auto write=[&](const fs::path& file,std::string_view bytes){std::ofstream out(file,std::ios::binary);out.write(bytes.data(),bytes.size());};
 write(root/"separate-art/cards"/(name+".png"),png);
 refreshCardArt(root/"main/art");
 // The browser's physical containment check rejects this separate-folder
 // layout, although opening the virtual asset path succeeds.
 const auto canonicalRoot=fs::weakly_canonical(root/"main");
 const auto canonicalImage=fs::weakly_canonical(root/"main/art/cards"/(name+".png"));
 assert(*canonicalImage.lexically_relative(canonicalRoot).begin()=="..");
 auto image=readCardArt("cards/"+name+".png",root/"main/art");
 assert(image.error.empty()&&image.data=="data:image/png;base64,"+artBase64(png));
 assert(artBase64("")==""&&artBase64("f")=="Zg=="&&artBase64("fo")=="Zm8="&&artBase64("foo")=="Zm9v");
 assert(artBase64(std::string_view("\0\xff\xfe",3))=="AP/+" );
 for(auto raw:{"1\ncards/test.png","9007199254740991\nbacks/mosaic.png"})assert(parseArtRequest(raw));
 for(auto raw:{"","0\nbacks/mosaic.png","-1\nbacks/mosaic.png","1x\nbacks/mosaic.png","9007199254740992\nbacks/mosaic.png","1\n","1 backs/mosaic.png"})assert(!parseArtRequest(raw));
 assert(!parseArtRequest("1\n"+std::string(2048,'a')));
 for(auto path:{"../outside.png","cards/../../outside.png","C:/outside.png","/outside.png","cards\\outside.png","cards//test.png","cards/./test.png","cards/test.png.","cards/test.png ","cards/test.png:secret","cards/unknown.png","cards/test\n.png"})assert(!transferableArtPath(path));
 // Even a maliciously registered path cannot escape the allowed image syntax.
 for(auto path:{"../outside.png","C:/outside.png","cards/test.png:secret"}){cardImages[0].art=path;assert(!transferableArtPath(path));}
 refreshCardArt(root/"main/art");
 fs::remove(root/"separate-art/cards"/(name+".png"));
 write(root/"separate-art/cards"/(name+".JPG"),std::string_view("\xff\xd8\xff",3));
 refreshCardArt(root/"main/art");assert(cardImages[0].art=="cards/"+name+".JPG");
 assert(readCardArt(cardImages[0].art,root/"main/art").data.starts_with("data:image/jpeg;base64,"));
 fs::remove(root/"separate-art/cards"/(name+".JPG"));
 write(root/"separate-art/cards"/(name+".webp"),std::string_view("RIFF\x04\0\0\0WEBP",12));
 refreshCardArt(root/"main/art");assert(readCardArt(cardImages[0].art,root/"main/art").data.starts_with("data:image/webp;base64,"));
 write(root/"separate-art/cards"/(name+".webp"),"<svg>not a raster</svg>");
 assert(!readCardArt(cardImages[0].art,root/"main/art").error.empty());
 fs::resize_file(root/"separate-art/cards"/(name+".webp"),maxTransferredArtBytes+1);
 assert(readCardArt(cardImages[0].art,root/"main/art").error=="artwork size outside supported range");
 fs::remove(root/"separate-art/cards"/(name+".webp"));
 assert(readCardArt(cardImages[0].art,root/"main/art").error=="artwork file missing or unreadable");
 fs::create_directories(root/"main/art/backs/custom");
 const std::string custom="backs/custom/Moon & Star's_夜 100%.PNG";
 write(root/"main/art"/fs::path(std::u8string(custom.begin(),custom.end())),png);
 refreshCardArt(root/"main/art");assert(readCardArt(custom,root/"main/art").data==image.data);
 fs::remove_all(root);
 std::cout<<"Art transfer: separate physical roots, raster formats, exact bytes, Unicode, allowlist, traversal rejection, missing/corrupt/oversize assets passed\n";
}
