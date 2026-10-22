#pragma once
#include "InterfaceTheme.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace preview {
struct SurfaceGradient {float x,y,middle;unsigned a,b,c;};
// Cache each theme’s gradient with dithering for smooth dark tones.
inline std::vector<unsigned char> gradientPixels(const SurfaceGradient& g,int width,int height,bool linear=false,int finish=0) {
    const float rx=std::max(g.x,1-g.x)*std::sqrt(2.f),ry=std::max(g.y,1-g.y)*std::sqrt(2.f);
    std::vector<unsigned char> pixels(std::size_t(width)*height*4);
    std::uint32_t noise=73;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const float u=(x+.5f)/width,v=(y+.5f)/height;
        const float distance=linear?(.75f*u+.25f*v):std::hypot((u-g.x)/rx,(v-g.y)/ry);
        const bool outer=distance>g.middle;
        const float t=std::clamp(outer?(distance-g.middle)/(1-g.middle):distance/g.middle,0.f,1.f);
        noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;
        const float grain=float(noise&65535)/65535.f-.5f;
        const float texture=finish==5?std::sin(u*183+std::sin(v*71))*std::sin(v*137+u*23)*.4f:
            finish==6?std::sin((u+v)*220)*.18f:finish==7?std::sin(v*360)*.18f:0.f;
        for(int channel=0;channel<3;++channel){
            const int shift=(2-channel)*8;
            const float a=((outer?g.b:g.a)>>shift)&255,b=((outer?g.c:g.b)>>shift)&255;
            pixels[(std::size_t(y)*width+x)*4+channel]=static_cast<unsigned char>(std::clamp(std::round(a+(b-a)*t+grain*.95f+texture),0.f,255.f));
        }
        pixels[(std::size_t(y)*width+x)*4+3]=255;
    }
    return pixels;
}
inline std::vector<unsigned char> surfacePixels(int theme,int width,int height) {
    constexpr SurfaceGradient gradients[]{
        {.40f,.15f,.58f,0x2a3731,0x17231e,0x111b17},
        {0,0,.72f,0x25282b,0x111417,0x111417},
        {.60f,0,.80f,0x242222,0x19191a,0x19191a},
        {.50f,.32f,.65f,0xeee1c7,0xe0cfaa,0xc4a574},
        {.20f,.05f,.62f,0x493029,0x2b201c,0x1e1715},
        {.45f,0,.62f,0x302626,0x1b1b1c,0x111213},
        {.10f,0,.68f,0x302d26,0x1b1a17,0x111210},
        {.50f,0,.76f,0x2b3033,0x161a1c,0x0e1113}
    };
    static_assert(std::size(gradients)==ttcg::interfaceThemeCount);
    return gradientPixels(gradients[ttcg::interfaceThemeIndex(theme)],width,height,theme==1,theme);
}
inline std::vector<unsigned char> boardPixels(int surface,int width,int height) {
    constexpr SurfaceGradient gradients[]{
        {.50f,.50f,.50f,0x1d2522,0x1d2522,0x1d2522},
        {.30f,0,.65f,0x343b41,0x20272c,0x1c2328},
        {.50f,.35f,.80f,0x28473b,0x18382c,0x18382c},
        {.50f,.40f,1,0xe3cfa4,0xc4a475,0xc4a475},
        {.35f,.10f,.72f,0x35312e,0x24211f,0x24211f},
        {.40f,.15f,.80f,0x583d2a,0x3d291c,0x3d291c}
    };
    return gradientPixels(gradients[std::clamp(surface,1,6)-1],width,height);
}
}
