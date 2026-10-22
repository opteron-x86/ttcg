#include "App.h"
#include <cstdio>

namespace preview {
void App::refreshFilter() {
    filter.text=query;visible=filter.results(collection.cards,collection.connected);
}
void App::cardGrid(ImVec2 at,ImVec2 size,bool changed) {
    ImGui::SetCursorScreenPos(at-ImVec2(5,0)*scale);
    ImGui::BeginChild("catalog",ImVec2(size.x+10*scale,size.y),ImGuiChildFlags_None);
    if(changed)ImGui::SetScrollY(0);
    const float gap=18*scale,content=ImGui::GetContentRegionAvail().x-24*scale;
    const float target=screen==3?std::array<float,3>{185,116,86}[deckDensity]:185.f;
    const int columns=std::clamp(int((content+gap)/(target*scale+gap)),2,10);
    const float w=(content-gap*(columns-1))/columns,h=w/.75f,rowH=h+gap;
    std::vector<unsigned> packs;
    if(collection.connected&&screen==0)for(unsigned kind=0;kind<2;++kind)if(collection.packs[kind]>0)packs.push_back(kind);
    const int count=int(visible.size()+packs.size());
    ImGuiListClipper clip;clip.Begin((count+columns-1)/columns,rowH);
    int material=MaterialSlot::grid;
    while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
        const auto origin=ImGui::GetCursorScreenPos()+ImVec2(6,6)*scale;
        for(int col=0;col<columns;++col) {
            const int item=row*columns+col;if(item>=count)break;
            const ImVec2 p=origin+ImVec2(col*(w+gap),0);
            if(item<int(packs.size())) {
                const unsigned kind=packs[item];ImGui::PushID(kind==0?"pack-standard":"pack-tournament");ImGui::SetCursorScreenPos(p);
                if(ImGui::InvisibleButton("pack",ImVec2(w,h),ImGuiButtonFlags_EnableNav))selectedPack=int(kind);
                packTile(kind,p,ImVec2(w,h),selectedPack==int(kind)||(ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible));
                ImGui::PopID();continue;
            }
            const int display=visible[item-packs.size()],id=display%int(collection.cards.size());const bool foil=display>=int(collection.cards.size());
            const auto& c=collection.cards[id];const bool known=!collection.connected||c.known[foil?1:0]||c.owned[foil?1:0]>0;
            ImGui::PushID(display);ImGui::SetCursorScreenPos(p);
            if(ImGui::InvisibleButton("card",ImVec2(w,h),ImGuiButtonFlags_EnableNav)) {selected=id;selectedFoil=foil;selectedPack=-1;}
            if(known&&((ImGui::IsItemHovered()&&(ImGui::IsMouseDoubleClicked(0)||ImGui::IsMouseClicked(1)))||(ImGui::IsItemFocused()&&(ImGui::IsKeyPressed(ImGuiKey_I)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp))))) inspectCard(display,false);
            if(known&&collection.connected&&collection.writable&&!collection.busy&&c.owned[foil?1:0]>0&&ImGui::BeginDragDropSource()) {
                const auto key=c.id|(foil?ttcg::foilFlag:0);ImGui::SetDragDropPayload("TESSERA_CARD",&key,sizeof(key));
                dragCard(id,foil);ImGui::EndDragDropSource();
            }
            const bool chosen=(id==selected&&foil==selectedFoil&&(screen!=0||selectedPack<0))||(ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible);
            if(known)card(id,p,ImVec2(w,h),chosen,foil&&face.foil,-1,material++);
            else {
                auto texture=assets.get(customBack);ImGui::GetWindowDrawList()->AddImage(texture.id,p,p+ImVec2(w,h));
                if(chosen)ImGui::GetWindowDrawList()->AddRect(p-ImVec2(3,3),p+ImVec2(w+3,h+3),palette().accent);
            }
            if(collection.connected&&c.owned[foil?1:0]>0) {
                const auto label="×"+std::to_string(c.owned[foil?1:0]);const float unit=w/180.f,pixels=14*unit;
                const auto measure=font()->CalcTextSizeA(pixels,1e5f,0,label.c_str());const auto extent=measure+ImVec2(10,6)*unit;
                const auto badge=p+ImVec2(w-5*unit-extent.x,h*.75f-extent.y);
                auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(badge,badge+extent,IM_COL32(18,29,24,182),unit);
                draw->AddRect(badge,badge+extent,IM_COL32(187,166,116,110),unit);
                draw->AddText(font(),pixels,badge+ImVec2(5,3)*unit,IM_COL32(239,228,204,255),label.c_str());
            }
            ImGui::PopID();
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x-6*scale,origin.y-6*scale+rowH-ImGui::GetStyle().ItemSpacing.y));ImGui::Dummy(ImVec2(0,0));
    }
    if(count==0)text(ImGui::GetCursorScreenPos()+ImVec2(10,30)*scale,"No matching cards",24,palette().muted);
    ImGui::EndChild();
}
void App::dragCard(int index,bool foil,int owner,CardContext context) {
    // This is the drag tooltip's own draw list, so the preview stays above the
    // destination and is not clipped by the source grid's scrolling region.
    const auto at=ImGui::GetCursorScreenPos()+ImVec2(6,6)*scale;
    const ImVec2 size=ImVec2(138,184)*scale;
    ImGui::Dummy(size+ImVec2(12,12)*scale);
    card(index,at,size,false,foil&&face.foil,owner,MaterialSlot::drag,false,context);
    dragPreviewDrawn=true;
}
void App::decks(ImVec2 at,ImVec2 size) {
    const bool editable=collection.ready&&collection.writable&&!collection.busy;
    const float side=AlbumLayout::navigation*scale,right=AlbumLayout::preview*scale,gap=AlbumLayout::gap*scale;
    const ImVec2 mid=at+ImVec2(side+gap,0);const float width=size.x-side-right-2*gap;
    line(at+ImVec2(side+gap*.5f,0),at+ImVec2(side+gap*.5f,size.y));
    if(button("new-deck","+  New deck",at,ImVec2(side,40*scale),false,editable&&collection.decks.size()<collection.maxDecks)) {
        deckDialog="new";std::snprintf(deckName,sizeof(deckName),"Deck %zu",collection.decks.size()+1);
    }
    ImGui::SetCursorScreenPos(at+ImVec2(0,60)*scale);ImGui::BeginChild("saved-decks",ImVec2(side,size.y-60*scale));
    const float listWidth=side-20*scale;
    for(const auto& deck:collection.decks) {
        const auto pos=ImGui::GetCursorScreenPos();const auto count=std::count_if(deck.cards.begin(),deck.cards.end(),[](auto id){return id!=0;});
        ImGui::SetCursorScreenPos(pos);const bool hit=ImGui::InvisibleButton(("saved"+std::to_string(deck.id)).c_str(),ImVec2(listWidth,119*scale),ImGuiButtonFlags_EnableNav);
        auto* d=ImGui::GetWindowDrawList();const bool active=deck.id==collection.activeDeck;
        if(active||ImGui::IsItemHovered()||ImGui::IsItemFocused()){const auto colour=(palette().selection&0xffffff)|0x60000000;d->AddRectFilledMultiColor(pos,pos+ImVec2(listWidth,119*scale),colour,colour&0xffffff,colour&0xffffff,colour);}
        line(pos,pos+ImVec2(0,119*scale),active?palette().accent:palette().line);
        text(pos+ImVec2(10,8)*scale,deck.name,20,palette().ink,listWidth-20*scale);
        const float thumb=(listWidth-32*scale)/5;
        for(int i=0;i<5;++i){const int index=collection.index(deck.cards[i]);const auto p=pos+ImVec2(10*scale+i*(thumb+3*scale),40*scale);
            if(index>=0){const auto texture=assets.get(collection.cards[index].thumbnail);d->AddImage(texture.id,p,p+ImVec2(thumb,thumb/.75f));}
            else d->AddRect(p,p+ImVec2(thumb,thumb/.75f),palette().line);
        }
        text(pos+ImVec2(10,98)*scale,std::to_string(count)+" / 5",14,palette().muted);
        if(hit&&editable)request({{"action","select"},{"deckID",deck.id}});
        ImGui::SetCursorScreenPos(pos+ImVec2(0,132)*scale);ImGui::Dummy(ImVec2(1,1));
    }
    ImGui::EndChild();
    auto active=std::find_if(collection.decks.begin(),collection.decks.end(),[&](const auto& d){return d.id==collection.activeDeck;});
    if(renamingDeck&&renamingDeckID!=collection.activeDeck)renamingDeck=false;
    const float titleWidth=width-200*scale;
    if(renamingDeck){
        ImGui::SetCursorScreenPos(mid);ImGui::SetNextItemWidth(titleWidth);
        if(focusDeckName){ImGui::SetKeyboardFocusHere();focusDeckName=false;}
        const bool enter=ImGui::InputText("##deck-title",deckName,sizeof(deckName),ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll);
        const bool cancel=ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false),blur=ImGui::IsItemDeactivated();
        if(cancel)renamingDeck=false;
        else if(enter||blur){
            if(editable&&active!=collection.decks.end()&&ttcg::validDeckName(deckName)&&active->name!=deckName)request({{"action","rename"},{"deckID",renamingDeckID},{"name",deckName}});
            renamingDeck=false;
        }
    }else{
        ImGui::SetCursorScreenPos(mid);const bool rename=ImGui::InvisibleButton("rename",ImVec2(titleWidth,38*scale),ImGuiButtonFlags_EnableNav);
        const auto title=active==collection.decks.end()?"Your deck":active->name;
        auto* d=ImGui::GetWindowDrawList();d->PushClipRect(mid,mid+ImVec2(titleWidth,38*scale),true);text(mid+ImVec2(0,3)*scale,title,28,palette().ink);d->PopClipRect();
        if(editable){
            const bool hover=ImGui::IsItemHovered(),focus=ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible;
            itemTooltip("Rename deck");
            if(hover||focus){
                const float titleSize=std::min(titleWidth,font()->CalcTextSizeA(28*scale,1e5f,0,title.c_str()).x);
                line(mid+ImVec2(0,35*scale),mid+ImVec2(titleSize,35*scale),palette().muted);
                if(hover)ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
            }
        }
        if(rename&&editable&&active!=collection.decks.end()){renamingDeck=true;focusDeckName=true;renamingDeckID=collection.activeDeck;std::snprintf(deckName,sizeof(deckName),"%s",active->name.c_str());}
    }
    float x=mid.x+width-169*scale;
    for(const auto& item:std::array<std::pair<const char*,const char*>,2>{{{"copy","Copy"},{"delete","Delete"}}}) {
        const bool enabled=editable&&(std::string_view(item.first)!="delete"||collection.decks.size()>1)&&
            (std::string_view(item.first)!="copy"||collection.decks.size()<collection.maxDecks);
        if(button(item.first,item.second,ImVec2(x,mid.y),ImVec2(79,34)*scale,false,enabled)) {
            deckDialog=item.first;std::snprintf(deckName,sizeof(deckName),"%s",active==collection.decks.end()?"Deck":active->name.c_str());
            if(deckDialog=="copy")std::snprintf(deckName,sizeof(deckName),"Copy of %s",active==collection.decks.end()?"Deck":active->name.c_str());
        }
        x+=90*scale;
    }
    const float slotWidth=(width-4*14*scale)/5,w=std::min(slotWidth,135*scale),h=w/.75f;
    for(int slot=0;slot<5;++slot) {
        const auto pos=mid+ImVec2(slot*(slotWidth+14*scale)+(slotWidth-w)/2,60*scale);const int index=collection.index(collection.deck[slot]);
        ImGui::PushID(slot);ImGui::SetCursorScreenPos(pos);
        if(ImGui::InvisibleButton("slot",ImVec2(w,h),ImGuiButtonFlags_EnableNav))deckSlot=deckSlot==slot?-1:slot;
        if(index>=0&&((ImGui::IsItemHovered()&&(ImGui::IsMouseDoubleClicked(0)||ImGui::IsMouseClicked(1)))||(ImGui::IsItemFocused()&&(ImGui::IsKeyPressed(ImGuiKey_I)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp))))) {
            inspectCard(index+(ttcg::isFoil(collection.deck[slot])?collection.cards.size():0),false);
        }
        if(index>=0&&editable&&ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("TESSERA_SLOT",&slot,sizeof(slot));dragCard(index,ttcg::isFoil(collection.deck[slot]));ImGui::EndDragDropSource();
        }
        if(ImGui::BeginDragDropTarget()) {
            if(const auto* payload=ImGui::AcceptDragDropPayload("TESSERA_CARD"))if(payload->DataSize==sizeof(ttcg::CardID)) {
                const auto id=*static_cast<const ttcg::CardID*>(payload->Data);
                if(collection.canUse(id,slot)) {auto hand=collection.deck;hand[slot]=id;deckSlot=slot;request({{"action","deck"},{"cards",hand}});}
                else collection.notice="That card cannot fill this slot. Check your owned copies and the deck limit.";
            }
            if(const auto* payload=ImGui::AcceptDragDropPayload("TESSERA_SLOT"))if(payload->DataSize==sizeof(int)&&editable) {
                const int source=*static_cast<const int*>(payload->Data);
                if(source>=0&&source<5&&source!=slot){auto hand=collection.deck;std::swap(hand[source],hand[slot]);deckSlot=slot;request({{"action","deck"},{"cards",hand}});}
            }
            ImGui::EndDragDropTarget();
        }
        if(index>=0)card(index,pos,ImVec2(w,h),deckSlot==slot,ttcg::isFoil(collection.deck[slot])&&face.foil,-1,1+slot);
        else {
            ImGui::GetWindowDrawList()->AddRect(pos,pos+ImVec2(w,h),deckSlot==slot?palette().accent:palette().line);
            text(pos+ImVec2(w*.5f,h*.5f-12*scale),collection.deck[slot]?"Unavailable":"+",24,palette().muted,0,true);
        }
        ImGui::PopID();
    }
    // One reserved strip: selecting a slot never changes the collection layout.
    const float y=mid.y+60*scale+h+16*scale;
    if(deckSlot>=0) {
        text(ImVec2(mid.x,y+5*scale),"Slot "+std::to_string(deckSlot+1),17,palette().muted);
        if(button("slot-left","←",ImVec2(mid.x+width-220*scale,y),ImVec2(40,32)*scale,false,editable&&deckSlot>0)) {
            auto hand=collection.deck;std::swap(hand[deckSlot],hand[deckSlot-1]);--deckSlot;request({{"action","deck"},{"cards",hand}});
        }
        if(button("slot-right","→",ImVec2(mid.x+width-170*scale,y),ImVec2(40,32)*scale,false,editable&&deckSlot<4)) {
            auto hand=collection.deck;std::swap(hand[deckSlot],hand[deckSlot+1]);++deckSlot;request({{"action","deck"},{"cards",hand}});
        }
        if(button("slot-remove","Remove",ImVec2(mid.x+width-110*scale,y),ImVec2(110,32)*scale,false,editable&&collection.deck[deckSlot]!=0)) {
            auto hand=collection.deck;hand[deckSlot]=0;request({{"action","deck"},{"cards",hand}});
        }
    }
    std::array<int,ttcg::creatureGroups.size()> groups{};std::array<int,5> affinities{};
    for(auto id:collection.deck){const auto i=collection.index(id);if(i<0)continue;const auto& c=collection.cards[i];for(unsigned g=0;g<groups.size();++g)if(c.groups&(1ull<<g))++groups[g];for(int a=0;a<5;++a)if(c.affinityMask&(1u<<a))++affinities[a];}
    std::string groupSummary;
    for(unsigned g=0;g<groups.size();++g)if(groups[g]){if(!groupSummary.empty())groupSummary+=" · ";groupSummary+=std::string(ttcg::creatureGroups[g])+" "+std::to_string(groups[g]);}
    const int affinityCount=int(std::count_if(affinities.begin(),affinities.end(),[](int count){return count>0;}));
    const float groupWidth=width-(affinityCount?affinityCount*39+25:0)*scale;
    const auto measure=font()->CalcTextSizeA(14*scale,1e5f,groupWidth,groupSummary.c_str());
    const ImVec2 summary(mid.x,y+45*scale);
    text(summary+ImVec2(0,3)*scale,groupSummary,14,palette().muted,groupWidth);
    if(affinityCount){
        float x=summary.x+std::min(groupWidth,measure.x)+25*scale;
        line(ImVec2(x-13*scale,summary.y+2*scale),ImVec2(x-13*scale,summary.y+19*scale));
        for(int a=0;a<5;++a)if(affinities[a]){
            const ImVec2 pos(x,summary.y);ImGui::SetCursorScreenPos(pos);
            ImGui::InvisibleButton(("deck-affinity-"+std::to_string(a)).c_str(),ImVec2(35,22)*scale);
            itemTooltip(std::string(ttcg::attributeNames[a])+": "+std::to_string(affinities[a]));
            auto icon=assets.get("../icons/affinity-"+std::to_string(a)+".png");
            ImGui::GetWindowDrawList()->AddImage(icon.id,pos,pos+ImVec2(20,20)*scale);
            text(pos+ImVec2(23,3)*scale,std::to_string(affinities[a]),14,palette().muted);x+=39*scale;
        }
    }
    const float summaryBottom=summary.y+std::max(22*scale,measure.y+6*scale);
    line(ImVec2(mid.x,summaryBottom+10*scale),ImVec2(mid.x+width,summaryBottom+10*scale));
    const auto filters=ImVec2(mid.x,summaryBottom+26*scale);
    pickerFilters(filters,width);
    const auto grid=filters+ImVec2(0,86*scale);
    cardGrid(grid,ImVec2(width,at.y+size.y-grid.y));
    line(at+ImVec2(size.x-right-gap*.5f,0),at+ImVec2(size.x-right-gap*.5f,size.y));
    drawDetail(at+ImVec2(size.x-right,0),ImVec2(right,size.y));
}
void App::pickerFilters(ImVec2 at,float width){
    ImGui::SetCursorScreenPos(at);ImGui::SetNextItemWidth(width);
    if(ImGui::InputTextWithHint("##deck-search","Search cards",query,sizeof(query)))refreshFilter();
}
void App::deckModal() {
    if(!deckDialog.empty()&&!ImGui::IsPopupOpen("Deck"))ImGui::OpenPopup("Deck");
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize*.5f,ImGuiCond_Always,ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(ImVec2(470,210)*scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(24,24)*scale);
    if(ImGui::BeginPopupModal("Deck",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar|popupFlags())) {
        popupSurface();
        const bool remove=deckDialog=="delete";
        ImGui::TextUnformatted(remove?"Delete this saved deck?":deckDialog=="new"?"New deck":deckDialog=="copy"?"Copy deck":"Rename deck");
        ImGui::Dummy(ImVec2(1,14*scale));
        bool enter=false;
        if(remove)ImGui::TextUnformatted("Your cards stay in the album.");
        else {ImGui::SetNextItemWidth(-1);if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();enter=ImGui::InputText("##deck-name",deckName,sizeof(deckName),ImGuiInputTextFlags_EnterReturnsTrue);}
        ImGui::Dummy(ImVec2(1,12*scale));
        const auto pos=ImGui::GetCursorScreenPos();
        if(button("cancel","Cancel",pos,ImVec2(180,39)*scale)||ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false)) {
            deckDialog.clear();ImGui::CloseCurrentPopup();
        } else if(button("save-deck",remove?"Delete":"Save",pos+ImVec2(215,0)*scale,ImVec2(180,39)*scale,true,
                   !collection.busy&&(remove||ttcg::validDeckName(deckName)))||enter) {
            if(remove||ttcg::validDeckName(deckName)) {request({{"action",deckDialog},{"name",deckName}});deckDialog.clear();ImGui::CloseCurrentPopup();}
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}
}
