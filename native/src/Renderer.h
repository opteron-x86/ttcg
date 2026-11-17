#pragma once
#include "Colour.h"
#include <imgui.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace preview {
// One render target for each card drawn with a foil or match finish in a frame. Two
// cards must not share a slot within a frame: the second would redraw, or rebuild at
// another size, a target the frame's draw list already refers to.
// Where a card's finished face is kept between frames. Each card on screen needs a slot of
// its own, so every place that draws cards has a range.
struct MaterialSlot {
    static constexpr int single = 0; // The one large card of a page: the inspector and the previews.
    static constexpr int hands = 1, board = 11;
    // From here on a face is kept at half the size; these cards are never drawn large.
    static constexpr int reduced = 22;
    static constexpr int drag = 86, packs = 87;
    static constexpr int demoHands = 110, demoBoard = 120;
    static constexpr int eventPrizes = 135, resultPrizes = 145, foilChoice = 148, tradeHands = 150;
    static constexpr int placing = 160, ghost = 161, swapping = 162;
    static constexpr int appearance = 170;
    // A scrolling grid's cards. The range holds more than any layout shows at once.
    static constexpr int grid = 174, gridCount = 160, count = grid + gridCount, none = -1;
    // The grid's nth visible card. Past the range a card is drawn without a finish.
    static constexpr int gridSlot(int n) { return n >= 0 && n < gridCount ? grid + n : none; }
};
struct Texture {
    ImTextureID id = 0;
    int width = 0, height = 0;
    std::uint64_t bytes = 0;
};
struct Options {
    std::filesystem::path tesseraRoot, previewRoot, screenshot, snapshot;
    std::filesystem::path backdrop; // Standalone review only: a game scene drawn behind the interface.
    std::filesystem::path keyIcons; // Standalone review only: an ImGui Icons folder outside Data.
    std::string page, topic;
    int demoStep = 0;
    bool history = false; // Standalone review only.
    int width = 1600, height = 1000, frames = 0, theme = 0, screen = 0;
    bool hidden = false, inspect = false, hosted = false;
    float yaw = 0;
    double presentationTime = -1; // Deterministic capture in the standalone verification host.
};
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual Texture upload(const unsigned char* pixels, int w, int h) = 0;
    virtual void release(Texture texture) = 0;
    virtual Texture material(Texture source, int slot, float x, float y, float strength, int treatment = 0,
                             float amount = 1) = 0;
    virtual const char* name() const = 0;
};
std::string glFragment();
std::string hlslShader();
int runHost(const Options& options);
void saveBMP(const std::filesystem::path& file, int width, int height, const unsigned char* rgba, bool bottomUp);
}
