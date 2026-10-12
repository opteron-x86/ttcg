#include "GameApp.h"
namespace preview {
void GameApp::rules(ImVec2 at,ImVec2 size){
    const float nav=AlbumLayout::navigation*scale,gap=AlbumLayout::gap*scale;line(at+ImVec2(nav+gap*.5f,0),at+ImVec2(nav+gap*.5f,size.y));
    ImGui::SetCursorScreenPos(at);ImGui::BeginChild("rule-chapters",ImVec2(nav,size.y));const float navWidth=nav-20*scale;
    for(const auto& page:arrayMember(reference,"pages")){const auto id=label(page,"id");const auto pos=ImGui::GetCursorScreenPos();if(chapter(id,label(page,"title"),pos,ImVec2(navWidth,43*scale),topic==id)){topic=id;demoStep=0;demoSelected=false;selectedHand=-1;demoChanged=ImGui::GetTime();}}ImGui::EndChild();
    const auto origin=at+ImVec2(nav+gap,0);
    ImGui::SetCursorScreenPos(origin);ImGui::BeginChild("rule-content",ImVec2(size.x-nav-gap,size.y));const float width=ImGui::GetContentRegionAvail().x-12*scale;
    struct EndChild { ~EndChild(){ImGui::EndChild();} } endChild;
    const Json* page=nullptr;for(const auto& p:arrayMember(reference,"pages"))if(label(p,"id")==topic)page=&p;if(!page)return;
    ImGui::SetCursorScreenPos(origin);textBlock(label(*page,"title"),width,32,palette().accent);ImGui::Dummy(ImVec2(0,15*scale));textBlock(label(*page,"copy"),width,18,palette().muted);ImGui::Dummy(ImVec2(0,18*scale));
    const auto top=ImGui::GetCursorScreenPos();
    if(topic=="trade"){
        ImGui::SetCursorScreenPos(top);ImGui::BeginChild("trade-reference",ImVec2(width,at.y+size.y-top.y));
        for(const auto& trade:arrayMember(reference,"trades")){const auto pos=ImGui::GetCursorScreenPos();textBlock(label(trade,"title"),width,24,palette().accent);ImGui::Dummy(ImVec2(0,8*scale));textBlock(label(trade,"copy"),width-20*scale,19,palette().ink);ImGui::Dummy(ImVec2(0,24*scale));line(ImGui::GetCursorScreenPos(),ImGui::GetCursorScreenPos()+ImVec2(width,0));ImGui::Dummy(ImVec2(0,15*scale));}ImGui::EndChild();return;
    }
    const auto& demos=member(reference,"demos");if(!demos.contains(topic)||!demos[topic].is_array()||demos[topic].empty())return;const auto& frames=demos[topic];demoStep=std::clamp(demoStep,0,int(frames.size())-1);const auto& frame=frames[demoStep];
    const bool still=topic=="open"||topic=="threeopen";const float available=at.y+size.y-top.y-(still?20:104)*scale;
    table(frame,top,ImVec2(width,available),true);
    if(still)return;
    auto footer=ImVec2(origin.x,top.y+available+10*scale);std::string caption=label(frame,"caption");
    if(demoSelected&&number(frame,"square",-1)>=0)caption="Place the card on the highlighted square.";
    text(footer+ImVec2(width/2,0),caption,18,palette().ink,width,true);
    footer.y=at.y+size.y-38*scale;
    const auto controls=footer+ImVec2((width-445*scale)*.5f,0);
    if(button("demo-back","← Back",controls,ImVec2(98,36)*scale,false,demoStep>0)){--demoStep;demoSelected=false;selectedHand=-1;}
    text(controls+ImVec2(142,9.5f)*scale,std::to_string(demoStep+1)+" / "+std::to_string(frames.size()),17,palette().muted,0,true);
    if(button("demo-next","Next →",controls+ImVec2(186,0)*scale,ImVec2(104,36)*scale,false,demoStep+1<int(frames.size())&&number(frame,"hand",-1)<0)){++demoStep;demoSelected=false;selectedHand=-1;}
    if(button("demo-replay","Start again",controls+ImVec2(320,0)*scale,ImVec2(125,36)*scale)){demoStep=0;demoSelected=false;selectedHand=-1;}
}
void GameApp::rulesModal(){
    if(!showRuleReference)return;if(!ImGui::IsPopupOpen("Rules"))ImGui::OpenPopup("Rules");
    const auto display=ImGui::GetIO().DisplaySize;const ImVec2 extent=ImVec2(std::min(display.x-35*scale,1260*scale),std::min(display.y-35*scale,880*scale));
    ImGui::SetNextWindowPos(display*.5f,ImGuiCond_Always,ImVec2(.5f,.5f));ImGui::SetNextWindowSize(extent);
    if(ImGui::BeginPopupModal("Rules",nullptr,ImGuiWindowFlags_NoDecoration)){
        const auto at=ImGui::GetWindowPos()+ImVec2(25,22)*scale;text(at,"Rules",25,palette().accent);
        if(button("close-rules","×",at+ImVec2(extent.x-85*scale,-6*scale),ImVec2(35,36)*scale)||ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false)){showRuleReference=false;ImGui::CloseCurrentPopup();}
        line(at+ImVec2(0,41*scale),at+ImVec2(extent.x-50*scale,41*scale));rules(at+ImVec2(0,61)*scale,extent-ImVec2(50,110)*scale);ImGui::EndPopup();
    }
}
void GameApp::lessonPanel(ImVec2 at,ImVec2 size){
    const auto& frame=member(state,"lesson");text(at,"How to play",28,palette().accent);ruleChips(at+ImVec2(220,0)*scale,size.x-220*scale,number(frame,"rules"));
    table(frame,at+ImVec2(0,50)*scale,size-ImVec2(0,160)*scale,true,true);
    std::string caption=label(frame,"caption");
    if(caption.empty())for(const char* source:{"demos","guided"})for(const auto& frames:member(reference,source)){
        for(const auto& f:frames)if(number(f,"step")==number(frame,"step")&&
            flag(f,"riftTour")==flag(frame,"riftTour")&&flag(f,"affinityTour")==flag(frame,"affinityTour")&&
            flag(f,"sameLesson")==flag(frame,"sameLesson")&&flag(f,"plusLesson")==flag(frame,"plusLesson")&&
            f.value("workshopRule",0u)==frame.value("workshopRule",0u))caption=label(f,"caption");
    }
    if(demoSelected&&number(frame,"hand",-1)>=0)caption="Place the card on the highlighted square.";
    text(at+ImVec2(size.x/2,size.y-92*scale),caption,20,palette().ink,size.x,true);
    const auto buttons=at+ImVec2(size.x/2-180*scale,size.y-39*scale);
    if(button("replay-lesson","Start again",buttons,ImVec2(140,38)*scale))send("lesson","replay");
    const bool ended=number(frame,"step")>=number(frame,"total");if(ended){if(button("lesson-play","Play",buttons+ImVec2(155,0)*scale,ImVec2(170,38)*scale,true))send("lesson","play");}
    else if(number(frame,"hand",-1)<0){if(button("lesson-next","Next →",buttons+ImVec2(155,0)*scale,ImVec2(170,38)*scale,true))send("lesson","next");}
}
}
