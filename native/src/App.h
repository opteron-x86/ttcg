#pragma once
#include "Assets.h"
#include "Model.h"
#include "MatchAppearance.h"
#include <array>

namespace preview {
struct Palette {
    const char* name; ImU32 background, centre, ink, muted, accent, line, selection;
    bool sans;
};
struct Face {
    bool classic = false, names = true, foil = true;
    int nameplate = 0, lettering = 0, back = 0;
    float shadowBlur=2, shadowOffset=1;
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
    ImFont *serif = nullptr, *sans = nullptr, *bold = nullptr;
    std::array<Texture,11> surfaces{};
    ImFont *preferredFont=nullptr,*cardNameFont=nullptr,*cardValueFont=nullptr;
    std::string customBack="backs/mosaic.webp";
    Filter filter;
    AlbumView collection;
    bool selectedFoil=false;
    int selectedPack=-1;
    double revealedAt=0;
    bool dragPreviewDrawn=false;
    bool resetPopups=false;
    int deckSlot=-1;
    int deckDensity=1;
    bool renamingDeck=false,focusDeckName=false;
    unsigned renamingDeckID=0;
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
    const Palette& palette() const;
    ImFont* font() const;
    void text(ImVec2 at, const std::string& value, float size, ImU32 colour, float wrap = 0, bool centre = false);
    void textRight(ImVec2 at,const std::string& value,float size,ImU32 colour);
    float choiceWidth(const std::vector<std::string>& choices) const;
    bool select(const char* id,int& value,const std::vector<std::string>& choices,float width);
    bool sectionTab(const char* id,const std::string& title,ImVec2 at,ImVec2 size,bool active);
    bool button(const char* id, const char* label, ImVec2 at, ImVec2 size, bool active = false, bool enabled = true);
    void itemTooltip(const std::string& label);
    bool closeButton(const char* id,ImVec2 at);
    void line(ImVec2 a, ImVec2 b, ImU32 colour = 0);
    void surface(ImVec2 at,ImVec2 size);
    void boardSurface(int material,ImVec2 at,ImVec2 size);
    ImU32 controlColour(int owner,bool forText=false) const;
    void controlFrame(ImVec2 at,ImVec2 size,int owner);
    void card(int index, ImVec2 at, ImVec2 size, bool selected, bool foil, int owner = -1, int materialSlot = 0, bool large = false,CardContext context=CardContext::General);
    void cardGrid(ImVec2 at, ImVec2 size, bool changed=false);
    void dragCard(int index,bool foil,int owner=-1,CardContext context=CardContext::General);
    void packTile(unsigned kind,ImVec2 at,ImVec2 size,bool chosen);
    void packDetail(ImVec2 at,ImVec2 size);
    void packModal();
    void decks(ImVec2 at, ImVec2 size);
    void deckModal();
    void refreshFilter();
    virtual void request(Json command)=0;
    virtual void drawDetail(ImVec2 at,ImVec2 size)=0;
    virtual void pickerFilters(ImVec2 at,float width);
    virtual void inspect()=0;
    virtual void inspectCard(int display,bool metadata=true,int modifier=0)=0;
    void style();
public:
    App(Renderer& renderer, const Options& options);
    virtual ~App();
    virtual void draw()=0;
    void resume() { closing = false; }
    bool closeRequested() const { return closing; }
    unsigned missingAssets() const { return assets.missing; }
};
}
