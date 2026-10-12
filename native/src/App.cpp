#include "App.h"
#include "CardName.h"
#include "Surface.h"
#include <chrono>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <numbers>

namespace preview {
namespace {
constexpr ImU32 rgb(unsigned n, unsigned alpha = 255) { return IM_COL32(n >> 16, (n >> 8) & 255, n & 255, alpha); }
const Palette themes[]{
    {"Tessera",rgb(0x131e19),rgb(0x28372e),rgb(0xe7dec6),rgb(0xafa991),rgb(0xd0b474),rgb(0x586044,130),rgb(0x4c5237),false},
    {"Charcoal",rgb(0x111417),rgb(0x292d31),rgb(0xeeeef0),rgb(0xa6acb1),rgb(0xe2e5e7),rgb(0x666a70,120),rgb(0x41474d),true},
    {"Oxblood",rgb(0x19191a),rgb(0x2c2828),rgb(0xe7e5de),rgb(0xaaa7a0),rgb(0xdedbd2),rgb(0x625955,120),rgb(0x683c35),true},
    {"Parchment",rgb(0xcfb88f),rgb(0xeee1c7),rgb(0x35291f),rgb(0x6b5843),rgb(0x765130),rgb(0x765130,90),rgb(0xc5a475),false},
    {"Umber",rgb(0x1e1715),rgb(0x493029),rgb(0xe9d8bc),rgb(0xb69b81),rgb(0xd1a76e),rgb(0x92704e,115),rgb(0x604633),false}
};
const char* romans[]{"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
ImU32 rarity(const CardView& card) {
    const std::string_view r = card.rarity;
    return r == "Rare" ? rgb(0x56dfff) : r == "Epic" ? rgb(0xd684fa) : r == "Legendary" ? rgb(0xe6bc65) : rgb(0xb7b89e);
}
std::string groups(const CardView& card) {
    std::string out;
    for (unsigned i = 0; i < ttcg::creatureGroups.size(); ++i) if (card.groups & (1ull << i)) {
        if (!out.empty()) out += " / ";
        out += ttcg::creatureGroups[i];
    }
    return out;
}
std::string affinities(const CardView& card) {
    std::string out;
    const char* labels[]{"Strength", "Intelligence", "Willpower", "Agility", "Endurance"};
    for (int i = 0; i < 5; ++i) if (card.affinityMask & (1u << i)) {
        if (!out.empty()) out += " / ";
        out += labels[i];
    }
    return out.empty() ? "No affinity" : out;
}
ImU32 alpha(ImU32 colour, float amount) { return (colour & 0xffffff) | (unsigned(std::clamp(amount,0.f,1.f)*255) << 24); }
}
const Palette& App::palette() const { return themes[theme]; }
ImFont* App::font() const { return preferredFont?preferredFont:palette().sans ? sans : serif; }
App::App(Renderer& r, const Options& o) : renderer(r), options(o), assets(r,o.tesseraRoot) {
    theme = std::clamp(o.theme,0,4); screen = o.screen; inspecting = o.inspect; yaw = o.yaw; turnTarget = yaw;
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    const auto loadFont=[&](const char* file) {
        const auto path=(o.hosted?o.tesseraRoot/"TesseraUI/fonts"/file:o.previewRoot/"assets/fonts"/file).u8string();
        return io.Fonts->AddFontFromFileTTF(reinterpret_cast<const char*>(path.c_str()),24);
    };
    serif = loadFont("LiberationSerif-Regular.ttf");
    sans = loadFont("LiberationSans-Regular.ttf");
    bold = loadFont("LiberationSerif-Bold.ttf");
    if (!serif || !sans || !bold) throw std::runtime_error("Tessera fonts are missing");
    selected = findCard("Assembled Sanitizer");
    visible = filter.results();
}
App::~App() { for(auto texture:surfaces)if(texture.id)renderer.release(texture); }
void App::surface(ImVec2 at,ImVec2 size) {
    auto& texture=surfaces[theme];
    if(!texture.id){const auto pixels=surfacePixels(theme,1024,768);texture=renderer.upload(pixels.data(),1024,768);}
    ImGui::GetWindowDrawList()->AddImage(texture.id,at,at+size);
}
void App::boardSurface(int material,ImVec2 at,ImVec2 size) {
    auto& texture=surfaces[4+std::clamp(material,1,6)];
    if(!texture.id){const auto pixels=boardPixels(material,512,512);texture=renderer.upload(pixels.data(),512,512);}
    ImGui::GetWindowDrawList()->AddImage(texture.id,at,at+size);
}
void App::text(ImVec2 at, const std::string& value, float size, ImU32 colour, float wrap, bool centre) {
    const float pixels = size*scale;
    if (centre) at.x -= font()->CalcTextSizeA(pixels,1e5f,wrap,value.c_str()).x*.5f;
    ImGui::GetWindowDrawList()->AddText(font(),pixels,at,colour,value.c_str(),nullptr,wrap);
}
void App::textRight(ImVec2 at,const std::string& value,float size,ImU32 colour) {
    text(at-ImVec2(font()->CalcTextSizeA(size*scale,1e5f,0,value.c_str()).x,0),value,size,colour);
}
float App::choiceWidth(const std::vector<std::string>& choices) const {
    float width=0;for(const auto& choice:choices)width=std::max(width,ImGui::CalcTextSize(choice.c_str()).x);
    return width+2*ImGui::GetStyle().FramePadding.x+ImGui::GetFrameHeight();
}
bool App::select(const char* id,int& value,const std::vector<std::string>& choices,float width) {
    if(choices.empty())return false;
    ImGui::SetNextItemWidth(width);
    // Popups need their own padding: the absolute-layout root has none.
    const ImVec2 padding=ImVec2(12,9)*scale;
    float longest=0;for(const auto& choice:choices)longest=std::max(longest,ImGui::CalcTextSize(choice.c_str()).x);
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::max(width,longest+2*padding.x+ImGui::GetStyle().ScrollbarSize),0),ImVec2(FLT_MAX,std::min(ImGui::GetIO().DisplaySize.y*.75f,14*(ImGui::GetTextLineHeight()+14*scale)+2*padding.y)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,padding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,ImVec2(0,.5f));
    bool changed=false;const std::string widgetID=std::string_view(id).starts_with("##")?id:std::string("##")+id;
    if(ImGui::BeginCombo(widgetID.c_str(),choices[std::clamp(value,0,int(choices.size())-1)].c_str(),ImGuiComboFlags_HeightLarge)){
        for(int i=0;i<int(choices.size());++i){
            if(ImGui::Selectable(choices[i].c_str(),value==i,0,ImVec2(0,ImGui::GetTextLineHeight()+14*scale))){value=i;changed=true;}
            if(value==i)ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar(3);return changed;
}
bool App::sectionTab(const char* id,const std::string& title,ImVec2 at,ImVec2 size,bool active){
    ImGui::SetCursorScreenPos(at);const bool hit=ImGui::InvisibleButton(id,size,ImGuiButtonFlags_EnableNav);
    auto* d=ImGui::GetWindowDrawList();
    if(ImGui::IsItemHovered()||ImGui::IsItemFocused())d->AddRectFilled(at,at+size,alpha(palette().selection,.20f));
    if(active&&(theme==2||theme==4)){
        const auto colour=theme==2?alpha(palette().selection,.40f):rgb(0xa56c30,32);
        const auto top=at+ImVec2(0,size.y*.2f);
        d->AddRectFilledMultiColor(top,at+size,colour&0xffffff,colour&0xffffff,colour,colour);
    }
    const float pixels=palette().sans?18:24;const auto measure=font()->CalcTextSizeA(pixels*scale,1e5f,0,title.c_str());
    text(at+(size-measure)*.5f,title,pixels,active?palette().accent:palette().muted);
    if(active){
        const auto colour=theme==0?rgb(0xe2c58a):theme==2?rgb(0xdfd6c5):palette().accent;
        const auto edge=alpha(colour,theme==0?.25f:0.f);const float centre=at.x+size.x*.5f,y=at.y+size.y-(theme==2?1.f:2.f)*scale;
        d->AddRectFilledMultiColor(ImVec2(at.x,y),ImVec2(centre,at.y+size.y),edge,colour,colour,edge);
        d->AddRectFilledMultiColor(ImVec2(centre,y),at+size,colour,edge,edge,colour);
    }
    return hit;
}
void App::line(ImVec2 a, ImVec2 b, ImU32 colour) { ImGui::GetWindowDrawList()->AddLine(a,b,colour?colour:palette().line,1); }
void App::itemTooltip(const std::string& label) {
    if(!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)&&!(ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible))return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(10,8)*scale);
    ImGui::SetTooltip("%s",label.c_str());
    ImGui::PopStyleVar();
}
bool App::closeButton(const char* id,ImVec2 at) {
    ImGui::SetCursorScreenPos(at);
    const bool pressed=ImGui::InvisibleButton(id,ImVec2(34,34)*scale,ImGuiButtonFlags_EnableNav);
    const auto colour=ImGui::IsItemHovered()||ImGui::IsItemFocused()?palette().accent:palette().ink;
    for(int side:{-1,1})ImGui::GetWindowDrawList()->AddLine(at+ImVec2(10,17-7*side)*scale,at+ImVec2(24,17+7*side)*scale,colour,1.5f*scale);
    return pressed;
}
bool App::button(const char* id, const char* label, ImVec2 at, ImVec2 size, bool active, bool enabled) {
    ImGui::SetCursorScreenPos(at);
    ImGui::BeginDisabled(!enabled);
    const bool pressed = ImGui::InvisibleButton(id,size,ImGuiButtonFlags_EnableNav);
    const bool hover = ImGui::IsItemHovered(), focused = ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
    ImGui::EndDisabled();
    auto* draw = ImGui::GetWindowDrawList();
    if (active || hover || focused) draw->AddRectFilled(at,at+size,alpha(palette().selection,active?.82f:.48f),2);
    draw->AddRect(at,at+size,active?palette().accent:palette().line,2);
    if (focused) draw->AddRect(at-ImVec2(3,3),at+size+ImVec2(3,3),palette().accent,3);
    const ImVec2 measure=font()->CalcTextSizeA(19*scale,1e5f,0,label);
    text(at+ImVec2((size.x-measure.x)*.5f,(size.y-measure.y)*.5f),label,19,enabled?palette().ink:alpha(palette().muted,.5f));
    return pressed && enabled;
}
void App::style() {
    auto& s = ImGui::GetStyle(); const auto& p = palette();
    s.WindowPadding = ImVec2(0,0); s.FramePadding = ImVec2(12*scale,10*scale);
    s.ItemSpacing = ImVec2(12*scale,12*scale); s.FrameRounding = 2; s.FrameBorderSize = 1;
    s.ScrollbarSize = 8*scale; s.ScrollbarRounding = 2; s.ChildBorderSize = 0; s.PopupRounding = 2;
    s.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(p.ink);
    s.Colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(p.muted);
    s.Colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(p.background);
    s.Colors[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(p.selection);
    s.Colors[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(p.selection);
    s.Colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(p.line);
    s.Colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(p.selection);
    s.Colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(p.selection);
    s.Colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(p.selection);
    s.Colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(p.background);
    s.Colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(p.selection);
    s.Colors[ImGuiCol_ButtonActive] = ImGui::ColorConvertU32ToFloat4(p.selection);
    s.Colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(p.background);
    s.Colors[ImGuiCol_DragDropTarget] = ImGui::ColorConvertU32ToFloat4(p.accent);
    s.Colors[ImGuiCol_NavCursor] = ImGui::ColorConvertU32ToFloat4(p.accent);
    s.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(.012f,.019f,.015f,.76f);
    s.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0,0,0,0);
    s.Colors[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(alpha(p.accent,.65f));
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(p.accent);
    s.Colors[ImGuiCol_ScrollbarGrabActive] = ImGui::ColorConvertU32ToFloat4(p.accent);
    s.Colors[ImGuiCol_SliderGrab] = ImGui::ColorConvertU32ToFloat4(p.accent);
    s.Colors[ImGuiCol_SliderGrabActive] = ImGui::ColorConvertU32ToFloat4(p.accent);
}
ImU32 App::controlColour(int owner,bool forText) const {
    return rgb(matchAppearance.colour(owner,forText&&theme==3));
}
void App::controlFrame(ImVec2 at,ImVec2 size,int owner) {
    if(owner<0)return;
    auto* d=ImGui::GetWindowDrawList();const auto& look=matchAppearance;
    const ImU32 colour=alpha(controlColour(owner),look.borderOpacity);
    const float weight=look.borderWidth*scale;
    if(weight>0&&look.borderOpacity>0){
        if(look.borderStyle==0)d->AddRect(at,at+size,colour,1,0,weight);
        else if(look.borderStyle==1){
            d->AddRect(at,at+size,colour,1,0,weight);
            const float inset=weight+2.5f*scale;d->AddRect(at+ImVec2(inset,inset),at+size-ImVec2(inset,inset),colour,1,0,std::max(.7f*scale,weight*.65f));
        }else if(look.borderStyle==2){
            const float cut=std::min(size.x*.09f,12*scale);
            const ImVec2 points[]{at+ImVec2(cut,0),at+ImVec2(size.x-cut,0),at+ImVec2(size.x,cut),at+ImVec2(size.x,size.y-cut),at+ImVec2(size.x-cut,size.y),at+ImVec2(cut,size.y),at+ImVec2(0,size.y-cut),at+ImVec2(0,cut)};
            d->AddPolyline(points,8,colour,ImDrawFlags_Closed,weight);
        }else{
            const float arm=std::min(size.x*.22f,32*scale);
            for(int x=0;x<2;++x)for(int y=0;y<2;++y){const auto corner=at+ImVec2(x*size.x,y*size.y);const ImVec2 points[]{corner+ImVec2((x?-1:1)*arm,0),corner,corner+ImVec2(0,(y?-1:1)*arm)};d->AddPolyline(points,3,colour,0,weight);}
        }
    }
    if(look.marker){
        const float unit=size.x/180.f,r=4.8f*unit;const auto p=at+ImVec2(12*unit,size.y-12*unit);
        d->AddCircleFilled(p,8*unit,IM_COL32(12,18,18,230),20);
        const ImVec2 diamond[]{p+ImVec2(0,-r),p+ImVec2(r,0),p+ImVec2(0,r),p+ImVec2(-r,0)};
        if(owner==0)d->AddConvexPolyFilled(diamond,4,controlColour(owner));
        else d->AddPolyline(diamond,4,controlColour(owner),ImDrawFlags_Closed,1.4f*unit);
    }
}
void App::card(int index, ImVec2 at, ImVec2 size, bool chosen, bool foil, int owner, int slot, bool large,CardContext context) {
    const auto& c = collection.cards[index]; auto* d = ImGui::GetWindowDrawList();
    auto texture = assets.get(large?c.art:c.thumbnail);
    const bool treated=matchAppearance.treats(owner,context);
    if ((foil||treated) && texture.id) {
        const float t = float(ImGui::GetTime());
        texture = renderer.material(texture,slot,.5f+std::sin(t*.32f)*.36f,.5f+std::cos(t*.21f)*.27f,foil?face.foilStrength:0,treated?matchAppearance.treatment:0,matchAppearance.strength);
    }
    d->AddRectFilled(at+ImVec2(3,5),at+size+ImVec2(3,5),rgb(0x000000,55),2);
    d->AddRectFilled(at,at+size,rgb(0x14201c),2);
    if (texture.id) {
        const float ratio=float(texture.width)/texture.height,target=size.x/size.y;
        ImVec2 uv0(0,0),uv1(1,1);
        if(ratio>target){const float crop=target/ratio;uv0.x=(1-crop)*.5f;uv1.x=uv0.x+crop;}
        else {const float crop=ratio/target;uv0.y=(1-crop)*.4f;uv1.y=uv0.y+crop;}
        d->AddImage(texture.id,at,at+size,uv0,uv1);
    }
    if(face.names&&face.nameplate!=2)drawCardShade(d,at,size);
    if(owner>=0)controlFrame(at,size,owner);else d->AddRect(at,at+size,rgb(0x6f979a),1,0,1.2f*scale);
    if (chosen) d->AddRect(at-ImVec2(5,5)*scale,at+size+ImVec2(5,5)*scale,palette().accent,5,0,2*scale);
    const float unit = size.x/180.f;
    const float digitSize = 29*unit;
    const auto number = [&](int side, ImVec2 pos) {
        std::string value=c.sides[side]==10?"A":std::to_string(c.sides[side]);
        auto* f=cardValueFont?cardValueFont:bold;
        ImVec2 extent=f->CalcTextSizeA(digitSize,1e5f,0,value.c_str());pos-=extent*.5f;
        if(face.shadow>0){
            const ImVec2 shadow=pos+ImVec2(0,face.shadowOffset*unit);const float radius=face.shadowBlur*unit;
            if(radius>.05f)for(int i=0;i<12;++i){const float angle=i*std::numbers::pi_v<float>/6;d->AddText(f,digitSize,shadow+ImVec2(std::cos(angle),std::sin(angle))*radius*.65f,rgb(0x000000,unsigned(face.shadow*30)),value.c_str());}
            d->AddText(f,digitSize,shadow,rgb(0x000000,unsigned(face.shadow*(radius>.05f?165:255))),value.c_str());
        }
        if(face.stroke>0) for(int i=0;i<8;++i){float a=float(i)*std::numbers::pi_v<float>/4;d->AddText(f,digitSize,pos+ImVec2(std::cos(a),std::sin(a))*face.stroke*unit,rgb(0x101815,210),value.c_str());}
        d->AddText(f,digitSize,pos,rgb(0xf4eedb),value.c_str());
    };
    if(face.classic){
        number(0,at+ImVec2(33,20)*unit);number(1,at+ImVec2(49,39)*unit);
        number(2,at+ImVec2(33,58)*unit);number(3,at+ImVec2(17,39)*unit);
    }else{
        number(0,at+ImVec2(size.x*.5f,20*unit));number(1,at+ImVec2(size.x-15*unit,size.y*(face.names?.42f:.5f)));
        number(2,at+ImVec2(size.x*.5f,size.y*(face.names?.65f:.92f)));number(3,at+ImVec2(15*unit,size.y*(face.names?.42f:.5f)));
    }
    const ImVec2 badge=at+ImVec2(face.classic?size.x*.55f:17*unit,19*unit);
    const auto pigment=c.rarity=="Rare"?std::array<unsigned,2>{0x08718c,0x062e3c}:c.rarity=="Epic"?std::array<unsigned,2>{0x612480,0x260c38}:c.rarity=="Legendary"?std::array<unsigned,2>{0x826024,0x362506}:std::array<unsigned,2>{0x515744,0x1f2921};
    d->AddCircleFilled(badge,11*unit,rgb(0x0c1512,230));d->AddCircleFilled(badge,9.5f*unit,rgb(pigment[0]));
    d->AddCircleFilled(badge+ImVec2(0,2)*unit,7.5f*unit,rgb(pigment[1]));d->AddCircle(badge,10*unit,rarity(c),32,1.1f*unit);d->AddCircle(badge,7.8f*unit,alpha(rarity(c),.55f),32,.55f*unit);
    auto tier=serif->CalcTextSizeA(10*unit,1e5f,0,romans[c.tier]);d->AddText(serif,10*unit,badge-tier*.5f,rarity(c),romans[c.tier]);
    int offset=0;
    for(int i=0;i<5;++i)if(c.affinityMask&(1u<<i)){
        const ImVec2 p=at+ImVec2(size.x-(22+offset*18)*unit,10*unit);
        auto icon=assets.get("../icons/affinity-"+std::to_string(i)+".png");
        d->AddImage(icon.id,p,p+ImVec2(17,17)*unit);++offset;
    }
    if(face.names)drawCardName(d,cardNameFont?cardNameFont:serif,at,size,c.name,face.nameplate,face.lettering,scale);
}
}
