#pragma once
#include "Assets.h"
#include "Model.h"
#include "MatchAppearance.h"
#include "InterfaceTheme.h"
#include <array>
#include <string>
#include <unordered_map>

namespace preview {
struct Palette {
    const char* name;
    ImU32 background, centre, ink, muted, accent, line, selection;
    bool sans;
};
// One texture of a skin and how it is drawn; see TesseraUI/skins/README.md.
struct SkinArt {
    std::string path;
    float margin = 0, inset = 0, cap = .1f, opacity = 1;
    float scale = 1; // Screen pixels per texture pixel at 2160p; 3 for art authored at 720p.
    float pad = 0;   // Thickness of a painted frame, kept clear of the window's contents.
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
    // A skin without panel art draws flat grounds, as the minimalist UI mods do.
    ImU32 ground = 0, dialog = 0;
    bool border = true, ruleSelector = false;
    int corner = 0;        // 0 none, 1 key, 2 dot
    bool ornament = false; // A diamond on the title rule.
    std::unordered_map<std::string, SkinArt> art;
};
struct Face {
    bool classic = false, names = true, groups = false, foil = true;
    int nameplate = 0, lettering = 0, back = 0;
    int frame = 0; // 0 medallions, 1 the earlier borderless face, 2 printed.
    bool legacy() const { return frame == 1; }
    bool printed() const { return frame == 2; }
    float shadowBlur = 2, shadowOffset = 1;
    float shadow = 0.8f, stroke = 0.6f, foilStrength = 0.72f, grain = 0.10f;
};
struct AlbumLayout {
    static constexpr float navigation = 196, gap = 30, preview = 260, deck = 310;
    // No interface text is drawn smaller than this, so it stays readable from a sofa or on a handheld.
    static constexpr float smallestText = 15, smallestCaps = 13.5f;
};
class App {
protected:
    Renderer& renderer;
    Options options;
    Assets assets;
    ImFont *serif = nullptr, *sans = nullptr, *bold = nullptr, *jost = nullptr, *jostBold = nullptr, *lato = nullptr;
    std::array<Skin, ttcg::interfaceThemeCount> skins{};
    bool padPrompts = false;
    int keyPromptStyle = 0; // 0 automatic, 1 drawn in the skin's style, 2 installed icons
    bool hostInputDevice = false;
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
    int deckDensity = 1, cardDensity = 1;
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
    // Set while the inspector draws, so the foil answers the card's tilt and not the clock.
    bool foilLightSet = false;
    // Card art is drawn as this many strips each way; the inspector raises it so a tilted card
    // keeps true perspective instead of bending along one diagonal.
    int artGrid = 1;
    void artImage(Texture texture, ImVec2 a, ImVec2 b, ImVec2 uv0, ImVec2 uv1);
    ImVec2 foilLight{.5f, .5f};
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
    void line(ImVec2 a, ImVec2 b, ImU32 colour = 0);
    void surface(ImVec2 at, ImVec2 size, bool main = false);
    void popupSurface();
    // Textured skins. Source art is authored at three times Skyrim's 720p stage.
    // A rarity's colour for text on the interface ground: bright on dark skins, inked on paper.
    ImU32 rarityInk(std::string_view rarity) const {
        const bool paper = lightGround();
        return rarity == "Rare"        ? (paper ? IM_COL32(22, 98, 128, 255) : IM_COL32(86, 223, 255, 255))
               : rarity == "Epic"      ? (paper ? IM_COL32(104, 44, 150, 255) : IM_COL32(214, 132, 250, 255))
               : rarity == "Legendary" ? (paper ? IM_COL32(128, 86, 8, 255) : IM_COL32(240, 196, 101, 255))
                                       : palette().ink;
    }
    // Paper skins put dark ink on a light ground; fixed light-on-dark details check this.
    bool lightGround() const {
        const ImU32 c = palette().background;
        return (c & 255) * 299 + ((c >> 8) & 255) * 587 + ((c >> 16) & 255) * 114 > 128000;
    }
    const Skin& skin() const { return skins[theme]; }
    void loadSkins();
    float skinUnit() const;
    const SkinArt* skinPiece(const char* name) const;
    Texture skinArt(const char* name);
    // Corners of `margin` texture pixels drawn `k` screen pixels per texture pixel; edges stretch or tile.
    void nineSlice(Texture art, ImVec2 at, ImVec2 size, float margin, float k, bool tile = false,
                   ImU32 tint = IM_COL32_WHITE);
    void skinPanel(ImVec2 at, ImVec2 size, float density = 1, ImU32 tint = IM_COL32_WHITE, const char* piece = "panel");
    // Draws nothing and returns false when the skin has no such texture.
    bool skinBand(const char* name, ImVec2 at, ImVec2 size, ImU32 tint = IM_COL32_WHITE);
    void skinRule(ImVec2 a, ImVec2 b, float strength = 1);
    void skinBackdrop();
    // Card grids scroll a whole row per wheel notch and fade out where more rows lie beyond the edge.
    // Call gridScroll inside the grid's child window; it reports which edges hide rows.
    struct GridEdges {
        bool above = false, below = false;
    };
    GridEdges gridScroll(float rowHeight, bool reset = false);
    // Call last inside the same child window.
    void gridFade(GridEdges edges);
    // Shared presentation effects.
    void softShadow(ImVec2 at, ImVec2 size, float strength = 1, float reach = 10);
    void glow(ImVec2 at, ImVec2 size, ImU32 colour, float strength, float reach = 14);
    void cornerMarks(ImVec2 at, ImVec2 size, ImU32 colour, float length = 12, float offset = 4);
    // Multiplies the alpha of everything drawn since `firstVertex`.
    void fadeVertices(int firstVertex, float amount);
    // Scales everything drawn since `firstVertex` about `centre`.
    void scaleVertices(int firstVertex, ImVec2 centre, ImVec2 factor);
    void trackInputDevice();
    // Keycaps come from the player's ImGui Icons installation; a drawn cap stands in without one.
    float keyPrompt(const char* key, const char* pad, const char* label, ImVec2 at, bool* pressed = nullptr,
                    const char* id = nullptr);
    void textCaps(ImVec2 at, const std::string& value, float size, ImU32 colour, float tracking = 0,
                  bool centre = false);
    float capsWidth(const std::string& value, float size, float tracking = 0) const;
    ImGuiWindowFlags popupFlags() const { return ImGuiWindowFlags_NoBackground; }
    void controlBorder(ImVec2 at, ImVec2 size, ImU32 colour);
    void chosenOutline(ImVec2 at, ImVec2 size);
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
    // Unopened packs, each with its own Open button; returns the height used.
    float packStrip(ImVec2 at, float width);
    // Puts a card in the chosen slot, or the first empty one.
    bool addToDeck(ttcg::CardID id);
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
    void inputDevice(bool controller) {
        hostInputDevice = true;
        padPrompts = controller;
    }
    bool closeRequested() const { return closing; }
    unsigned missingAssets() const { return assets.missing; }
};
}
