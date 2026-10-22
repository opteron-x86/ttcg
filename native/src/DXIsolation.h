#pragma once
#include <imgui.h>
#include <d3d11_1.h>
#include <wrl/client.h>
#include <stdexcept>

namespace preview {
// Restore the engine graphics state after the entire frame, including foil passes.
class DXIsolation {
    template<class T> using Ptr = Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11DeviceContext1> context;
    Ptr<ID3DDeviceContextState> local;
public:
    DXIsolation(ID3D11Device* device, ID3D11DeviceContext* immediate) {
        Ptr<ID3D11Device1> extended;
        if(FAILED(device->QueryInterface(IID_PPV_ARGS(extended.GetAddressOf()))) ||
           FAILED(immediate->QueryInterface(IID_PPV_ARGS(context.GetAddressOf()))))
            throw std::runtime_error("DirectX 11.1 context-state isolation is unavailable");
        const auto level = device->GetFeatureLevel();
        const auto flags = (device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED)
            ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED : 0u;
        if(FAILED(extended->CreateDeviceContextState(flags, &level, 1, D3D11_SDK_VERSION,
                __uuidof(ID3D11Device), nullptr, local.GetAddressOf())))
            throw std::runtime_error("Could not create an isolated DirectX context state");
    }
    class Frame {
        DXIsolation& owner;
        Ptr<ID3DDeviceContextState> previous;
        Ptr<ID3D11RenderTargetView> target;
    public:
        explicit Frame(DXIsolation& value) : owner(value) {
            owner.context->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
            if(!target)throw std::runtime_error("Skyrim has no active UI render target");
            owner.context->SwapDeviceContextState(owner.local.Get(), previous.GetAddressOf());
            bindTarget();
        }
        ~Frame() {
            // Release frame resources so Skyrim can resize its buffers.
            owner.context->ClearState();
            owner.context->SwapDeviceContextState(previous.Get(), nullptr);
        }
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;
        void bindTarget() { auto* view = target.Get(); owner.context->OMSetRenderTargets(1, &view, nullptr); }
        ImVec2 targetSize() const {
            Ptr<ID3D11Resource> resource;
            Ptr<ID3D11Texture2D> texture;
            target->GetResource(resource.GetAddressOf());
            if(FAILED(resource.As(&texture)))return {};
            D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
            return {float(desc.Width), float(desc.Height)};
        }
    };
};
}
