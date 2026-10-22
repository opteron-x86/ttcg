// Exercise exported editor packages with the real runtime parser/registry.
#include "ExpansionLoader.h"
#include <iostream>
int main(int argc,char** argv){
 if(argc!=2)return 2;
 const auto root=std::filesystem::path(argv[1]);
 const auto report=ttcg::loadCardExpansions(root/"SKSE/Plugins/Tessera/Expansions");
 for(const auto& error:report.errors)std::cerr<<error<<'\n';
 if(!report.errors.empty()||report.sets!=1)return 1;
 for(const auto& card:ttcg::expansionCards){
  for(const auto* art:{&card.art,&card.thumbnail})
   if(*art!="backs/mosaic.webp"&&!std::filesystem::is_regular_file(root/"TesseraUI/art"/ *art))return 3;
 }
 std::cout<<report.cards<<" cards loaded through the runtime registry\n";
}
