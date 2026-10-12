#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include "App.h"
#include "../tests/GameTestAccess.h"
#include "RendererDX11.h"
#include "DXIsolation.h"
#include <array>
#include <cstring>
#include <iostream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace preview {
namespace {
template<class T>void drop(T*& object){if(object)object->Release();object=nullptr;}
void check(HRESULT hr,const char* message){if(FAILED(hr))throw std::runtime_error(std::string(message)+" (HRESULT "+std::to_string(unsigned(hr))+")");}
unsigned resizeW=0,resizeH=0;
LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
    if(ImGui::GetCurrentContext()&&ImGui_ImplWin32_WndProcHandler(hwnd,message,w,l))return true;
    switch(message){
    case WM_SIZE:if(w!=SIZE_MINIMIZED){resizeW=LOWORD(l);resizeH=HIWORD(l);}return 0;
    case WM_GETMINMAXINFO:{auto* m=reinterpret_cast<MINMAXINFO*>(l);m->ptMinTrackSize.x=1100;m->ptMinTrackSize.y=720;return 0;}
    case WM_SYSCOMMAND:if((w&0xfff0)==SC_KEYMENU)return 0;break;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,w,l);
}
}
int runHost(const Options& o){
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW),CS_CLASSDC,windowProc,0,0,GetModuleHandleW(nullptr),nullptr,nullptr,nullptr,nullptr,L"TesseraNativePreview",nullptr};
    RegisterClassExW(&wc);RECT rect{0,0,o.width,o.height};AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
    HWND window=CreateWindowW(wc.lpszClassName,L"Tessera | Native UI",WS_OVERLAPPEDWINDOW,100,100,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,wc.hInstance,nullptr);
    if(!window)throw std::runtime_error("Could not create preview window");
    DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=window;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;IDXGISwapChain* swap=nullptr;ID3D11RenderTargetView* target=nullptr;
    D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};D3D_FEATURE_LEVEL level;
    HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,1,D3D11_SDK_VERSION,&sd,&swap,&device,&level,&context);
    if(FAILED(hr))hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,1,D3D11_SDK_VERSION,&sd,&swap,&device,&level,&context);
    check(hr,"Create DirectX 11 device");
    auto createTarget=[&](){ID3D11Texture2D* back=nullptr;check(swap->GetBuffer(0,IID_PPV_ARGS(&back)),"Get back buffer");auto status=device->CreateRenderTargetView(back,nullptr,&target);drop(back);check(status,"Create window target");};
    createTarget();if(!o.hidden)ShowWindow(window,SW_SHOWDEFAULT);UpdateWindow(window);resizeW=resizeH=0;
    IMGUI_CHECKVERSION();ImGui::CreateContext();ImGui_ImplWin32_Init(window);ImGui_ImplDX11_Init(device,context);
    {
        auto renderer=makeDXRenderer(device,context);DXIsolation isolation(device,context);GameApp app(*renderer,o);GameTestAccess::load(app,o);bool running=true;int frame=0;
        while(running){
            ULONGLONG start=GetTickCount64();MSG message;
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);if(message.message==WM_QUIT)running=false;}
            if(!running)break;
            if(!o.frames&&IsIconic(window)){Sleep(50);continue;}
            if(resizeW&&resizeH){context->OMSetRenderTargets(0,nullptr,nullptr);drop(target);check(swap->ResizeBuffers(0,resizeW,resizeH,DXGI_FORMAT_UNKNOWN,0),"Resize window");resizeW=resizeH=0;createTarget();}
            context->OMSetRenderTargets(1,&target,nullptr);DXIsolation::Frame isolated(isolation);
            ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();GameTestAccess::captureFrame(app,o);app.draw();ImGui::Render();
            float colour[]{.04f,.05f,.04f,1};context->OMSetRenderTargets(1,&target,nullptr);context->ClearRenderTargetView(target,colour);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            ++frame;
            bool last=(o.frames&&frame>=o.frames);
            if(last&&!o.screenshot.empty()){
                ID3D11Texture2D* back=nullptr;check(swap->GetBuffer(0,IID_PPV_ARGS(&back)),"Read screenshot");D3D11_TEXTURE2D_DESC desc;back->GetDesc(&desc);
                desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
                ID3D11Texture2D* stage=nullptr;check(device->CreateTexture2D(&desc,nullptr,&stage),"Create screenshot staging");context->CopyResource(stage,back);drop(back);
                D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(stage,0,D3D11_MAP_READ,0,&mapped),"Map screenshot");
                std::vector<unsigned char> rgba(std::size_t(desc.Width)*desc.Height*4);
                for(unsigned row=0;row<desc.Height;++row)std::memcpy(rgba.data()+row*desc.Width*4,static_cast<unsigned char*>(mapped.pData)+row*mapped.RowPitch,desc.Width*4);
                context->Unmap(stage,0);drop(stage);saveBMP(o.screenshot,int(desc.Width),int(desc.Height),rgba.data(),false);
            }
            hr=swap->Present(1,0);if(hr==DXGI_ERROR_DEVICE_REMOVED||hr==DXGI_ERROR_DEVICE_RESET)throw std::runtime_error("DirectX device lost; restart the preview");
            if(last)running=false;auto elapsed=GetTickCount64()-start;if(elapsed<16)Sleep(DWORD(16-elapsed));
        }
        std::cout<<"Native frames: "<<frame<<"; missing artwork: "<<app.missingAssets()<<'\n';
    }
    ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();context->ClearState();drop(target);drop(swap);drop(context);drop(device);DestroyWindow(window);UnregisterClassW(wc.lpszClassName,wc.hInstance);return 0;
}
}
