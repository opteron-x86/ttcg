#include "GameApp.h"
#include <cmath>
#include <cstdio>
#include <numbers>
#ifdef _WIN32
#include <windows.h>
#endif
namespace preview {
namespace {
std::string keyLabel(unsigned binding){if(!binding)return "Unbound";std::string text;for(auto [mask,name]:std::array<std::pair<unsigned,const char*>,6>{{{256,"LShift"},{512,"RShift"},{1024,"LCtrl"},{2048,"RCtrl"},{4096,"LAlt"},{8192,"RAlt"}}})if(binding&mask)text+=std::string(name)+" + ";const unsigned scan=binding&255;
#ifdef _WIN32
    wchar_t wide[64]{};const LONG parameter=(scan&127)<<16|(scan>=128?1<<24:0);if(GetKeyNameTextW(parameter,wide,64)){char result[192]{};WideCharToMultiByte(CP_UTF8,0,wide,-1,result,192,nullptr,nullptr);return text+result;}
#endif
    if(scan>=59&&scan<=68)return text+"F"+std::to_string(scan-58);return text+"Key "+std::to_string(scan);
}
}
void GameApp::matchAppearancePreview(ImVec2 at,ImVec2 size){
    if(collection.cards.empty())return;
    auto* d=ImGui::GetWindowDrawList();const float gap=18*scale;
    line(at-ImVec2(AlbumLayout::gap*.5f*scale,0),at+ImVec2(-AlbumLayout::gap*.5f*scale,size.y));
    const float w=std::min((size.x-gap-8*scale)*.5f,std::max(54*scale,(size.y-280*scale)*.375f)),h=w/.75f;
    const float left=(size.x-2*w-gap)*.5f;
    // Reuse known art so the preview doesn't reveal undiscovered cards.
    const auto known=[&](int i){const auto& c=collection.cards[i];return c.known[0]||c.known[1]||c.owned[0]||c.owned[1];};
    int sample=std::clamp(selected,0,int(collection.cards.size())-1);
    if(!known(sample)){
        sample=-1;for(int i=0;i<int(collection.cards.size());++i)if(known(i)){sample=i;break;}
    }
    if(sample<0)return;
    const auto handAt=at+ImVec2(left,76*scale);
    text(at+ImVec2(size.x*.5f,0),"Hand",19,palette().muted,0,true);
    for(int owner=0;owner<2;++owner){const auto p=handAt+ImVec2(owner*(w+gap),0);
        text(p+ImVec2(w*.5f,-33*scale),owner?"Opponent":"You",17,controlColour(owner,true),0,true);
        card(sample,p,ImVec2(w,h),false,false,owner,MaterialSlot::appearance+owner,false,CardContext::Hand);
    }
    const auto boardAt=handAt+ImVec2(0,h+82*scale);
    text(boardAt+ImVec2(w+gap*.5f,-39*scale),"Board",19,palette().muted,0,true);
    const auto material=number(member(state,"settings"),"boardSurface");
    boardSurface(material,boardAt-ImVec2(7,7)*scale,ImVec2(2*w+gap,h)+ImVec2(14,14)*scale);
    card(sample,boardAt,ImVec2(w,h),false,false,0,MaterialSlot::appearance+2,false,CardContext::Board);
    float fold=1;int owner=previewOwner;
    if(previewCapturedAt>=0){const double progress=(ImGui::GetTime()-previewCapturedAt)/.55;
        if(progress>=1){previewOwner=1-previewOwner;owner=previewOwner;previewCapturedAt=-1;}
        else{if(progress>=.5)owner=1-previewOwner;fold=std::max(.025f,float(std::abs(std::cos(progress*std::numbers::pi))));}}
    const auto cardAt=boardAt+ImVec2(w+gap,0);const int first=d->VtxBuffer.Size;
    card(sample,cardAt,ImVec2(w,h),false,false,owner,MaterialSlot::appearance+3,false,CardContext::Board);
    for(int v=first;v<d->VtxBuffer.Size;++v)d->VtxBuffer[v].pos.x=cardAt.x+w*.5f+(d->VtxBuffer[v].pos.x-cardAt.x-w*.5f)*fold;
    const auto controls=boardAt+ImVec2(0,h+27*scale);
    if(button("preview-capture",previewOwner?"Capture":"Return",controls,ImVec2(2*w+gap,36*scale),false,previewCapturedAt<0))previewCapturedAt=ImGui::GetTime();
    if(button("preview-next-art","Change card",controls+ImVec2(0,48*scale),ImVec2(2*w+gap,36*scale))){
        for(int i=0;i<int(collection.cards.size());++i){selected=(sample+1+i)%int(collection.cards.size());if(known(selected))break;}}
}
void GameApp::settings(ImVec2 at,ImVec2 size){
    const auto& s=member(state,"settings");const auto& dev=member(s,"development");const float nav=AlbumLayout::navigation*scale,gap=AlbumLayout::gap*scale;const bool developer=flag(dev,"enabled");if(settingsPage==3&&!developer)settingsPage=0;
    const char* names[]{"Appearance","Controls","Gameplay","Developer"};for(int i=0;i<(developer?4:3);++i)if(chapter(names[i],names[i],at+ImVec2(0,i*47*scale),ImVec2(nav-20*scale,43*scale),settingsPage==i))settingsPage=i;
    line(at+ImVec2(nav+gap/2,0),at+ImVec2(nav+gap/2,size.y));const float preview=settingsPage==0?AlbumLayout::preview*scale:0;const float area=size.x-nav-gap-(preview?preview+gap:0);const auto origin=at+ImVec2(nav+gap,0);
    text(origin,names[settingsPage],30,palette().ink);
    if(settingsPage==0){
        if(sectionTab("appearance-general","General",origin+ImVec2(area-198*scale,0),ImVec2(90,40)*scale,appearancePage==0))appearancePage=0;
        if(sectionTab("appearance-match","Match",origin+ImVec2(area-98*scale,0),ImVec2(90,40)*scale,appearancePage==1))appearancePage=1;
    }
    ImGui::SetCursorScreenPos(origin+ImVec2(0,63)*scale);ImGui::BeginChild(settingsPage==0&&appearancePage==1?"match-appearance-body":"settings-body",ImVec2(area,size.y-63*scale));
    const float width=ImGui::GetContentRegionAvail().x-16*scale;
    const auto valueOf=[&](const char* json){if(member(s,json).is_boolean())return int(flag(s,json));for(const auto& field:ttcg::InterfaceSettings::fields)if(field.json==json)return number(s,json,field.defaultValue);return number(s,json);};
    const auto heading=[&](const char* title){ImGui::Dummy(ImVec2(0,15*scale));line(ImGui::GetCursorScreenPos(),ImGui::GetCursorScreenPos()+ImVec2(width,0));ImGui::Dummy(ImVec2(0,19*scale));textBlock(title,width,22,palette().accent);ImGui::Dummy(ImVec2(0,16*scale));};
    const auto toggle=[&](const char* name,const char* key,const char* json){bool v=valueOf(json)!=0;const auto p=ImGui::GetCursorScreenPos();text(p+ImVec2(0,9)*scale,name,19,palette().ink,width-115*scale);if(button(key,v?"On":"Off",p+ImVec2(width-68*scale,0),ImVec2(68,36)*scale,v,!pendingCommand&&!capturing()))send("settings",std::string(key)+" "+(v?"0":"1"));ImGui::SetCursorScreenPos(p+ImVec2(0,58)*scale);ImGui::Dummy(ImVec2(width,0));};
    const auto select=[&](const char* name,const char* key,const char* json,std::vector<std::string> choices){int value=valueOf(json);if(combo(key,name,value,choices,width))send("settings",std::string(key)+" "+std::to_string(value));};
    const auto slider=[&](const char* name,const char* key,const char* json,int maximum,const char* format="%d",bool tenths=false){
        const auto p=ImGui::GetCursorScreenPos();text(p+ImVec2(0,8)*scale,name,18,palette().ink,width*.51f);
        ImGui::SetCursorScreenPos(p+ImVec2(width*.56f,0));ImGui::SetNextItemWidth(width*.44f-72*scale);int value=valueOf(json);
        for(auto colour:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive})ImGui::PushStyleColor(colour,ImVec4(0,0,0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
        ImGui::SliderInt((std::string("##")+key).c_str(),&value,0,maximum,"",ImGuiSliderFlags_NoInput);
        ImGui::PopStyleVar();ImGui::PopStyleColor(5);
        const auto low=ImGui::GetItemRectMin(),high=ImGui::GetItemRectMax();const float y=(low.y+high.y)*.5f;
        const float first=low.x+8*scale,last=high.x-8*scale,x=first+(last-first)*float(value)/maximum;
        auto* draw=ImGui::GetWindowDrawList();draw->AddLine(ImVec2(first,y),ImVec2(last,y),palette().line,2*scale);
        draw->AddLine(ImVec2(first,y),ImVec2(x,y),palette().accent,2*scale);
        draw->AddCircleFilled(ImVec2(x,y),(ImGui::IsItemHovered()||ImGui::IsItemActive()?4.5f:3.5f)*scale,palette().accent,20);
        char shown[32]{};if(tenths)std::snprintf(shown,sizeof(shown),format,value/10.f);else std::snprintf(shown,sizeof(shown),format,value);
        textRight(p+ImVec2(width,8*scale),shown,18,palette().ink);
        if(ImGui::IsItemDeactivatedAfterEdit())send("settings",std::string(key)+" "+std::to_string(value));
        else if(ImGui::IsItemActive())state["settings"][json]=value;
        ImGui::SetCursorScreenPos(p+ImVec2(0,58)*scale);ImGui::Dummy(ImVec2(width,0));
    };
    if(settingsPage==0&&appearancePage==0){
        const auto p=ImGui::GetCursorScreenPos();const int current=number(s,"windowSize",90);text(p+ImVec2(0,8)*scale,"Fullscreen",19,palette().ink);if(button("fullscreen",current==100?"On":"Off",p+ImVec2(width-68*scale,0),ImVec2(68,36)*scale,current==100))send("settings","window-size "+std::to_string(current==100?90:100));ImGui::SetCursorScreenPos(p+ImVec2(0,58)*scale);ImGui::Dummy(ImVec2(width,0));
        int sizeIndex=std::clamp((current-60)/5,0,8);if(combo("window-size","Window size",sizeIndex,{"60%","65%","70%","75%","80%","85%","90%","95%","100%"},width))send("settings","window-size "+std::to_string(60+5*sizeIndex));
        heading("Theme");std::vector<std::string> themeNames;for(const auto& theme:ttcg::interfaceThemes)themeNames.emplace_back(theme.name);
        select("Interface","theme","theme",themeNames);select("Game board","board-surface","boardSurface",{"Theme default","Plain","Slate","Felt","Parchment","Leather","Walnut"});slider("Board texture","board-texture","boardTexture",100,"%d%%");
        heading("Fonts");const std::vector<std::string> fonts{"Default","Georgia","Times New Roman","Arial","Verdana"};select("Interface","interface-font","interfaceFont",fonts);select("Card names","card-name-font","cardNameFont",fonts);select("Card values","card-value-font","cardValueFont",fonts);
        heading("Card face");select("Values","card-ranks","rankLayout",{"Edge values","Classic values"});toggle("Card names","card-names","showCardNames");toggle("Card groups","card-groups","showCardGroups");select("Nameplate","nameplate","nameplateStyle",{"Shaded band","Soft fade","Printed on art"});select("Lettering","name-case","nameCase",{"Small caps","Title case","Uppercase"});
        heading("Card values");slider("Shadow opacity","rank-shadow-opacity","rankShadowOpacity",100,"%d%%");slider("Shadow blur","rank-shadow-blur","rankShadowBlur",8);slider("Shadow offset","rank-shadow-offset","rankShadowOffset",6);toggle("Stroke","rank-stroke","rankStroke");slider("Stroke weight","rank-stroke-weight","rankStrokeWeight",30,"%.1f px",true);
        if(ImGui::Button("Reset card values"))send("settings","rank-finish-reset 0");
        heading("Card back");int i=0;for(const auto& back:arrayMember(state,"cardBacks")){const auto pos=ImGui::GetCursorScreenPos();ImGui::PushID(i++);if(chapter("back",label(back,"name"),pos+ImVec2(72*scale,0),ImVec2(width-72*scale,85*scale),label(state,"cardBackID")==label(back,"id"),flag(back,"custom")?"Custom":""))send("cardback","select "+label(back,"id"));auto image=assets.get(label(back,"art"));ImGui::GetWindowDrawList()->AddImage(image.id,pos,pos+ImVec2(54,72)*scale);ImGui::PopID();}
        if(ImGui::Button("Refresh card backs"))send("cardback","refresh");textBlock("Add custom images to the card backs folder, then refresh.",width,17);if(!label(s,"cardBackNotice").empty())textBlock(label(s,"cardBackNotice"),width,17,palette().accent);
    }else if(settingsPage==0){
        textBlock("Opponent artwork",width,22,palette().accent);ImGui::Dummy(ImVec2(0,16*scale));
        select("Finish","match-art-treatment","opponentTreatment",{"Original","Muted","Grayscale","Sepia"});
        if(valueOf("opponentTreatment")!=0){
            select("Apply to","match-art-scope","opponentScope",{"Hand","Board","Hand and board"});
            slider("Strength","match-art-strength","opponentStrength",100,"%d%%");
        }
        heading("Control colours");
        select("Palette","match-palette","controlPalette",{"Tessera","Blue / amber","Violet / mint","Ivory / slate","Custom"});
        if(valueOf("controlPalette")==4){
            const auto colour=[&](const char* title,const char* key,const char* json){
                const auto p=ImGui::GetCursorScreenPos();text(p+ImVec2(0,8)*scale,title,18,palette().ink,width*.51f);
                const unsigned rgb=valueOf(json);float channels[]{float(rgb>>16)/255.f,float((rgb>>8)&255)/255.f,float(rgb&255)/255.f};
                ImGui::SetCursorScreenPos(p+ImVec2(width*.56f,0));ImGui::SetNextItemWidth(width*.44f);
                const bool changed=ImGui::ColorEdit3((std::string("##")+key).c_str(),channels,ImGuiColorEditFlags_DisplayHex|ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_NoDragDrop);
                const int packed=(int(channels[0]*255+.5f)<<16)|(int(channels[1]*255+.5f)<<8)|int(channels[2]*255+.5f);
                if(changed)state["settings"][json]=packed;
                if(ImGui::IsItemDeactivatedAfterEdit())send("settings",std::string(key)+" "+std::to_string(packed));
                ImGui::SetCursorScreenPos(p+ImVec2(0,58)*scale);ImGui::Dummy(ImVec2(width,0));
            };
            colour("You","match-player-colour","playerColour");colour("Opponent","match-opponent-colour","opponentColour");
        }
        heading("Card borders");
        select("Style","match-border-style","controlBorderStyle",{"Single line","Double line","Clipped corners","Corner brackets","Original Tessera"});
        slider("Thickness","match-border-width","controlBorderWidth",40,"%.1f px",true);
        slider("Opacity","match-border-opacity","controlBorderOpacity",100,"%d%%");
        toggle("Control marker","match-control-marker","controlMarker");
        if(valueOf("controlMarker"))textBlock("Filled: yours. Outline: opponent’s.",width,17);
        ImGui::Dummy(ImVec2(0,16*scale));if(ImGui::Button("Reset match appearance"))send("settings","match-appearance-reset 0");
    }else if(settingsPage==1){
        for(const auto& field:ttcg::HotkeyBindings::fields){const auto p=ImGui::GetCursorScreenPos();const std::string id(field.name),name(field.label);text(p+ImVec2(0,8)*scale,name,21,palette().ink,width*.4f);
            const auto binding=member(s,id.c_str());unsigned code=binding.is_number_integer()?binding.get<unsigned>():0;const bool capture=label(s,"capturing")==id;
            if(button(id.c_str(),capture?"Press a key…":keyLabel(code).c_str(),p+ImVec2(width*.43f,0),ImVec2(width*.39f,40*scale),capture,!pendingCommand))send("binding",std::string(capture?"cancel":"begin "+id));
            if(button((id+"-clear").c_str(),"×",p+ImVec2(width-115*scale,0),ImVec2(35,40)*scale,false,!capturing()))send("binding","unbind "+id);
            if(button((id+"-reset").c_str(),"Reset",p+ImVec2(width-72*scale,0),ImVec2(72,40)*scale,false,!capturing()))send("binding","reset "+id);
            ImGui::SetCursorScreenPos(p+ImVec2(0,79)*scale);ImGui::Dummy(ImVec2(width,0));}
        textBlock("Left and right Shift, Ctrl, and Alt can be combined with a key. Escape cancels.",width,18);if(!label(s,"notice").empty())textBlock(label(s,"notice"),width,18,palette().accent);
        heading("In Tessera");textBlock("Click a card, then a board square to play. You can also drag cards onto the board or into a deck. Right-click a card to inspect it. Tab and the arrow keys move focus; Enter selects. On a controller, use the directional pad and confirm button. Back closes the current panel.",width,19);
    }else if(settingsPage==2){
        const auto& collectionState=member(state,"collection");
        const auto missing=join(arrayMember(collectionState,"missingSets"),", ");
        if(!missing.empty()){
            textBlock("Unavailable cards",width,22,palette().accent);ImGui::Dummy(ImVec2(0,16*scale));
            textBlock("Missing expansions: "+missing+". Reinstall them to restore your cards, or remove their saved copies.",width,18);
            const auto blocked=label(collectionState,"recoveryBlocked");
            ImGui::BeginDisabled(pendingCommand||capturing()||!member(state,"interruptedMatch").empty()||!blocked.empty());
            if(ImGui::Button("Remove unavailable cards"))confirm("Remove unavailable cards?","Removes unavailable cards and their discovery marks from this character, and clears their deck slots. Installed cards and game progress are kept. Reinstalling the expansions will no longer restore these copies. Keep an earlier save if you may want them back.","collection-recovery","remove-missing confirm","Remove cards");
            ImGui::EndDisabled();if(!blocked.empty())textBlock(blocked,width,17,palette().accent);
            heading("Gameplay");
        }
        toggle("Tessera games","games","gamesEnabled");textBlock("Disables Tessera games. The album is still available.",width,17);heading("Modded players");select("Residents","residents","residents",{"Disabled","Some residents","All eligible residents"});toggle("Only vanilla voices","vanilla-voices","vanillaVoicesOnly");textBlock("Authored opponents can use their own voices.",width,17);
        heading("Add a player");textBlock(label(s,"residentCandidate","Look at an eligible NPC before opening the album."),width,18);if(button("add-resident","Add player",ImGui::GetCursorScreenPos(),ImVec2(200,40)*scale,false,flag(s,"canAddResident")&&!pendingCommand))send("npc-add");
        heading("World pause");toggle("Pause in album","pause-album","pauseWorldInAlbum");heading("Experimental");textBlock("Unpaused games can be interrupted when the opponent moves or enters a scene.",width,17);toggle("Pause in shops","pause-shop","pauseWorldInShops");toggle("Pause in match setup","pause-setup","pauseWorldInMatchSetup");toggle("Pause in match","pause-match","pauseWorldDuringMatches");
    }else{
        const auto devToggle=[&](const char* name,const char* command,const char* json){const auto p=ImGui::GetCursorScreenPos();const bool value=flag(dev,json);text(p+ImVec2(0,8)*scale,name,19,palette().ink);if(button(command,value?"On":"Off",p+ImVec2(width-68*scale,0),ImVec2(68,36)*scale,value))send("development",std::string("set ")+command+" "+(value?"0":"1"));ImGui::SetCursorScreenPos(p+ImVec2(0,58)*scale);ImGui::Dummy(ImVec2(width,0));};
        heading("Collection");if(ImGui::Button("Add all cards"))send("development","cards");ImGui::SameLine();if(ImGui::Button("Add all foils"))send("development","foils");
        const auto blocked=label(member(state,"collection"),"recoveryBlocked");
        ImGui::Dummy(ImVec2(0,12*scale));
        ImGui::BeginDisabled(pendingCommand||capturing()||!member(state,"interruptedMatch").empty()||!blocked.empty());
        if(ImGui::Button("Clear collection"))confirm("Clear your collection?","Deletes every card and foil, including unavailable cards, clears discovery marks and empties all decks for this character. Your album, gold and game progress are kept. Automatic card grants and Reveal all cards will be turned off for all characters. Restore an earlier save to recover deleted cards.","collection-recovery","clear confirm","Clear collection");
        ImGui::EndDisabled();
        textBlock(blocked.empty()?"Start collecting again with an empty album. Your game progress is kept.":blocked,width,17);
        ImGui::Dummy(ImVec2(0,18*scale));
        const auto resetBlocked=label(member(state,"collection"),"resetBlocked",blocked);
        ImGui::BeginDisabled(pendingCommand||capturing()||!member(state,"interruptedMatch").empty()||!resetBlocked.empty());
        if(ImGui::Button("Reset Tessera"))confirm("Reset Tessera for this character?","Erases your collection, decks, records, reputation, tournaments and regional rule changes. Resets NPC and merchant collections, cancels card exchanges, and removes carried packs and old invitations. Keeps your album and gives eight starter cards and a starter deck. Gold and interface preferences stay. Development overrides return to defaults for all characters. Restore an earlier save to undo.","collection-recovery","reset confirm","Reset Tessera");
        ImGui::EndDisabled();
        textBlock(resetBlocked.empty()?"Start Tessera over with your album and starter cards. All Tessera progress is erased.":resetBlocked,width,17);
        heading("Tournaments");devToggle("Regular tournaments","RegularTournaments","RegularTournaments");devToggle("Invitational tournaments","InvitationalTournaments","InvitationalTournaments");devToggle("Masters tournaments","MastersTournaments","MastersTournaments");
        std::vector<std::string> holds;for(const auto& h:arrayMember(member(state,"collection"),"regions"))holds.push_back(label(h,"name"));int hold=std::clamp(number(dev,"TournamentHold",1)-1,0,9);if(!holds.empty()&&combo("dev-hold","Courier hold",hold,holds,width))send("development","set TournamentHold "+std::to_string(hold+1));
        heading("Players");devToggle("Show all players","ShowAllPlayers","ShowAllPlayers");devToggle("Unlock all players","UnlockAllPlayers","UnlockAllPlayers");devToggle("Reveal all cards","RevealAllCards","RevealAllCards");devToggle("Allow any NPC","AllowAnyNPC","AllowAnyNPC");devToggle("Record results","RecordResults","RecordResults");
        heading("Match overrides");int rules=number(dev,"Rules",-1);bool inherit=rules<0;if(ImGui::Checkbox("Use regional rules",&inherit))send("development","set Rules "+std::to_string(inherit?-1:0));
        if(!inherit)for(auto [bit,name]:ruleNames){bool value=(rules&bit)!=0;if(ImGui::Checkbox(name,&value)){const auto proposed=value?ttcg::withSpreadRule(rules,bit):ttcg::withoutCultureRule(rules,bit);send("development","set Rules "+std::to_string(proposed));}}
        int trade=number(dev,"TradeRule",-1)+1;if(combo("dev-trade","Trade rule",trade,{"Default","None","One","Diff","Direct","All"},width))send("development","set TradeRule "+std::to_string(trade-1));
        int skill=number(dev,"OpponentSkill",-1)+1;if(combo("dev-skill","Opponent skill",skill,{"Default","Learner","Regular","Expert","Master"},width))send("development","set OpponentSkill "+std::to_string(skill-1));
        std::vector<int> wagers{-1};std::vector<std::string> wagerLabels{"Default"};int wager=0;
        for(int amount:ttcg::wagerAmounts){wagers.push_back(amount);wagerLabels.push_back(std::to_string(amount));if(amount==number(dev,"Wager",-1))wager=int(wagers.size())-1;}
        if(combo("dev-wager","Wager",wager,wagerLabels,width))send("development","set Wager "+std::to_string(wagers[wager]));
        int lesson=number(dev,"RuleLesson");if(combo("dev-lesson","Rule lesson",lesson,{"Off","Three Open","Legion","Decimation","Affinity"},width))send("development","set RuleLesson "+std::to_string(lesson));if(!label(dev,"notice").empty())textBlock(label(dev,"notice"),width,18,palette().accent);
    }
    if(settingsPage==0&&!label(s,"interfaceNotice").empty())textBlock(label(s,"interfaceNotice"),width,17,palette().accent);
    ImGui::Dummy(ImVec2(1,30*scale));ImGui::EndChild();
    applySettings();
    if(settingsPage==0&&appearancePage==1){matchAppearancePreview(at+ImVec2(size.x-preview,0),ImVec2(preview,size.y));return;}
    if(settingsPage==0&&!collection.cards.empty()){
        auto p=at+ImVec2(size.x-preview,58*scale);line(p-ImVec2(gap/2,0),p+ImVec2(-gap/2,size.y-58*scale));const float w=preview-12*scale,h=w/.75f;
        if(previewBack){auto image=assets.get(customBack);ImGui::GetWindowDrawList()->AddImage(image.id,p,p+ImVec2(w,h));}else displayCard(selected,p,ImVec2(w,h),false,-1,0,0,true);
        auto buttons=p+ImVec2(0,h+22*scale);const auto step=[&](int dir){for(int i=0;i<int(collection.cards.size());++i){selected=(selected+dir+int(collection.cards.size()))%int(collection.cards.size());if(collection.cards[selected].known[0]||collection.cards[selected].owned[0])break;}};
        if(button("previous-preview","←",buttons,ImVec2(35,36)*scale))step(-1);if(button("next-preview","→",buttons+ImVec2(w-35*scale,0),ImVec2(35,36)*scale))step(1);text(buttons+ImVec2(w/2,0),collection.cards[selected].name,18,palette().muted,w-90*scale,true);
        if(button("turn-preview",previewBack?"Show front":"Show back",buttons+ImVec2((w-145*scale)/2,58*scale),ImVec2(145,38)*scale))previewBack=!previewBack;
    }
}
}
