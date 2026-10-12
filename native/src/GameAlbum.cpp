#include "GameApp.h"
#include <bit>
namespace preview {
void GameApp::nativeFilters(ImVec2 at,float width,bool editing){
    const int page=editing?1:0;
    if(filterPage!=page){if(filterPage==0)cardFilter=filter;else if(filterPage==1)deckFilter=filter;filter=editing?deckFilter:cardFilter;std::snprintf(query,sizeof(query),"%s",filter.text.c_str());filterPage=page;filterDirty=true;refreshFilter();}
    const std::vector<std::string> ownership{"Owned","Discovered","Missing","Extras"},tiers{"Tier","I","II","III","IV","V","VI","VII","VIII","IX","X"},sorts{"Low to high","High to low","Name","Quantity","Rarity"};
    const float ownW=choiceWidth(ownership),tierW=choiceWidth(tiers),sortW=choiceWidth(sorts),searchW=width-ownW-tierW-sortW-120*scale;
    ImGui::SetCursorScreenPos(at);ImGui::SetNextItemWidth(searchW);ImGui::InputTextWithHint("##search","Search cards",query,sizeof(query));
    float x=searchW+10*scale;
    ImGui::SetCursorScreenPos(at+ImVec2(x,0));select("##ownership",filter.ownership,ownership,ownW);x+=ownW+10*scale;
    ImGui::SetCursorScreenPos(at+ImVec2(x,0));select("##tier",filter.tier,tiers,tierW);x+=tierW+10*scale;
    ImGui::SetCursorScreenPos(at+ImVec2(x,0));select("##sort",filter.sort,sorts,sortW);
    if(button("filters","Filters",at+ImVec2(width-80*scale,0),ImVec2(80,38)*scale))ImGui::OpenPopup("Card filters");
    ImGui::SetNextWindowSize(ImVec2(370*scale,0));ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(20,20)*scale);
    if(ImGui::BeginPopup("Card filters")){
        std::vector<std::string> groups{"All groups"};for(auto name:ttcg::creatureGroups)groups.emplace_back(name);int group=filter.group+1;
        if(combo("##group","Group",group,groups,330*scale))filter.group=group-1;
        combo("##rarity","Rarity",filter.rarity,{"All rarities","Common","Rare","Epic","Legendary"},330*scale);
        combo("##finish","Finish",filter.finish,{"All finishes","Standard","Foil"},330*scale);
        int affinity=filter.affinity+1;if(combo("##affinity","Affinity",affinity,{"All affinities","Neutral","Strength","Intelligence","Willpower","Agility","Endurance"},330*scale))filter.affinity=affinity-1;
        std::vector<std::string> sets{"All sets","Tessera"},keys{"","tessera"};
        for(const auto& c:collection.cards)if(!c.setKey.empty()&&std::find(keys.begin(),keys.end(),c.setKey)==keys.end()){keys.push_back(c.setKey);sets.push_back(c.set);}
        int set=int(std::find(keys.begin(),keys.end(),filter.set)-keys.begin());if(set>=int(keys.size()))set=0;
        if(combo("##set","Set",set,sets,330*scale))filter.set=keys[set];
        if(ImGui::Button("Reset filters")){filter=Filter{};query[0]=0;}ImGui::EndPopup();
    }ImGui::PopStyleVar();
    filter.text=query;
    if(editing){
        const char* ids[]{"view-large","view-medium","view-small"};
        const char* labels[]{"Large cards","Medium cards","Small cards"};
        for(int i=0;i<3;++i){
            const auto pos=at+ImVec2(i*40.f,47)*scale;
            if(button(ids[i],"",pos,ImVec2(34,32)*scale,deckDensity==i))deckDensity=i;
            itemTooltip(labels[i]);
            const int columns=i+1;const float gap=2*scale,unit=(18*scale-gap*i)/columns;
            const auto origin=pos+ImVec2(8,7)*scale;
            for(int row=0;row<columns;++row)for(int col=0;col<columns;++col){
                const auto cell=origin+ImVec2(col*(unit+gap),row*(unit+gap));
                ImGui::GetWindowDrawList()->AddRectFilled(cell,cell+ImVec2(unit,unit),deckDensity==i?palette().accent:palette().muted,scale*.5f);
            }
        }
    }
    const auto countLabel=[&]{textRight(at+ImVec2(width-14*scale,56*scale),std::to_string(visible.size())+" cards",16,palette().muted);};
    if(!filterDirty&&filter==appliedFilter){countLabel();return;}
    appliedFilter=filter;filterDirty=false;visible.clear();const int count=int(collection.cards.size());
    for(int id=0;id<count*2;++id){const auto& c=collection.cards[id%count];const bool foil=id>=count;const int n=c.owned[foil];
        if(!c.known[foil]&&n<=0)continue;
        Filter f=filter;f.ownership=1;f.text.clear();if(!f.matches(c,foil,true))continue;
        if(filter.ownership==0&&!n||filter.ownership==2&&n)continue;
        if(filter.ownership==3&&(foil||n<=std::max(1,itemNumber(member(member(state,"collection"),"deckReserved"),id,0))))continue;
        if(filter.affinity==0&&c.affinityMask||filter.affinity>0&&!(c.affinityMask&(1u<<(filter.affinity-1))))continue;
        if(!filter.set.empty()&&(c.setKey.empty()?"tessera":c.setKey)!=filter.set)continue;
        if(query[0]){const auto& raw=state["cards"][id];const auto hay=lower(c.name+" "+c.rarity+" "+join(member(raw,"groups"))+" "+join(member(raw,"subtypes"))+" "+join(member(raw,"affinities")));if(hay.find(lower(query))==std::string::npos)continue;}
        visible.push_back(id);
    }
    std::stable_sort(visible.begin(),visible.end(),[&](int a,int b){const auto& x=collection.cards[a%count];const auto& y=collection.cards[b%count];int order=0;
        if(filter.sort<=1)order=x.tier-y.tier;else if(filter.sort==3)order=x.owned[a/count]-y.owned[b/count];else if(filter.sort==4){const std::vector<std::string> rarity{"Common","Rare","Epic","Legendary"};order=int(std::find(rarity.begin(),rarity.end(),x.rarity)-std::find(rarity.begin(),rarity.end(),y.rarity));}
        if(order)return filter.sort==1||filter.sort==3?order>0:order<0;return x.name!=y.name?x.name<y.name:a<b;
    });
    countLabel();
}
void GameApp::nativeAlbum(ImVec2 at,ImVec2 size){
    const float right=AlbumLayout::preview*scale,gap=AlbumLayout::gap*scale,width=size.x-right-gap;
    nativeFilters(at,width);cardGrid(at+ImVec2(0,86)*scale,ImVec2(width,size.y-86*scale));
    line(at+ImVec2(width+gap*.5f,0),at+ImVec2(width+gap*.5f,size.y));drawDetail(at+ImVec2(width+gap,0),ImVec2(right,size.y));
    int owned=0;std::array<int,2> known{},total{};for(const auto& c:collection.cards){const int set=c.setKey.empty()?0:1;owned+=c.owned[0]+c.owned[1]>0;known[set]+=c.known[0]||c.known[1];++total[set];}
    const auto progress=total[1]?"Tessera "+std::to_string(known[0])+"/"+std::to_string(total[0])+" · Expansions "+std::to_string(known[1])+"/"+std::to_string(total[1]):std::to_string(owned)+" owned · "+std::to_string(known[0])+"/"+std::to_string(total[0])+" discovered";
    textRight(at+ImVec2(size.x,-55*scale),progress,16,palette().muted);
}
void GameApp::nativeDecks(ImVec2 at,ImVec2 size){
    if(!member(state,"interruptedMatch").is_null()){text(at,"Finish your current game to edit decks.",22,palette().muted);return;}
    decks(at,size);
}
void GameApp::drawDetail(ImVec2 at,ImVec2 size){
    if(selectedPack>=0&&tab==0){packDetail(at,size);return;}
    if(collection.cards.empty()||selected<0)return;
    const auto& c=collection.cards[selected];const bool known=c.known[selectedFoil]||c.owned[selectedFoil];
    const float w=std::min(size.x-12*scale,(size.y-235*scale)*.75f),h=w/.75f;const auto p=at+ImVec2((size.x-w)/2,8*scale);
    ImGui::SetCursorScreenPos(p);if(ImGui::InvisibleButton("selected-art",ImVec2(w,h),ImGuiButtonFlags_EnableNav)&&known)inspectCard(selected+(selectedFoil?collection.cards.size():0),false);
    if(known)card(selected,p,ImVec2(w,h),false,selectedFoil,-1,0,true);else displayCard(-2,p,ImVec2(w,h));
    if(!known)return;
    auto mid=at.x+size.x/2;float y=p.y+h+22*scale;
    text(ImVec2(mid,y),std::to_string(c.owned[selectedFoil])+" owned · Tier "+std::array<const char*,11>{"","I","II","III","IV","V","VI","VII","VIII","IX","X"}[c.tier],16,palette().muted,0,true);y+=33*scale;
    text(ImVec2(mid,y),c.rarity+(selectedFoil?" · Foil":""),19,c.rarity=="Rare"?IM_COL32(86,223,255,255):c.rarity=="Epic"?IM_COL32(214,132,250,255):palette().accent,0,true);y+=36*scale;
    text(ImVec2(mid,y),join(member(state["cards"][selected],"groups")),18,palette().muted,size.x,true);y+=30*scale;
    text(ImVec2(mid,y),c.affinityMask?join(member(state["cards"][selected],"affinities")):"Neutral",18,palette().muted,size.x,true);y+=47*scale;
    if(tab==1){int slot=deckSlot;if(slot<0)for(int i=0;i<5;++i)if(!collection.deck[i]){slot=i;break;}
        if(slot<0)return;
        const auto id=c.id|(selectedFoil?ttcg::foilFlag:0);const auto title=collection.deck[slot]?"Replace slot "+std::to_string(slot+1):"Add to deck";
        if(button("add",title.c_str(),ImVec2(at.x,y),ImVec2(size.x,40*scale),true,collection.canUse(id,slot)&&!pendingCommand)){auto hand=collection.deck;hand[slot]=id;request({{"action","deck"},{"cards",hand}});}
    }else if(button("deck-card","Use in deck",ImVec2(at.x,y),ImVec2(size.x,40*scale),false,collection.writable&&c.owned[selectedFoil]>0)){tab=1;screen=3;selectedPack=-1;}
}
}
