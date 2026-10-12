#pragma once
#include "Renderer.h"
#include <d3d11.h>

namespace preview {
// The host owns the device, context and render-thread lifetime.
std::unique_ptr<Renderer> makeDXRenderer(ID3D11Device*, ID3D11DeviceContext*);
}
