#define GL_GLEXT_PROTOTYPES
#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include "App.h"
#include "../tests/GameTestAccess.h"
#include <array>
#include <iostream>

namespace preview {
class GLRenderer final : public Renderer {
    struct Target { Texture texture; GLuint framebuffer=0; ImTextureID staticSource=0; int treatment=0; float amount=0; };
    std::array<Target,MaterialSlot::count> targets{};
    GLuint program=0,vao=0;
    static GLuint compile(GLenum type,const std::string& code) {
        auto shader=glCreateShader(type);const char* source=code.c_str();glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);
        GLint okay=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&okay);
        if(!okay){char error[8192]{};glGetShaderInfoLog(shader,sizeof(error),nullptr,error);glDeleteShader(shader);throw std::runtime_error(error);}return shader;
    }
public:
    GLRenderer(){
        const auto vertex=compile(GL_VERTEX_SHADER,"#version 330 core\nout vec2 uv;void main(){uv=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(uv*2.-1.,0.,1.);}");
        const auto fragment=compile(GL_FRAGMENT_SHADER,glFragment());
        program=glCreateProgram();glAttachShader(program,vertex);glAttachShader(program,fragment);glLinkProgram(program);glDeleteShader(vertex);glDeleteShader(fragment);
        GLint okay=0;glGetProgramiv(program,GL_LINK_STATUS,&okay);if(!okay)throw std::runtime_error("Cannot link foil shader");
        glGenVertexArrays(1,&vao);
    }
    ~GLRenderer() override {for(auto& t:targets){release(t.texture);if(t.framebuffer)glDeleteFramebuffers(1,&t.framebuffer);}glDeleteVertexArrays(1,&vao);glDeleteProgram(program);}
    const char* name()const override{return "OpenGL 3.3";}
    Texture upload(const unsigned char* pixels,int w,int h) override {
        GLuint id=0;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        glBindTexture(GL_TEXTURE_2D,0);return {ImTextureID(id),w,h,std::uint64_t(w)*h*4};
    }
    void release(Texture t)override {if(t.id){for(auto& target:targets)if(target.staticSource==t.id)target.staticSource=0;const GLuint id=GLuint(t.id);glDeleteTextures(1,&id);}}
    Texture material(Texture source,int slot,float x,float y,float strength,int treatment,float amount) override {
        if(!source.id||slot<0||slot>=int(targets.size()))return source;
        auto& t=targets[slot];const float ratio=std::min(1.f,(slot>=22?512.f:1024.f)/source.height);
        const int w=std::max(1,int(source.width*ratio)),h=std::max(1,int(source.height*ratio));
        if(strength==0&&t.staticSource==source.id&&t.treatment==treatment&&t.amount==amount&&t.texture.width==w&&t.texture.height==h)return t.texture;
        if(!t.texture.id||t.texture.width!=w||t.texture.height!=h){
            t.staticSource=0;
            release(t.texture);t.texture=upload(nullptr,w,h);if(!t.framebuffer)glGenFramebuffers(1,&t.framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER,t.framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,GLuint(t.texture.id),0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Foil framebuffer incomplete");
        }else glBindFramebuffer(GL_FRAMEBUFFER,t.framebuffer);
        glViewport(0,0,w,h);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glDisable(GL_DEPTH_TEST);
        glUseProgram(program);glBindVertexArray(vao);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,GLuint(source.id));
        glUniform1i(glGetUniformLocation(program,"source"),0);glUniform2f(glGetUniformLocation(program,"light"),x,y);glUniform1f(glGetUniformLocation(program,"strength"),strength);
        glUniform1f(glGetUniformLocation(program,"treatment"),float(treatment));glUniform1f(glGetUniformLocation(program,"amount"),amount);
        glDrawArrays(GL_TRIANGLES,0,3);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glBindVertexArray(0);glUseProgram(0);glBindTexture(GL_TEXTURE_2D,0);
        t.staticSource=strength==0?source.id:0;t.treatment=treatment;t.amount=amount;
        return t.texture;
    }
};
int runHost(const Options& o) {
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_GAMECONTROLLER)!=0)throw std::runtime_error(SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    auto* window=SDL_CreateWindow("Tessera | Native UI",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,o.width,o.height,
        SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(o.hidden?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN));
    if(!window)throw std::runtime_error(SDL_GetError());
    SDL_SetWindowMinimumSize(window,1100,720);
    auto context=SDL_GL_CreateContext(window);if(!context)throw std::runtime_error(SDL_GetError());
    SDL_GL_MakeCurrent(window,context);SDL_GL_SetSwapInterval(1);
    IMGUI_CHECKVERSION();ImGui::CreateContext();ImGui_ImplSDL2_InitForOpenGL(window,context);ImGui_ImplOpenGL3_Init("#version 330 core");
    {
        GLRenderer renderer;GameApp app(renderer,o);GameTestAccess::load(app,o);bool running=true;int frame=0;
        while(running){
            const auto start=SDL_GetTicks64();SDL_Event event;
            while(SDL_PollEvent(&event)){
                ImGui_ImplSDL2_ProcessEvent(&event);
                if(event.type==SDL_QUIT||(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_CLOSE))running=false;
            }
            if(!running)break;
            if(!o.frames&&(SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED)){SDL_Delay(50);continue;}
            ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();GameTestAccess::captureFrame(app,o);app.draw();ImGui::Render();
            int w,h;SDL_GL_GetDrawableSize(window,&w,&h);glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,w,h);
            glClearColor(.04f,.05f,.04f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            ++frame;
            const bool last=(o.frames&&frame>=o.frames);
            if(last&&!o.screenshot.empty()){
                std::vector<unsigned char> rgba(std::size_t(w)*h*4);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());saveBMP(o.screenshot,w,h,rgba.data(),true);
            }
            SDL_GL_SwapWindow(window);if(last)running=false;
            // Bound the standalone host to 60 fps when a compositor ignores swap interval.
            const auto elapsed=SDL_GetTicks64()-start;if(elapsed<16)SDL_Delay(Uint32(16-elapsed));
        }
        std::cout<<"Native frames: "<<frame<<"; missing artwork: "<<app.missingAssets()<<'\n';
    }
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplSDL2_Shutdown();ImGui::DestroyContext();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
}
