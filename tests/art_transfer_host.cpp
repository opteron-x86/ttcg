#include "ArtTransfer.h"
#include "Presentation.h"
#include <iostream>
int main(int argc,char** argv){
 if(argc!=2)return 1;
 const auto root=std::filesystem::path(argv[1]);ttcg::refreshCardArt(root);
 std::string id,path;
 while(std::getline(std::cin,id)&&std::getline(std::cin,path)){
  const auto request=ttcg::parseArtRequest(id+"\n"+path);if(!request)continue;
  const auto image=ttcg::readCardArt(request->path,root);
  std::cout<<"{\"id\":"<<request->id<<",\"data\":"<<ttcg::quote(image.data)<<",\"error\":"<<ttcg::quote(image.error)<<"}"<<std::endl;
 }
}
