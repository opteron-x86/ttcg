#include "Renderer.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace preview {
void saveBMP(const std::filesystem::path& file, int w, int h, const unsigned char* pixels, bool bottomUp) {
    if (file.empty()) return;
    if (!file.parent_path().empty()) std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file,std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write screenshot: "+file.string());
    const auto word=[&](unsigned value,int bytes){for(int i=0;i<bytes;++i)out.put(char(value>>(i*8)));};
    const unsigned row=(w*3+3)&~3;
    out.write("BM",2);word(54+row*h,4);word(0,4);word(54,4);word(40,4);word(w,4);word(h,4);
    word(1,2);word(24,2);word(0,4);word(row*h,4);word(2835,4);word(2835,4);word(0,4);word(0,4);
    for(int y=0;y<h;++y){const auto* p=pixels+(bottomUp?y:h-1-y)*w*4;
        for(int x=0;x<w;++x){out.put(char(p[x*4+2]));out.put(char(p[x*4+1]));out.put(char(p[x*4]));}
        for(unsigned padding=w*3;padding<row;++padding)out.put(0);
    }
    std::cout<<"Screenshot: "<<file.string()<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        preview::Options o;
        o.tesseraRoot=TESSERA_DEFAULT_ROOT;o.previewRoot=TESSERA_PREVIEW_ROOT;
        // Permit a Windows build to travel with this prototype folder.
        auto executable=std::filesystem::absolute(argv[0]).parent_path();
        for (const auto& candidate : {executable,executable.parent_path(),executable.parent_path().parent_path()})
            if(std::filesystem::exists(candidate/"assets/fonts/LiberationSerif-Regular.ttf"))o.previewRoot=candidate;
        for(int i=1;i<argc;++i){
            std::string a=argv[i];
            auto value=[&](){if(++i>=argc)throw std::runtime_error("Missing argument for "+a);return std::string(argv[i]);};
            if(a=="--snapshot")o.snapshot=value();
            else if(a=="--page")o.page=value();
            else if(a=="--topic")o.topic=value();
            else if(a=="--demo-step")o.demoStep=std::max(0,std::stoi(value()));
            else if(a=="--history")o.history=true;
            else if(a=="--tessera-root")o.tesseraRoot=value();
            else if(a=="--preview-root")o.previewRoot=value();
            else if(a=="--screenshot")o.screenshot=value();
            else if(a=="--frames")o.frames=std::max(1,std::stoi(value()));
            else if(a=="--presentation-time")o.presentationTime=std::stod(value());
            else if(a=="--hidden")o.hidden=true;
            else if(a=="--skyrim-controls")o.hosted=true;
            else if(a=="--inspect")o.inspect=true;
            else if(a=="--size"){auto v=value();auto p=v.find('x');if(p==v.npos)throw std::runtime_error("Use WIDTHxHEIGHT");o.width=std::clamp(std::stoi(v),1100,3840);o.height=std::clamp(std::stoi(v.substr(p+1)),720,2160);}
            else if(a=="--theme"){auto v=value();const char* names[]{"tessera","charcoal","oxblood","parchment","umber"};bool found=false;for(int n=0;n<5;++n)if(v==names[n]){o.theme=n;found=true;}if(!found)throw std::runtime_error("Unknown theme: "+v);}
            else if(a=="--help"){
                std::cout<<"Tessera native UI verification host\n"
                    "  --snapshot FILE.json --page cards|decks|journal|rules|tournaments|appearance|match-appearance|controls|gameplay|developer\n"
                    "  --topic RULE [--demo-step N] --history --inspect --theme tessera|charcoal|oxblood|parchment|umber\n"
                    "  Shop snapshots: --page buy|sell|buyback\n"
                    "  --tessera-root PATH --size WIDTHxHEIGHT\n"
                    "  --frames N --screenshot FILE.bmp [--hidden]\n";
                return 0;
            }else throw std::runtime_error("Unknown option: "+a);
        }
        if(!std::filesystem::exists(o.tesseraRoot/"TesseraUI/art/backs/mosaic.webp"))throw std::runtime_error("Cannot find Tessera artwork; pass --tessera-root PATH");
        if(o.snapshot.empty())o.snapshot=o.tesseraRoot/"build/native-migration/fixtures/album.json";
        return preview::runHost(o);
    }catch(const std::exception& e){std::cerr<<"Native preview: "<<e.what()<<'\n';return 1;}
}
