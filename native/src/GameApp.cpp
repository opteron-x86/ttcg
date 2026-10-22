#include "GameApp.h"
#include "CardName.h"
#include <fstream>
#include <cmath>
#include <numbers>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif
namespace preview {
GameApp::GameApp(Renderer& r,const Options& options):App(r,options){
    std::ifstream file(options.tesseraRoot/"TesseraUI/rules.json");if(!file||!(file>>reference))throw std::runtime_error("TesseraUI/rules.json is missing");
    interfaceFonts={nullptr,serif,serif,sans,sans};nameFonts=interfaceFonts;valueFonts={nullptr,bold,bold,sans,sans};
#ifdef _WIN32
    wchar_t windows[32768]{};const auto n=GetWindowsDirectoryW(windows,32768);
    if(n&&n<32768){const auto fonts=std::filesystem::path(windows)/"Fonts";
        const char* regular[]{"","georgia.ttf","times.ttf","arial.ttf","verdana.ttf"};
        const char* strong[]{"","georgiab.ttf","timesbd.ttf","arialbd.ttf","verdanab.ttf"};
        for(unsigned i=1;i<5;++i){auto p=fonts/regular[i];if(std::filesystem::is_regular_file(p))interfaceFonts[i]=nameFonts[i]=ImGui::GetIO().Fonts->AddFontFromFileTTF(p.string().c_str(),24);
            p=fonts/strong[i];if(std::filesystem::is_regular_file(p))valueFonts[i]=ImGui::GetIO().Fonts->AddFontFromFileTTF(p.string().c_str(),24);}
    }
#endif
    collection.connected=true;collection.ready=false;visible.clear();
}
void GameApp::reset(){previewOwner=1;previewCapturedAt=-1;state=Json{};sourceReady=false;pendingCommand=false;ruleAnswerPending=false;collection.revealID=0;collection.revealed.clear();inspecting=false;showTableDetails=false;showRuleReference=false;confirmationTitle.clear();groupChoices.clear();groupHand=-1;presentation={};demoPresentation={};demoAnimation={};demoIdentity.clear();demoSelected=false;selectedHand=-1;deckDialog.clear();renamingDeck=focusDeckName=false;inspectionList.clear();inspectionModifier=0;trade={};resetPopups=true;escapeRequested=false;seenEvents.clear();seenReveal=0;dismissedReveal=0;closing=false;}
void GameApp::receive(std::string_view raw){
    Json next=Json::parse(raw);auto nextAlbum=albumFromGame(next);
    const bool changed=!sourceReady||next.at("session")!=state.at("session")||next.at("revision")!=state.at("revision");
    const bool newSession=!sourceReady||next.at("session")!=state.at("session");
    const auto previousID=selected>=0&&selected<int(collection.cards.size())?collection.cards[selected].id:0;
    const auto previousDeck=collection.activeDeck;
    if(changed){inspecting=false;showTableDetails=false;showRuleReference=false;groupChoices.clear();groupHand=-1;confirmationTitle.clear();selectedHand=-1;pendingCommand=false;ruleAnswerPending=false;resetPopups=true;}
    if(changed&&label(next,"screen")=="lesson"){demoSelected=false;selectedHand=-1;}
    if(newSession){claimMask=0;trade={};wager=std::max(0,number(member(next,"collection"),"fixedWager"));}
    if(sourceReady&&number(next,"cardBackVersion")!=number(state,"cardBackVersion"))assets.invalidateBacks();
    const Json previousState=changed?state:Json{};
    state=std::move(next);collection=std::move(nextAlbum);sourceReady=true;pendingCommand=false;
    selected=collection.index(previousID);if(selected<0)selected=0;if(previousDeck!=collection.activeDeck)deckSlot=-1;
    if(selectedPack>=0&&collection.packs[selectedPack]<1)selectedPack=-1;
    if(collection.revealID==dismissedReveal){collection.revealID=0;collection.revealed.clear();}
    else if(collection.revealID&&collection.revealID!=seenReveal){seenReveal=collection.revealID;revealedAt=ImGui::GetTime();tab=0;resetPopups=true;}
    if(label(state,"albumSection")=="tournaments")tab=4;
    if(label(state,"screen")=="lobby"&&label(state,"phase")=="ready"){
        const int fixed=number(member(state,"collection"),"fixedWager",-1);wager=std::clamp(fixed<0?wager:fixed,0,number(member(state,"collection"),"maxWager"));
    }
    presentation.begin(state,ImGui::GetTime(),previousState);
    applySettings();filterDirty=true;refreshFilter();
}
void GameApp::applySettings(){
    const auto& c=member(state,"settings");theme=ttcg::interfaceThemeIndex(number(c,"theme"));
    face.classic=number(c,"rankLayout")==1;face.names=flag(c,"showCardNames",true);face.groups=flag(c,"showCardGroups");face.nameplate=number(c,"nameplateStyle");face.lettering=number(c,"nameCase");
    face.shadow=number(c,"rankShadowOpacity",100)/100.f;face.stroke=flag(c,"rankStroke")?number(c,"rankStrokeWeight",10)/10.f:0;face.grain=number(c,"boardTexture",10)/100.f;
    face.shadowBlur=float(number(c,"rankShadowBlur",2));face.shadowOffset=float(number(c,"rankShadowOffset",1));
    matchAppearance.treatment=std::clamp(number(c,"opponentTreatment"),0,3);
    matchAppearance.scope=std::clamp(number(c,"opponentScope",2),0,2);
    matchAppearance.strength=std::clamp(number(c,"opponentStrength",100),0,100)/100.f;
    matchAppearance.palette=std::clamp(number(c,"controlPalette"),0,4);
    matchAppearance.playerColour=std::clamp(number(c,"playerColour",0x87c4dd),0,0xffffff);
    matchAppearance.opponentColour=std::clamp(number(c,"opponentColour",0xdf9585),0,0xffffff);
    matchAppearance.borderStyle=std::clamp(number(c,"controlBorderStyle"),0,4);
    matchAppearance.borderWidth=std::clamp(number(c,"controlBorderWidth",12),0,40)/10.f;
    matchAppearance.borderOpacity=std::clamp(number(c,"controlBorderOpacity",100),0,100)/100.f;
    matchAppearance.marker=flag(c,"controlMarker");
    preferredFont=interfaceFonts[std::clamp(number(c,"interfaceFont")?number(c,"interfaceFont"):((theme==1||theme==2)?3:1),0,4)];
    if(!number(c,"interfaceFont")){
        if(palette().frame==FrameStyle::Veldun)preferredFont=lato;
        else if(palette().frame!=FrameStyle::Classic)preferredFont=jost;
    }
    cardNameFont=nameFonts[std::clamp(number(c,"cardNameFont",1)?number(c,"cardNameFont"):1,0,4)];
    cardValueFont=valueFonts[std::clamp(number(c,"cardValueFont",1)?number(c,"cardValueFont"):1,0,4)];
    customBack=label(state,"cardBack","backs/mosaic.webp");
}
void GameApp::send(std::string_view verb,std::string args,bool exclusive){
    if(!sourceReady||!submit||(exclusive&&pendingCommand))return;
    const auto command=std::string(verb)+" "+std::to_string(state.at("session").get<std::uint64_t>())+" "+std::to_string(state.at("revision").get<std::uint64_t>())+(args.empty()?"":" "+args);
    submit(command);if(exclusive){pendingCommand=true;collection.busy=true;sentAt=ImGui::GetTime();}
}
void GameApp::request(Json command){
    const auto action=label(command,"action");
    if(action=="dismiss-pack"){dismissedReveal=collection.revealID;collection.revealID=0;collection.revealed.clear();resetPopups=true;return;}
    if(action=="open-pack"){send(number(command,"kind")?"tournament":"open-pack",number(command,"kind")?"pack 0":"");return;}
    if(action=="deck"){
        std::string args;for(auto id:command.at("cards").get<ttcg::Hand>()){if(!args.empty())args+=' ';const auto i=collection.index(id);args+=std::to_string(i<0?-1:i+(ttcg::isFoil(id)?int(collection.cards.size()):0));}send("deck",args);return;
    }
    send("deck-manage",action+" "+std::to_string(command.value("deckID",collection.activeDeck))+" "+label(command,"name"));
}
void GameApp::captureKey(unsigned scan,unsigned modifiers){
    const auto name=label(member(state,"settings"),"capturing");if(name.empty())return;
    if(scan==1){send("binding","cancel");return;}
    const auto value=scan|modifiers;if(ttcg::validHotkey(value))send("binding","set "+name+" "+std::to_string(value));
}
void GameApp::confirm(std::string title,std::string copy,std::string verb,std::string args,std::string action){
    confirmationTitle=std::move(title);confirmationCopy=std::move(copy);confirmationVerb=std::move(verb);confirmationArgs=std::move(args);confirmationLabel=std::move(action);
    confirmSession=state.at("session");confirmRevision=state.at("revision");
}
void GameApp::confirmation(){
    if(confirmationTitle.empty())return;
    if(!ImGui::IsPopupOpen("Confirm"))ImGui::OpenPopup("Confirm");
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize*.5f,ImGuiCond_Always,ImVec2(.5f,.5f));ImGui::SetNextWindowSize(ImVec2(550*scale,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(30,28)*scale);
    if(ImGui::BeginPopupModal("Confirm",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|popupFlags())){
        popupSurface();
        textBlock(confirmationTitle,490*scale,27,palette().accent);
        if(!confirmationCopy.empty()){ImGui::Dummy(ImVec2(0,14*scale));textBlock(confirmationCopy,490*scale);}
        ImGui::Dummy(ImVec2(0,24*scale));auto p=ImGui::GetCursorScreenPos();
        if(button("cancel","Cancel",p,ImVec2(160,40)*scale)||ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false)){confirmationTitle.clear();ImGui::CloseCurrentPopup();}
        if(button("confirm",confirmationLabel.c_str(),p+ImVec2(270,0)*scale,ImVec2(220,40)*scale,true,!pendingCommand)){
            if(confirmSession==state.at("session")&&confirmRevision==state.at("revision"))send(confirmationVerb,confirmationArgs);
            confirmationTitle.clear();ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }ImGui::PopStyleVar();
}
void GameApp::textBlock(const std::string& value,float width,float pixels,ImU32 colour){
    const auto at=ImGui::GetCursorScreenPos();const auto h=font()->CalcTextSizeA(pixels*scale,1e6f,width,value.c_str()).y;
    text(at,value,pixels,colour?colour:palette().muted,width);ImGui::Dummy(ImVec2(width,h));
}
bool GameApp::chapter(const std::string& id,const std::string& title,ImVec2 at,ImVec2 size,bool active,const std::string& detail,float trailing,int marker){
    ImGui::SetCursorScreenPos(at);const bool hit=ImGui::InvisibleButton(id.c_str(),size,ImGuiButtonFlags_EnableNav);auto* d=ImGui::GetWindowDrawList();
    const bool hover=ImGui::IsItemHovered()||ImGui::IsItemFocused();
    if(active||hover){const auto colour=palette().selection;d->AddRectFilledMultiColor(at,at+size,(colour&0xffffff)|((active?105u:65u)<<24),colour&0xffffff,colour&0xffffff,(colour&0xffffff)|((active?105u:65u)<<24));d->AddLine(at,at+ImVec2(0,size.y),active?palette().accent:palette().line,active?2:1);}
    const float inset=(marker?30:12)*scale;
    if(marker){
        const auto centre=at+ImVec2(17,detail.empty()?22:17)*scale;
        if(marker==1){
            std::array<ImVec2,10> points;for(int i=0;i<10;++i){const float angle=i*std::numbers::pi_v<float>/5-std::numbers::pi_v<float>/2,r=(i%2?2.7f:6.5f)*scale;points[i]=centre+ImVec2(std::cos(angle),std::sin(angle))*r;}
            for(int i=0;i<10;++i)d->AddTriangleFilled(centre,points[i],points[(i+1)%10],palette().accent);
        }else d->AddCircleFilled(centre,2.5f*scale,palette().accent);
    }
    text(at+ImVec2(inset,(detail.empty()?12:7)*scale),title,palette().sans?17:19,active?palette().accent:palette().ink,size.x-inset-12*scale-trailing);
    if(!detail.empty())text(at+ImVec2(12,37)*scale,detail,13,palette().muted,size.x-24*scale);
    return hit;
}
bool GameApp::combo(const char* id,const char* label,int& value,const std::vector<std::string>& choices,float width){
    const auto at=ImGui::GetCursorScreenPos();text(at+ImVec2(0,8*scale),label,19,palette().ink);
    ImGui::SetCursorScreenPos(at+ImVec2(width*.55f,0));const bool changed=select(id,value,choices,width*.45f);
    ImGui::SetCursorScreenPos(at+ImVec2(0,58*scale));ImGui::Dummy(ImVec2(width,0));return changed;
}
void GameApp::inspectCard(int display,bool meta,int modifier){
    if(display<0||display>=int(collection.cards.size()*2))return;
    selected=display%int(collection.cards.size());selectedFoil=display>=int(collection.cards.size());selectedPack=-1;inspecting=true;showTableDetails=meta;inspectionModifier=modifier;
    inspectionList.clear();
    if(collection.revealID){for(auto id:collection.revealed){const auto index=collection.index(id);if(index>=0)inspectionList.push_back(index+(ttcg::isFoil(id)?collection.cards.size():0));}}
    else if(label(state,"screen")=="album"&&tab<=1)inspectionList=visible;
    yaw=pitch=turnTarget=0;autoRotate=false;
}
float GameApp::metadata(const CardView& c,ImVec2 at,float width,bool draw){
    float y=at.y;
    const auto row=[&](const std::string& value,float pixels,ImU32 colour,float gap){
        if(draw)text(ImVec2(at.x,y),value,pixels,colour,width);
        y+=font()->CalcTextSizeA(pixels*scale,1e6f,width,value.c_str()).y+gap*scale;
    };
    row(c.name,30,palette().ink,24);
    const char* tiers[]{"","I","II","III","IV","V","VI","VII","VIII","IX","X"};
    row(std::string("Tier ")+tiers[c.tier]+" · "+c.rarity+(selectedFoil?" · Foil":""),18,palette().muted,20);
    std::string group;for(unsigned i=0;i<ttcg::creatureGroups.size();++i)if(c.groups&(1ull<<i)){if(!group.empty())group+=" · ";group+=ttcg::creatureGroups[i];}
    row(group,20,palette().ink,20);
    for(int a=0;a<5;++a)if(c.affinityMask&(1u<<a)){
        if(draw){const ImVec2 p(at.x,y);auto image=assets.get("../icons/affinity-"+std::to_string(a)+".png");
            ImGui::GetWindowDrawList()->AddImage(image.id,p,p+ImVec2(25,25)*scale);text(p+ImVec2(33,2)*scale,ttcg::attributeNames[a],18,palette().muted);}
        y+=35*scale;
    }
    if(!c.affinityMask)row("Neutral",18,palette().muted,10);
    y+=14*scale;
    std::string values;for(int i=0;i<4;++i){if(i)values+="   ";values+=std::array<const char*,4>{"↑ ","→ ","↓ ","← "}[i];values+=c.sides[i]==10?"A":std::to_string(c.sides[i]);}
    row(values,22,palette().ink,0);
    if(inspectionModifier){y+=12*scale;row(std::string("All sides ")+(inspectionModifier>0?"+":"−")+std::to_string(std::abs(inspectionModifier)),19,palette().accent,0);}
    return y-at.y;
}
void GameApp::displayCard(int display,ImVec2 at,ImVec2 size,bool chosen,int owner,int slot,int modifier,bool large,CardContext context){
    if(display==-2){auto back=assets.get(customBack);ImGui::GetWindowDrawList()->AddImage(back.id,at,at+size);controlFrame(at,size,owner);return;}
    if(display<0||display>=int(collection.cards.size()*2))return;
    card(display%int(collection.cards.size()),at,size,chosen,display>=int(collection.cards.size()),owner,slot,large,context);
    if(modifier){const auto t=(modifier>0?"+":"−")+std::to_string(std::abs(modifier));const float pixels=size.x*.15f;const auto p=at+ImVec2(size.x*.5f,size.y*.49f);const auto extent=bold->CalcTextSizeA(pixels,1e5f,0,t.c_str());auto* d=ImGui::GetWindowDrawList();
        d->AddText(bold,pixels,p-extent*.5f+ImVec2(1,2),IM_COL32(0,0,0,255),t.c_str());d->AddText(bold,pixels,p-extent*.5f,modifier>0?IM_COL32(180,224,158,255):IM_COL32(244,163,144,255),t.c_str());}
}
float GameApp::ruleChips(ImVec2 at,float width,unsigned flags,int trade,bool casual,bool interactive,const Json* changes,bool centred){
    struct Chip{std::string name;int rule=-1,trade=-1;bool active=true;const Json* change=nullptr;};
    std::vector<Chip> chips;
    for(auto [bit,name]:ruleNames)if(flags&bit)chips.push_back({name,int(bit)});
    if(casual)chips.push_back({"Casual"});else if(trade>=0)chips.push_back({tradeName(trade),-1,trade});
    if(changes&&changes->is_array())for(const auto& change:*changes){
        const bool active=flag(change,"active");const int rule=number(change,"rule",-1),changedTrade=number(change,"trade",-1);
        auto found=std::find_if(chips.begin(),chips.end(),[&](const auto& chip){return (rule>=0&&chip.rule==rule)||(changedTrade>=0&&chip.trade==changedTrade);});
        if(found!=chips.end()&&active)found->change=&change;
        else if(found==chips.end()&&!active){const auto name=rule>=0?rulesLabel(rule,false):tradeName(changedTrade);if(!name.empty())chips.push_back({name,rule,changedTrade,false,&change});}
    }
    std::vector<float> widths,rows{0};
    for(const auto& chip:chips){const auto shown=(chip.change?(chip.active?"+ ":"− "):"")+chip.name;
        const float w=font()->CalcTextSizeA(17*scale,1e5f,0,shown.c_str()).x+20*scale;
        widths.push_back(w);if(rows.back()>0&&rows.back()+w>width)rows.push_back(0);rows.back()+=w;}
    unsigned row=0;
    float x=at.x,y=at.y;int index=0;
    if(centred)x+=(width-rows[0])*.5f;
    for(const auto& chip:chips){const auto& name=chip.name;const std::string shown=(chip.change?(chip.active?"+ ":"− "):"")+name;
        const auto w=widths[index];if(x+w>at.x+width+.5f&&x>at.x){++row;x=at.x+(centred?(width-rows[row])*.5f:0);y+=32*scale;}
        if(centred&&x>at.x+(width-rows[row])*.5f+.5f)ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(x-2*scale,y+15*scale),.75f*scale,palette().muted);
        ImGui::PushID(index++);ImGui::SetCursorScreenPos(ImVec2(x,y));const bool hit=ImGui::InvisibleButton("rule",ImVec2(w,30*scale),ImGuiButtonFlags_EnableNav);
        const auto ink=chip.change&&chip.active?palette().accent:palette().muted;
        text(ImVec2(x+8*scale,y+5*scale),shown,17,ink);
        if(!chip.active)line(ImVec2(x+26*scale,y+15*scale),ImVec2(x+w-10*scale,y+15*scale),ink);
        std::string help,chapterID="trade";
        for(const auto& page:reference.at("pages"))if(label(page,"title")==name){help=label(page,"copy");chapterID=label(page,"id");}
        for(const auto& page:reference.at("trades"))if(label(page,"title")==name)help=label(page,"copy");
        if(name=="Casual")help="Games with children have no card trades or gold wagers.";
        if(chip.change)help+=(chip.active?"\n\nProtected from removal until ":"\n\nCannot return until ")+label(*chip.change,"until")+".";
        if(ImGui::IsItemHovered()||ImGui::IsItemFocused()){ImGui::BeginTooltip();textBlock(help,350*scale,17,palette().ink);ImGui::EndTooltip();}
        if(hit&&interactive&&!presentation.pending(state)&&!pendingCommand&&!flag(state,"thinking")){topic=chapterID;demoStep=0;demoSelected=false;showRuleReference=true;}
        x+=w;ImGui::PopID();
    }
    return y-at.y+32*scale;
}
const Json* GameApp::tournament(unsigned id)const{for(const auto& e:arrayMember(member(member(state,"collection"),"tournaments"),"events"))if(number(e,"id")==int(id))return &e;return nullptr;}
void GameApp::closeView(){
    if(capturing()){send("binding","cancel");return;}
    if(!confirmationTitle.empty()){confirmationTitle.clear();resetPopups=true;return;}
    if(inspecting){inspecting=false;resetPopups=true;return;}
    if(showRuleReference){showRuleReference=false;resetPopups=true;return;}
    if(collection.revealID){dismissedReveal=collection.revealID;collection.revealID=0;collection.revealed.clear();resetPopups=true;return;}
    if(groupHand>=0){groupHand=-1;groupChoices.clear();resetPopups=true;return;}
    const auto& c=member(state,"collection");
    if(const auto* e=tournament(number(c,"tournamentID"));e&&number(*e,"foilUpgrades")>0){confirm("Leave unused foil upgrades?","They will be lost.","tournament","leave "+std::to_string(number(*e,"id")),"Leave");return;}
    if(label(state,"phase")=="match"){confirm("Leave this game?",number(c,"tournamentID")?"This ends your tournament run.":flag(c,"practice")?"End this practice game?":"The game counts as a loss. Its trade rule and wager still apply.","forfeit",{},"Leave");return;}
    if(flag(c,"staked")||flag(c,"paying"))return;
    if(label(state,"screen")=="album"&&flag(state,"matchContext")){send("browse","lobby");return;}
    send("close");
}
void GameApp::header(ImVec2 at,ImVec2 size){
    if(palette().frame!=FrameStyle::Classic){
        auto* d=ImGui::GetWindowDrawList();
        const auto colour=(palette().frame==FrameStyle::Oathvein?palette().selection:palette().accent)&0xffffff;
        const auto start=at-ImVec2(18,12)*scale,end=at+ImVec2(size.x+18*scale,45*scale);
        d->AddRectFilledMultiColor(start,end,colour|0x18000000,colour|0x05000000,colour,colour);
    }
    text(at,"T E S S E R A",22,palette().accent);
    if(flag(state,"musicAvailable")&&(flag(state,"matchContext")||label(state,"screen")=="lobby"))
        if(button("music",flag(state,"musicEnabled")?"♪":"♪ ×",at+ImVec2(size.x-265*scale,-3*scale),ImVec2(45,36)*scale))send("music",flag(state,"musicEnabled")?"0":"1",false);
    text(at+ImVec2(size.x-206*scale,6*scale),"Size",17,palette().muted);
    ImGui::SetCursorScreenPos(at+ImVec2(size.x-160*scale,-3*scale));ImGui::SetNextItemWidth(105*scale);
    const int percent=number(member(state,"settings"),"windowSize",90);int choice=std::clamp((percent-60)/5,0,8);
    if(select("##size",choice,{"60%","65%","70%","75%","80%","85%","90%","95%","100%"},105*scale))send("settings","window-size "+std::to_string(60+choice*5));
    if(closeButton("close",at+ImVec2(size.x-34*scale,-3*scale)))closeView();
    line(at+ImVec2(0,45*scale),at+ImVec2(size.x,45*scale));
}
void GameApp::draw(){
    if(resetPopups){if(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel))ImGui::ClosePopupToLevel(0,true);resetPopups=false;}
    assets.beginFrame();dragPreviewDrawn=false;
    if(!sourceReady)return;
    applySettings();const auto display=ImGui::GetIO().DisplaySize;const int percent=number(member(state,"settings"),"windowSize",90);
    scale=std::min(display.x/1400,display.y/920)*(percent/100.f);frameSize=percent==100?display:ImVec2(1400,920)*scale;frameAt=(display-frameSize)*.5f;
    style();ImGui::PushFont(font(),18*scale);
    if((pendingCommand||ruleAnswerPending)&&ImGui::GetTime()-sentAt>5){pendingCommand=false;ruleAnswerPending=false;collection.busy=false;collection.notice="The action did not finish. You can try again.";}
    if(ruleRequestModal()){ImGui::PopFont();tickPresentation();return;}
    ImGui::SetNextWindowPos(frameAt);ImGui::SetNextWindowSize(frameSize);
    ImGui::Begin("Tessera",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoScrollbar);
    auto* d=ImGui::GetWindowDrawList();d->PushClipRect(frameAt,frameAt+frameSize,false);surface(frameAt,frameSize,true);
    panelFrame(frameAt,frameSize,true);
    d->PopClipRect();
    const auto at=frameAt+ImVec2(28,21)*scale;const float width=frameSize.x-56*scale;header(at,ImVec2(width,46*scale));
    const bool back=escapeRequested||ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false);escapeRequested=false;
    if(back&&!ImGui::GetIO().WantTextInput&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel))closeView();
    ImVec2 body=at+ImVec2(0,70*scale),size(width,frameSize.y-116*scale);
    const auto page=label(state,"screen"),phase=label(state,"phase");
    if(page=="album"&&phase=="ready"){
        static const char* names[]{"Cards","Decks","Journal","Rules","Tournaments","Settings"};const int order[]{0,1,2,4,3,5};float x=body.x;
        for(int i:order){std::string name=names[i];if(palette().sans)for(auto& ch:name)ch=char(std::toupper(static_cast<unsigned char>(ch)));
            const float w=font()->CalcTextSizeA((palette().sans?18:24)*scale,1e5f,0,name.c_str()).x+12*scale;
            if(sectionTab(names[i],name,ImVec2(x,body.y),ImVec2(w,48*scale),tab==i)){tab=i;selectedPack=-1;}
            x+=w+28*scale;
        }
        if(flag(state,"matchContext")&&button("return","Return to game",ImVec2(body.x+width-160*scale,body.y),ImVec2(160,38)*scale))send("browse","lobby");
        line(body+ImVec2(0,48*scale),body+ImVec2(width,48*scale));body.y+=70*scale;size.y-=70*scale;
        const auto& interrupted=member(state,"interruptedMatch");
        if(!interrupted.is_null()&&!interrupted.empty()){
            text(body,"Resume your game with "+label(interrupted,"opponent")+".",17,palette().accent,width-300*scale);
            if(flag(interrupted,"canResume")&&button("resume","Resume",body+ImVec2(width-240*scale,-5*scale),ImVec2(105,35)*scale))send("interrupted","resume");
            if(!flag(interrupted,"finished")&&button("abandon","Forfeit",body+ImVec2(width-115*scale,-5*scale),ImVec2(115,35)*scale))confirm("Forfeit this game?","The trade rule and wager still apply.","interrupted","forfeit","Forfeit");
            body.y+=47*scale;size.y-=47*scale;
        }
        screen=tab==1?3:tab==5?2:0;
        if(tab==0)nativeAlbum(body,size);else if(tab==1)nativeDecks(body,size);else if(tab==2)journal(body,size);else if(tab==3)rules(body,size);else if(tab==4)tournaments(body,size);else settings(body,size);
    }else if(page=="shop")shop(body,size);else if(page=="lesson")lessonPanel(body,size);else if(phase=="ready")lobby(body,size);else matchPanel(body,size);
    auto notice=!collection.notice.empty()?collection.notice:label(member(state,"settings"),"interfaceNotice");
    if(notice.empty()&&page=="album"){
        const auto missing=join(arrayMember(member(state,"collection"),"missingSets"),", ");
        if(!missing.empty())notice="Cards unavailable from "+missing+". Reinstall the expansion to restore them.";
        else if(!arrayMember(state,"expansionErrors").empty())notice="An expansion could not be loaded. See TTCG.log.";
    }
    if(!notice.empty())text(frameAt+ImVec2(28,frameSize.y/scale-31)*scale,notice,16,palette().accent,width);
    ImGui::End();
    deckModal();packModal();rulesModal();groupModal();resultModal();tournamentOutcome();confirmation();
    if(!collection.revealID)nativeInspector();
    ImGui::PopFont();tickPresentation();
}

}
