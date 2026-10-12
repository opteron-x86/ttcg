#pragma once
#include <imgui.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace preview {
struct MaterialSlot {static constexpr int grid=22,drag=86,packs=87,appearance=170,count=174;};
struct Texture { ImTextureID id=0; int width=0,height=0; std::uint64_t bytes=0; };
struct Options {
 std::filesystem::path tesseraRoot,previewRoot,screenshot,snapshot;
 std::string page,topic;
 int demoStep=0;bool history=false; // Standalone review only.
 int width=1600,height=1000,frames=0,theme=0,screen=0;
 bool hidden=false,inspect=false,hosted=false;
 float yaw=0;
 double presentationTime=-1; // Deterministic capture in the standalone verification host.
};
class Renderer {
public:
 virtual ~Renderer()=default;
 virtual Texture upload(const unsigned char* pixels,int w,int h)=0;
 virtual void release(Texture texture)=0;
 virtual Texture material(Texture source,int slot,float x,float y,float strength,int treatment=0,float amount=1)=0;
 virtual const char* name()const=0;
};
std::string glFragment();
std::string hlslShader();
int runHost(const Options& options);
void saveBMP(const std::filesystem::path& file,int width,int height,const unsigned char* rgba,bool bottomUp);
}
