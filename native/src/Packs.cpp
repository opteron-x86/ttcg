#include "App.h"

namespace preview {
namespace {
const char* packName(unsigned kind){return kind?"Tournament pack":"Card pack";}
void fitImage(Texture texture,ImVec2 at,ImVec2 size) {
    if(!texture.id)return;
    const float zoom=std::min(size.x/texture.width,size.y/texture.height);
    const ImVec2 image(texture.width*zoom,texture.height*zoom);
    const auto p=at+(size-image)*.5f;
    ImGui::GetWindowDrawList()->AddImage(texture.id,p,p+image);
}
}
void App::packTile(unsigned kind,ImVec2 at,ImVec2 size,bool chosen) {
    auto* draw=ImGui::GetWindowDrawList();
    if(chosen)draw->AddRectFilled(at,at+size,palette().selection,2);
    draw->AddRect(at,at+size,chosen?palette().accent:palette().line,2);
    const float foot=34*scale;
    fitImage(assets.pack(kind),at+ImVec2(7,7)*scale,ImVec2(size.x-14*scale,size.y-foot-12*scale));
    text(at+ImVec2(size.x*.5f,size.y-foot+5*scale),std::string(packName(kind))+"  ×"+std::to_string(collection.packs[kind]),16,palette().ink,0,true);
}
void App::packDetail(ImVec2 at,ImVec2 size) {
    const unsigned kind=static_cast<unsigned>(selectedPack);
    const float h=std::min(size.x*1.31f,size.y-185*scale);
    fitImage(assets.pack(kind),at+ImVec2(12,8)*scale,ImVec2(size.x-24*scale,h));
    float y=at.y+h+35*scale;
    text(ImVec2(at.x+size.x*.5f,y),packName(kind),25,palette().ink,0,true);y+=44*scale;
    text(ImVec2(at.x+size.x*.5f,y),"×"+std::to_string(collection.packs[kind]),19,palette().muted,0,true);y+=48*scale;
    const bool enabled=collection.ready&&collection.writable&&!collection.busy&&collection.packs[kind]>0;
    if(button("open-pack",collection.busy?"Opening...":"Open pack",ImVec2(at.x+24*scale,y),ImVec2(size.x-48*scale,42*scale),true,enabled))
        request({{"action","open-pack"},{"kind",kind}});
}
void App::packModal() {
    if(collection.revealID&&!ImGui::IsPopupOpen("Pack contents"))ImGui::OpenPopup("Pack contents");
    const auto display=ImGui::GetIO().DisplaySize;
    const int count=int(collection.revealed.size()),columns=count>5?4:std::max(1,count),rows=(count+columns-1)/columns;
    const ImVec2 size(std::min(display.x-68*scale,columns*(232+24)*scale+48*scale),std::min(display.y-90*scale,(rows>1?840:610)*scale));
    ImGui::SetNextWindowPos(display*.5f,ImGuiCond_Always,ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(size);
    if(!ImGui::BeginPopupModal("Pack contents",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove))return;
    if(!collection.revealID||!collection.ready){ImGui::CloseCurrentPopup();ImGui::EndPopup();return;}
    const auto at=ImGui::GetWindowPos();
    const float gap=24*scale,w=std::min({232*scale,(size.x-72*scale-(columns-1)*gap)/columns,((size.y-202*scale)/std::max(1,rows)-38*scale)*.75f}),h=w/.75f;
    const float rowWidth=columns*w+(columns-1)*gap;
    text(at+ImVec2(size.x*.5f,28*scale),count==8?"Starter cards":packName(collection.revealKind),30,palette().accent,0,true);
    line(at+ImVec2(30,77)*scale,at+ImVec2(size.x-30*scale,77*scale));
    const float rowY=at.y+105*scale;
    for(int n=0;n<int(collection.revealed.size());++n) {
        const auto id=collection.revealed[n];const int index=collection.index(id);
        if(index<0)continue;
        const auto& c=collection.cards[index];
        const float progress=std::clamp(float((ImGui::GetTime()-revealedAt-n*.085)/.27),0.f,1.f);
        const float fade=progress*progress*(3-2*progress);
        const ImVec2 p(at.x+(size.x-rowWidth)*.5f+(n%columns)*(w+gap),rowY+(n/columns)*(h+52*scale)+(1-fade)*18*scale);
        ImGui::PushID(n);ImGui::SetCursorScreenPos(p);
        if(ImGui::InvisibleButton("revealed-card",ImVec2(w,h),ImGuiButtonFlags_EnableNav)&&progress>=1) {
            inspectCard(index+(ttcg::isFoil(id)?collection.cards.size():0),false);
        }
        auto* draw=ImGui::GetWindowDrawList();const int start=draw->VtxBuffer.Size;
        card(index,p,ImVec2(w,h),ImGui::IsItemHovered()||ImGui::IsItemFocused(),ttcg::isFoil(id)&&face.foil,-1,MaterialSlot::packs+n,true);
        text(p+ImVec2(w*.5f,h+17*scale),c.rarity+(ttcg::isFoil(id)?" · Foil":""),18,palette().muted,0,true);
        for(int i=start;i<draw->VtxBuffer.Size;++i) {
            auto& color=draw->VtxBuffer[i].col;
            color=(color&~IM_COL32_A_MASK)|(unsigned(((color&IM_COL32_A_MASK)>>IM_COL32_A_SHIFT)*fade)<<IM_COL32_A_SHIFT);
        }
        ImGui::PopID();
    }
    const bool back=!inspecting&&(ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false));
    if(button("pack-continue","Continue",at+ImVec2((size.x-180*scale)*.5f,size.y-66*scale),ImVec2(180,42)*scale,true,!collection.busy)||back)
        request({{"action","dismiss-pack"},{"revealID",collection.revealID}});
    inspect(); // Nested modal: Back closes artwork before dismissing the receipt.
    ImGui::EndPopup();
}
}
