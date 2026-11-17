#pragma once
#include "Renderer.h"
#include <unordered_map>

namespace preview {
class Assets {
    struct Entry {
        Texture texture;
        std::uint64_t touched;
    };
    Renderer& renderer;
    std::filesystem::path root;
    std::unordered_map<std::string, Entry> textures;
    std::uint64_t frame = 0, resident = 0;
    Texture load(const std::filesystem::path& file);

public:
    static constexpr std::uint64_t budget = 160ull * 1024 * 1024;
    unsigned missing = 0;
    Assets(Renderer& renderer, const std::filesystem::path& root);
    ~Assets();
    void beginFrame();
    void invalidateBacks();
    Texture get(const std::string& path);
    // Interface art that may be absent; no placeholder is substituted.
    Texture optional(const std::string& path);
    Texture pack(unsigned kind);
    std::size_t count() const { return textures.size(); }
};
}
