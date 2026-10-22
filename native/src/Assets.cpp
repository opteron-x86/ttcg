#include "Assets.h"
#include "Cards.h"
#include <webp/decode.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include "../third_party/stb/stb_image.h"

namespace preview {
Assets::Assets(Renderer& r, const std::filesystem::path& path) : renderer(r), root(path / "TesseraUI/art") {}
Assets::~Assets() { for (const auto& [key, entry] : textures) renderer.release(entry.texture); }
void Assets::beginFrame() {
    ++frame;
    // Never destroy a texture referenced by this frame's queued draw commands.
    while (resident > budget) {
        auto oldest = textures.end();
        for (auto it = textures.begin(); it != textures.end(); ++it)
            if (it->second.touched + 1 < frame &&
                (oldest == textures.end() || it->second.touched < oldest->second.touched)) oldest = it;
        if (oldest == textures.end()) break;
        resident -= oldest->second.texture.bytes;
        renderer.release(oldest->second.texture);
        textures.erase(oldest);
    }
}
void Assets::invalidateBacks() {
    for(auto it=textures.begin();it!=textures.end();) {
        if(it->first.starts_with("backs/")) {resident-=it->second.texture.bytes;renderer.release(it->second.texture);it=textures.erase(it);}else ++it;
    }
}
Texture Assets::load(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary|std::ios::ate);
    if (!stream) return {};
    const auto length=stream.tellg();
    if(length<=0||length>64*1024*1024)return {};
    stream.seekg(0);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(stream)), {});
    int w = 0, h = 0;
    if(!WebPGetInfo(data.data(),data.size(),&w,&h)) {
        int channels=0;
        if(!stbi_info_from_memory(data.data(),int(data.size()),&w,&h,&channels)||w<=0||h<=0||w>8192||h>8192)return {};
        std::unique_ptr<unsigned char,decltype(&stbi_image_free)> rgba(stbi_load_from_memory(data.data(),int(data.size()),&w,&h,&channels,4),stbi_image_free);
        if(!rgba)return {};
        auto texture=renderer.upload(rgba.get(),w,h);++decoded;return texture;
    }
    if(w<=0||h<=0||w>8192||h>8192)return {};
    std::unique_ptr<unsigned char, decltype(&WebPFree)> rgba(WebPDecodeRGBA(data.data(), data.size(), &w, &h),WebPFree);
    if (!rgba) return {};
    auto texture = renderer.upload(rgba.get(), w, h);
    ++decoded;
    return texture;
}
Texture Assets::get(const std::string& path) {
    auto it = textures.find(path);
    if (it != textures.end()) { it->second.touched = frame; return it->second.texture; }
    auto texture = load((root / std::filesystem::path(std::u8string(path.begin(), path.end()))).lexically_normal());
    if (!texture.id) {
        ++missing;
        std::cerr << "Artwork unavailable: " << path << '\n';
        if (path != "backs/mosaic.webp") texture = load(root / "backs/mosaic.webp");
    }
    resident += texture.bytes;
    textures.emplace(path, Entry{texture, frame});
    return texture;
}
Texture Assets::card(int index, bool large) {
    std::string path = ttcg::cards[index].art;
    if (!large && path.starts_with("cards/")) path.replace(0, 6, "thumbs/");
    return get(path);
}
Texture Assets::back(int index) {
    static const char* names[]{"mosaic", "dragon", "celestial"};
    return get(std::string("backs/") + names[std::clamp(index, 0, 2)] + ".webp");
}
Texture Assets::pack(unsigned kind) {
    // Pack images are included in Main.
    return get(kind?"../assets/packs/tournament.png":"../assets/packs/standard.png");
}
}
