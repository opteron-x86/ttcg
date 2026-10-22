#include "GameApp.h"
#include <cmath>
#include <numbers>
namespace preview {
void GameApp::nativeInspector(){
    if(!inspecting||selected<0||selected>=int(collection.cards.size()))return;
    const bool gallery=inspectionList.size()>1;
    const ImVec2 extent=showTableDetails?ImVec2(700,520+(gallery?44:0))*scale:ImVec2(600,645+(gallery?44:0))*scale;
    ImGui::SetNextWindowPos(frameAt+frameSize*.5f,ImGuiCond_Always,ImVec2(.5f,.5f));ImGui::SetNextWindowSize(extent);
    if(!ImGui::IsPopupOpen("Artwork"))ImGui::OpenPopup("Artwork");
    if(!ImGui::BeginPopupModal("Artwork",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|popupFlags()))return;
    const auto origin=ImGui::GetWindowPos();auto* d=ImGui::GetWindowDrawList();
    surface(origin,extent);if(palette().frame==FrameStyle::Classic)d->AddRect(origin,origin+extent,palette().line);else panelFrame(origin,extent);
    const bool dismiss=closeButton("close-art",origin+ImVec2(extent.x-42*scale,7*scale));
    if(dismiss||ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false)){inspecting=false;ImGui::CloseCurrentPopup();}
    const float h=(showTableDetails?440.f:490.f)*scale,w=h*.75f;
    const ImVec2 artAt=showTableDetails?origin+ImVec2(36,40)*scale:origin+ImVec2((extent.x-w)/2,66*scale);
    const ImVec2 centre=artAt+ImVec2(w,h)*.5f;
    if(!showTableDetails)text(origin+ImVec2(extent.x/2,22*scale),collection.cards[selected].name,26,palette().ink,extent.x-100*scale,true);
    ImGui::SetCursorScreenPos(artAt);ImGui::InvisibleButton("rotate-art",ImVec2(w,h),ImGuiButtonFlags_EnableNav);
    if(ImGui::IsItemHovered())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(0,0)){yaw+=ImGui::GetIO().MouseDelta.x*.009f;pitch+=ImGui::GetIO().MouseDelta.y*.006f;turnTarget=yaw;}
    if(ImGui::IsKeyDown(ImGuiKey_GamepadRStickLeft))turnTarget-=ImGui::GetIO().DeltaTime;
    if(ImGui::IsKeyDown(ImGuiKey_GamepadRStickRight))turnTarget+=ImGui::GetIO().DeltaTime;
    if(ImGui::IsKeyDown(ImGuiKey_GamepadRStickUp))pitch-=ImGui::GetIO().DeltaTime;
    if(ImGui::IsKeyDown(ImGuiKey_GamepadRStickDown))pitch+=ImGui::GetIO().DeltaTime;
    yaw+=(turnTarget-yaw)*std::min(1.f,ImGui::GetIO().DeltaTime*8);
    const bool back=std::cos(yaw)*std::cos(pitch)<0;
    auto texture=back?assets.get(customBack):assets.get(collection.cards[selected].art);
    if(face.foil&&selectedFoil)texture=renderer.material(texture,21,.5f+std::sin(yaw)*.45f,.5f+std::sin(pitch)*.45f,face.foilStrength);
    auto project=[&](float u,float v){float x=(u-.5f)*w,y=(v-.5f)*h,z=-x*std::sin(yaw);x*=std::cos(yaw);const float yy=y*std::cos(pitch)-z*std::sin(pitch);z=y*std::sin(pitch)+z*std::cos(pitch);const float perspective=1500*scale/(1500*scale+z);return centre+ImVec2(x,yy)*perspective;};
    for(int y=0;y<24;++y)for(int x=0;x<18;++x){const float u=x/18.f,v=y/24.f,u1=(x+1)/18.f,v1=(y+1)/24.f;auto uv=[&](float a,float b){return ImVec2(back?1-a:a,b);};d->AddImageQuad(texture.id,project(u,v),project(u1,v),project(u1,v1),project(u,v1),uv(u,v),uv(u1,v),uv(u1,v1),uv(u,v1));}
    const ImVec2 corners[]{project(0,0),project(1,0),project(1,1),project(0,1)};for(int i=0;i<4;++i)d->AddLine(corners[i],corners[(i+1)%4],IM_COL32(184,165,117,150),1.5f*scale);
    if(showTableDetails){
        const float infoWidth=262*scale,metadataHeight=metadata(collection.cards[selected],{},infoWidth-12*scale,false);
        const float infoHeight=std::min(440*scale,metadataHeight+64*scale);
        const auto infoAt=ImVec2(origin.x+402*scale,artAt.y+(h-infoHeight)*.5f);
        ImGui::SetCursorScreenPos(infoAt);ImGui::BeginChild("inspection-details",ImVec2(infoWidth,infoHeight));
        const auto top=ImGui::GetCursorScreenPos();metadata(collection.cards[selected],top,infoWidth-12*scale);
        const auto controls=top+ImVec2(0,metadataHeight+24*scale);
        if(button("flip","Turn over",controls,ImVec2(125,38)*scale))turnTarget+=std::numbers::pi_v<float>;
        if(button("reset-art","Reset",controls+ImVec2(137,0)*scale,ImVec2(95,38)*scale))turnTarget=pitch=0;
        ImGui::EndChild();
    }else{
        const auto controls=origin+ImVec2(extent.x/2-125*scale,580*scale);
        if(button("flip","Turn over",controls,ImVec2(135,38)*scale))turnTarget+=std::numbers::pi_v<float>;
        if(button("reset-art","Reset",controls+ImVec2(149,0)*scale,ImVec2(100,38)*scale))turnTarget=pitch=0;
    }
    const int selectedDisplay=selected+(selectedFoil?collection.cards.size():0);
    const auto found=std::find(inspectionList.begin(),inspectionList.end(),selectedDisplay);
    if(found!=inspectionList.end()&&inspectionList.size()>1){
        const int index=int(found-inspectionList.begin());const auto nav=origin+ImVec2(extent.x/2-115*scale,extent.y-45*scale);
        const bool previous=button("previous-art","←",nav,ImVec2(43,32)*scale,false,index>0);
        const bool next=button("next-art","→",nav+ImVec2(187,0)*scale,ImVec2(43,32)*scale,false,index+1<int(inspectionList.size()));
        text(nav+ImVec2(115*scale,5*scale),std::to_string(index+1)+" / "+std::to_string(inspectionList.size()),16,palette().muted,0,true);
        const int direction=previous||ImGui::IsKeyPressed(ImGuiKey_LeftArrow,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadL1,false)?-1:next||ImGui::IsKeyPressed(ImGuiKey_RightArrow,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadR1,false)?1:0;
        if(direction&&index+direction>=0&&index+direction<int(inspectionList.size())){const int target=inspectionList[index+direction];inspectCard(target,showTableDetails);}
    }
    ImGui::EndPopup();
}
}
