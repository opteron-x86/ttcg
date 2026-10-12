#include "RendererDX11.h"
#include <d3dcompiler.h>
#include <array>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <wrl/client.h>

namespace preview {
namespace {
void check(HRESULT hr,const char* message) {
    if(FAILED(hr))throw std::runtime_error(std::string(message)+" (HRESULT "+std::to_string(unsigned(hr))+")");
}
class DXRenderer final : public Renderer {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    struct Target { Texture texture; Ptr<ID3D11RenderTargetView> rtv; ImTextureID staticSource=0; int treatment=0; float amount=0; };
    std::array<Target,MaterialSlot::count> targets{};
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    Ptr<ID3D11VertexShader> vs;
    Ptr<ID3D11PixelShader> ps;
    Ptr<ID3D11Buffer> constants;
    Ptr<ID3D11SamplerState> sampler;
    Ptr<ID3D11RasterizerState> raster;
    Ptr<ID3DBlob> compile(const char* entry,const char* target){
        Ptr<ID3DBlob> blob,errors;auto code=hlslShader();
        auto hr=D3DCompile(code.data(),code.size(),"TesseraPrismatic",nullptr,nullptr,entry,target,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,blob.GetAddressOf(),errors.GetAddressOf());
        if(FAILED(hr))throw std::runtime_error(errors?std::string(static_cast<char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"Shader compile failed");
        return blob;
    }
public:
    DXRenderer(ID3D11Device* d,ID3D11DeviceContext* c):device(d),context(c){
        auto vertex=compile("vertex","vs_5_0");check(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,vs.GetAddressOf()),"Create foil vertex shader");
        auto pixel=compile("pixel","ps_5_0");check(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,ps.GetAddressOf()),"Create foil pixel shader");
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=32;desc.Usage=D3D11_USAGE_DYNAMIC;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        check(device->CreateBuffer(&desc,nullptr,constants.GetAddressOf()),"Create foil constants");
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        check(device->CreateSamplerState(&sd,sampler.GetAddressOf()),"Create sampler");
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
        check(device->CreateRasterizerState(&rd,raster.GetAddressOf()),"Create rasterizer");
    }
    ~DXRenderer()override{for(auto& target:targets)release(target.texture);}
    const char* name()const override{return "DirectX 11";}
    Texture upload(const unsigned char* pixels,int w,int h)override{
        D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
        desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|(pixels?0:D3D11_BIND_RENDER_TARGET);
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels;data.SysMemPitch=w*4;
        Ptr<ID3D11Texture2D> texture;check(device->CreateTexture2D(&desc,pixels?&data:nullptr,texture.GetAddressOf()),"Upload artwork");
        Ptr<ID3D11ShaderResourceView> view;check(device->CreateShaderResourceView(texture.Get(),nullptr,view.GetAddressOf()),"Create artwork view");
        return {ImTextureID(reinterpret_cast<std::uintptr_t>(view.Detach())),w,h,std::uint64_t(w)*h*4};
    }
    void release(Texture t)override{if(t.id){for(auto& target:targets)if(target.staticSource==t.id)target.staticSource=0;reinterpret_cast<ID3D11ShaderResourceView*>(std::uintptr_t(t.id))->Release();}}
    Texture material(Texture source,int slot,float x,float y,float strength,int treatment,float amount)override{
        if(!source.id||slot<0||slot>=int(targets.size()))return source;
        auto& t=targets[slot];float ratio=std::min(1.f,(slot>=22?512.f:1024.f)/source.height);
        int w=std::max(1,int(source.width*ratio)),h=std::max(1,int(source.height*ratio));
        if(!t.texture.id||t.texture.width!=w||t.texture.height!=h){
            t.staticSource=0;
            t.rtv.Reset();release(std::exchange(t.texture,{}));t.texture=upload(nullptr,w,h);
            Ptr<ID3D11Resource> resource;reinterpret_cast<ID3D11ShaderResourceView*>(std::uintptr_t(t.texture.id))->GetResource(resource.GetAddressOf());
            check(device->CreateRenderTargetView(resource.Get(),nullptr,t.rtv.GetAddressOf()),"Create foil target");
        }
        // Ordinary cards need one filter pass until their art or treatment changes.
        if(strength==0&&t.staticSource==source.id&&t.treatment==treatment&&t.amount==amount)return t.texture;
        D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(constants.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped),"Map foil constants");
        float values[]{x,y,strength,float(treatment),amount,0,0,0};std::memcpy(mapped.pData,values,sizeof(values));context->Unmap(constants.Get(),0);
        D3D11_VIEWPORT viewport{0,0,float(w),float(h),0,1};context->RSSetViewports(1,&viewport);context->RSSetState(raster.Get());
        context->OMSetRenderTargets(1,t.rtv.GetAddressOf(),nullptr);context->OMSetBlendState(nullptr,nullptr,0xffffffff);context->OMSetDepthStencilState(nullptr,0);
        context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->GSSetShader(nullptr,nullptr,0);
        auto* src=reinterpret_cast<ID3D11ShaderResourceView*>(std::uintptr_t(source.id));context->PSSetShaderResources(0,1,&src);
        context->PSSetSamplers(0,1,sampler.GetAddressOf());context->PSSetConstantBuffers(0,1,constants.GetAddressOf());context->Draw(3,0);
        ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);context->OMSetRenderTargets(0,nullptr,nullptr);
        t.staticSource=strength==0?source.id:0;t.treatment=treatment;t.amount=amount;
        return t.texture;
    }
};
}
std::unique_ptr<Renderer> makeDXRenderer(ID3D11Device* device, ID3D11DeviceContext* context) {
    return std::make_unique<DXRenderer>(device, context);
}
}
