#include "DXIsolation.h"
#include "RendererDX11.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <cmath>

using Microsoft::WRL::ComPtr;
void require(bool okay,const char* message) { if(!okay)throw std::runtime_error(message); }
void check(HRESULT hr) { require(SUCCEEDED(hr),"DirectX test setup failed"); }
int main() {
    try {
        ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
        auto status=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,device.GetAddressOf(),nullptr,context.GetAddressOf());
        if(FAILED(status))check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,device.GetAddressOf(),nullptr,context.GetAddressOf()));
        preview::DXIsolation isolation(device.Get(),context.Get());
        auto renderer=preview::makeDXRenderer(device.Get(),context.Get());
        D3D11_TEXTURE2D_DESC desc{};desc.Width=64;desc.Height=64;desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_UNORDERED_ACCESS;
        ComPtr<ID3D11Texture2D> output,compute;
        check(device->CreateTexture2D(&desc,nullptr,output.GetAddressOf()));check(device->CreateTexture2D(&desc,nullptr,compute.GetAddressOf()));
        ComPtr<ID3D11RenderTargetView> target;check(device->CreateRenderTargetView(output.Get(),nullptr,target.GetAddressOf()));
        ComPtr<ID3D11UnorderedAccessView> uav;check(device->CreateUnorderedAccessView(compute.Get(),nullptr,uav.GetAddressOf()));
        std::array<unsigned char,4*4*4> pixels;pixels.fill(180);
        auto art=renderer->upload(pixels.data(),4,4);
        auto* srv=reinterpret_cast<ID3D11ShaderResourceView*>(std::uintptr_t(art.id));
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        ComPtr<ID3D11Buffer> buffer;check(device->CreateBuffer(&bd,nullptr,buffer.GetAddressOf()));
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        ComPtr<ID3D11SamplerState> sampler;check(device->CreateSamplerState(&sd,sampler.GetAddressOf()));
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_WIREFRAME;rd.CullMode=D3D11_CULL_FRONT;rd.ScissorEnable=TRUE;
        ComPtr<ID3D11RasterizerState> raster;check(device->CreateRasterizerState(&rd,raster.GetAddressOf()));
        D3D11_VIEWPORT viewport{5,7,43,47,.2f,.9f};D3D11_RECT rect{3,4,41,42};
        float blend[]{.2f,.3f,.4f,.5f};
        context->OMSetRenderTargets(1,target.GetAddressOf(),nullptr);
        context->OMSetBlendState(nullptr,blend,0x76543210);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        context->RSSetState(raster.Get());context->RSSetViewports(1,&viewport);context->RSSetScissorRects(1,&rect);
        context->PSSetSamplers(0,1,sampler.GetAddressOf());context->PSSetConstantBuffers(0,1,buffer.GetAddressOf());
        context->PSSetShaderResources(0,1,&srv);context->CSSetShaderResources(3,1,&srv);
        context->CSSetUnorderedAccessViews(0,1,uav.GetAddressOf(),nullptr);
        const auto verify=[&] {
            ComPtr<ID3D11RenderTargetView> gotTarget;context->OMGetRenderTargets(1,gotTarget.GetAddressOf(),nullptr);
            require(gotTarget.Get()==target.Get(),"Render target leaked");
            D3D11_PRIMITIVE_TOPOLOGY topology;context->IAGetPrimitiveTopology(&topology);
            require(topology==D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,"Topology leaked");
            UINT count=1;D3D11_VIEWPORT got{};context->RSGetViewports(&count,&got);
            require(got.TopLeftX==5&&got.Width==43&&got.MinDepth==.2f,"Viewport leaked");
            count=1;D3D11_RECT gotRect{};context->RSGetScissorRects(&count,&gotRect);require(gotRect.left==3&&gotRect.right==41,"Scissor leaked");
            ComPtr<ID3D11RasterizerState> gotRaster;context->RSGetState(gotRaster.GetAddressOf());require(gotRaster.Get()==raster.Get(),"Rasterizer leaked");
            ComPtr<ID3D11SamplerState> gotSampler;context->PSGetSamplers(0,1,gotSampler.GetAddressOf());require(gotSampler.Get()==sampler.Get(),"Sampler leaked");
            ComPtr<ID3D11Buffer> gotBuffer;context->PSGetConstantBuffers(0,1,gotBuffer.GetAddressOf());require(gotBuffer.Get()==buffer.Get(),"Constants leaked");
            ComPtr<ID3D11ShaderResourceView> psSource,csSource;
            context->PSGetShaderResources(0,1,psSource.GetAddressOf());context->CSGetShaderResources(3,1,csSource.GetAddressOf());
            require(psSource.Get()==srv&&csSource.Get()==srv,"Shader resources leaked");
            ComPtr<ID3D11UnorderedAccessView> gotUav;context->CSGetUnorderedAccessViews(0,1,gotUav.GetAddressOf());require(gotUav.Get()==uav.Get(),"Compute UAV leaked");
            UINT mask=0;float factor[4]{};ComPtr<ID3D11BlendState> blendState;
            context->OMGetBlendState(blendState.GetAddressOf(),factor,&mask);
            require(mask==0x76543210&&factor[2]==.4f,"Blend state leaked");
        };
        for(int i=0;i<200;++i) {
            { preview::DXIsolation::Frame frame(isolation);renderer->material(art,i%preview::MaterialSlot::count,.2f,.8f,.7f);frame.bindTarget(); }
            verify();
        }
        try { preview::DXIsolation::Frame frame(isolation);renderer->material(art,0,.3f,.6f,1.f);throw 7; } catch(int) {}
        verify();
        // Check the actual HLSL pixels, including replacing a cached filter with
        // another treatment and a foil pass. Alpha must survive every path.
        const std::array<unsigned char,4> colour{90,140,210,173};auto colourArt=renderer->upload(colour.data(),1,1);
        const auto sample=[&](int treatment,float amount,float foil=0.f){
            preview::DXIsolation::Frame frame(isolation);
            const auto result=renderer->material(colourArt,0,.3f,.7f,foil,treatment,amount);
            ComPtr<ID3D11Resource> resource;reinterpret_cast<ID3D11ShaderResourceView*>(std::uintptr_t(result.id))->GetResource(resource.GetAddressOf());
            D3D11_TEXTURE2D_DESC readDesc{};readDesc.Width=readDesc.Height=readDesc.MipLevels=readDesc.ArraySize=1;
            readDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;readDesc.SampleDesc.Count=1;readDesc.Usage=D3D11_USAGE_STAGING;readDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> readback;check(device->CreateTexture2D(&readDesc,nullptr,readback.GetAddressOf()));context->CopyResource(readback.Get(),resource.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
            std::array<unsigned char,4> pixel;std::copy_n(static_cast<unsigned char*>(mapped.pData),4,pixel.begin());context->Unmap(readback.Get(),0);return pixel;
        };
        const auto original=sample(0,1);require(original==colour,"Original artwork was changed");verify();
        const auto grey=sample(2,1);require(grey[0]==grey[1]&&grey[1]==grey[2]&&grey[3]==173,"Grayscale or alpha is wrong");verify();
        require(sample(2,1)==grey,"Cached grayscale changed");verify();
        const auto half=sample(2,.5f),muted=sample(1,1),sepia=sample(3,1),foil=sample(2,1,.72f);
        require(half[0]>colour[0]&&half[2]<colour[2]&&half[2]>half[0],"Filter strength ignored");
        require(muted[2]>muted[0]&&muted[2]<colour[2],"Muted art lost all colour or kept full saturation");
        require(sepia[0]>sepia[1]&&sepia[1]>sepia[2],"Sepia lost its warm tone");
        require(foil[0]==foil[1]&&foil[1]==foil[2]&&foil[3]==173,"Foil bypassed the opponent filter");
        require(sample(2,0)==colour,"Zero-strength filter changed the artwork");
        require(sample(2,1)==grey,"Static cache retained the previous foil pass");verify();
        renderer->release(colourArt);
        context->ClearState();renderer->release(art);
        std::cout<<"PASS: DirectX context restored after 200 foil passes, exception unwinding and artwork filter/cache pixel checks\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
