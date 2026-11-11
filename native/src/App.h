#pragma once
#include "Assets.h"
#include "Model.h"
#include "MatchAppearance.h"
#include "InterfaceTheme.h"
#include <array>
#include <string>
#include <unordered_map>

namespace preview {
enum class FrameStyle { Classic, Skinned };
struct Palette {
    const char* name;
    ImU32 background, centre, ink, muted, accent, line, selection;
    bool sans;
    FrameStyle frame = FrameStyle::Classic;
};
// One texture of a skin and how it is drawn; see TesseraUI/skins/README.md.
struct SkinArt {
    std::string path;
    float margin = 0, inset = 0, cap = .1f, opacity = 1;
    bool tile = false;
    int passes = 1;
};
struct Skin {
    bool loaded = false;
    Palette palette{};
    std::string font = "jost";
    ImU32 value = IM_COL32(126, 196, 112, 255), notice = IM_COL32(200, 72, 58, 255);
    unsigned player = 0, opponent = 0; // Control colours; zero keeps the palette setting.
    int board = 0;                     // Default game board; zero keeps the theme's.
    std::unordered_map<std::string, SkinArt> art;
};
struct Face {
    bool classic = false, names = true, groups = false, foil = true;
    int nameplate = 0, lettering = 0, back = 0;
    int frame = 0; // 0 framed, 1 the earlier borderless face.
    float shadowBlur = 2, shadowOffset = 1;
    float shadow = 0.8f, stroke = 0.6f, foilStrength = 0.72f, grain = 0.10f;
};
struct AlbumLayout {
    static constexpr float navigation = 196, gap = 30, preview = 260;
};
class App {
protected:
    Renderer& renderer;
    Options options;
    Assets assets;
    ImFont *serif = nullptr, *sans = nullptr, *bold = nullptr, *jost = nullptr, *lato = nullptr;
    std::array<Texture, ttcg::interfaceThemeCount> surfaces{};
    std::array<Skin, ttcg::interfaceThemeCount> skins{};
    bool padPrompts = false;
    std::array<Texture, 7> boardSurfaces{};
    ImFont *preferredFont = nullptr, *cardNameFont = nullptr, *cardValueFont = nullptr;
    std::string customBack = "backs/mosaic.webp";
    Filter filter;
    AlbumView collection;
    bool selectedFoil = false;
    int selectedPack = -1;
    double revealedAt = 0;
    bool dragPreviewDrawn = false;
    bool resetPopups = false;
    int deckSlot = -1;
    int deckDensity = 1;
    bool renamingDeck = false, focusDeckName = false;
    unsigned renamingDeckID = 0;
    std::string deckDialog;
    char deckName[129]{};
    std::vector<int> visible;
    char query[128]{};
    int theme = 0, screen = 0, selected = 0, selectedHand = 0;
    float scale = 1;
    Face face;
    MatchAppearance matchAppearance;
    bool inspecting = false, autoRotate = false, showMetrics = false, closing = false;
    float yaw = 0, pitch = 0, turnTarget = 0;
    static const Palette& builtInPalette(int index);
    const Palette& palette() const;
    ImFont* font() const;
    void text(ImVec2 at, const std::string& value, float size, ImU32 colour, float wrap = 0, bool centre = false);
    void textRight(ImVec2 at, const std::string& value, float size, ImU32 colour);
    float choiceWidth(const std::vector<std::string>& choices) const;
    bool select(const char* id, int& value, const std::vector<std::string>& choices, float width);
    bool sectionTab(const char* id, const std::string& title, ImVec2 at, ImVec2 size, bool active);
    bool button(const char* id, const char* label, ImVec2 at, ImVec2 size, bool active = false, bool enabled = true);
    void itemTooltip(const std::string& label);
    bool closeButton(const char* id, ImVec2 at);
    void line(ImVec2 a, ImVec2 b, ImU32 colour = 0);
    void surface(ImVec2 at, ImVec2 size, bool main = false);
    void panelFrame(ImVec2 at, ImVec2 size, bool main = false);
    void popupSurface();
    // Textured skins. Source art is authored at three times Skyrim's 720p stage.
    bool skinned() const { return palette().frame == FrameStyle::Skinned; }
    const Skin& skin() const { return skins[theme]; }
    void loadSkins();
    float skinUnit() const;
    const SkinArt* skinPiece(const char* name) const;
    Texture skinArt(const char* name);
    // Corners of `margin` texture pixels drawn `k` screen pixels per texture pixel; edges stretch or tile.
    void nineSlice(Texture art, ImVec2 at, ImVec2 size, float margin, float k, bool tile = false,
                   ImU32 tint = IM_COL32_WHITE);
    void skinPanel(ImVec2 at, ImVec2 size, float density = 1, ImU32 tint = IM_COL32_WHITE);
    // Draws nothing and returns false when the skin has no such texture.
    bool skinBand(const char* name, ImVec2 at, ImVec2 size, ImU32 tint = IM_COL32_WHITE);
    void skinRule(ImVec2 a, ImVec2 b, float strength = 1);
    void skinBackdrop();
    void trackInputDevice();
    // Keycaps come from the player's ImGui Icons installation; a drawn cap stands in without one.
    float keyPrompt(const char* key, const char* pad, const char* label, ImVec2 at, bool* pressed = nullptr);
    void textCaps(ImVec2 at, const std::string& value, float size, ImU32 colour, float tracking = 0,
                  bool centre = false);
    float capsWidth(const std::string& value, float size, float tracking = 0) const;
    ImGuiWindowFlags popupFlags() const {
        return palette().frame == FrameStyle::Classic ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoBackground;
    }
    void controlBorder(ImVec2 at, ImVec2 size, ImU32 colour);
    void boardSurface(int material, ImVec2 at, ImVec2 size);
    ImU32 controlColour(int owner, bool forText = false) const;
    void controlFrame(ImVec2 at, ImVec2 size, int owner);
    void card(int index, ImVec2 at, ImVec2 size, bool selected, bool foil, int owner = -1, int materialSlot = 0,
              bool large = false, CardContext context = CardContext::General);
    // Centre of a value on a card of this size, shared with the rule demonstrations.
    ImVec2 valueAnchor(int side, ImVec2 size) const;
    void framedCard(const CardView& card, Texture texture, ImVec2 at, ImVec2 size, bool chosen, int owner);
    void cardGrid(ImVec2 at, ImVec2 size, bool changed = false);
    void dragCard(int index, bool foil, int owner = -1, CardContext context = CardContext::General);
    void packTile(unsigned kind, ImVec2 at, ImVec2 size, bool chosen);
    void packDetail(ImVec2 at, ImVec2 size);
    void packModal();
    void decks(ImVec2 at, ImVec2 size);
    void deckModal();
    void refreshFilter();
    virtual void request(Json command) = 0;
    virtual void drawDetail(ImVec2 at, ImVec2 size) = 0;
    virtual void pickerFilters(ImVec2 at, float width);
    virtual void inspect() = 0;
    virtual void inspectCard(int display, bool metadata = true, int modifier = 0) = 0;
    void style();

public:
    App(Renderer& renderer, const Options& options);
    virtual ~App();
    virtual void draw() = 0;
    void resume() { closing = false; }
    bool closeRequested() const { return closing; }
    unsigned missingAssets() const { return assets.missing; }
};
}
