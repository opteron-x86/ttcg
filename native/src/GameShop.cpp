#include "GameApp.h"
namespace preview {
void GameApp::shop(ImVec2 at,ImVec2 size){
    const auto& s=member(state,"shop");text(at,label(s,"name"),32,palette().accent);if(flag(s,"specialist"))text(at+ImVec2(0,44)*scale,"Specialist card dealer",18,palette().muted);
    text(at+ImVec2(size.x-360*scale,3*scale),std::to_string(number(s,"gold"))+" gold",22,palette().ink);text(at+ImVec2(size.x-360*scale,35*scale),"Merchant: "+std::to_string(number(s,"merchantGold"))+" gold",17,palette().muted);
    const char* tabs[]{"Buy","Sell","Buy back"};for(int i=0;i<3;++i)if(chapter(tabs[i],tabs[i],at+ImVec2(i*120*scale,78*scale),ImVec2(108,42)*scale,shopTab==i)){shopTab=i;shopSelected=-1;shopQuantity=1;}
    auto top=at+ImVec2(0,144)*scale;const float right=AlbumLayout::preview*scale,gap=AlbumLayout::gap*scale,width=size.x-right-gap;
    const std::vector<std::string> tiers{"All tiers","I","II","III","IV","V","VI","VII","VIII","IX","X"},rarities{"All rarities","Common","Rare","Epic","Legendary"};
    const float tierWidth=choiceWidth(tiers),rarityWidth=choiceWidth(rarities),filterX=width-tierWidth-rarityWidth-12*scale;
    ImGui::SetCursorScreenPos(top);ImGui::SetNextItemWidth(filterX-18*scale);ImGui::InputTextWithHint("##shop-search","Search cards",shopQuery,sizeof(shopQuery));
    ImGui::SetCursorScreenPos(top+ImVec2(filterX,0));select("##shop-tier",shopTier,tiers,tierWidth);
    ImGui::SetCursorScreenPos(top+ImVec2(width-rarityWidth,0));select("##shop-rarity",shopRarity,rarities,rarityWidth);
    top.y+=53*scale;if(shopTab==1){ImGui::SetCursorScreenPos(top);select("##inventory",shopSurplus,{"All cards","Surplus cards"},180*scale);if(shopSurplus)text(top+ImVec2(200,8)*scale,"Keeps two standard copies of each card.",16,palette().muted,width-200*scale);top.y+=53*scale;}
    std::vector<Json> rows;for(const auto& row:arrayMember(s,shopTab==1?"sell":"buy")){const int id=number(row,"id",-1);if(id<0||id>=int(collection.cards.size()*2))continue;const auto& card=collection.cards[id%collection.cards.size()];Json r=row;
        if(shopTab==1&&shopSurplus)r["count"]=number(row,"surplus");else if(shopTab==2){r["count"]=number(row,"buyback");r["price"]=number(row,"salePrice");}
        if(number(r,"count")<=0||shopTier&&card.tier!=shopTier||shopRarity&&card.rarity!=rarities[shopRarity])continue;
        if(shopQuery[0]&&lower(card.name+" "+join(member(state["cards"][id],"groups"))+" "+join(member(state["cards"][id],"affinities"))+(id>=int(collection.cards.size())?" foil":"")).find(lower(shopQuery))==std::string::npos)continue;rows.push_back(std::move(r));}
    std::sort(rows.begin(),rows.end(),[&](const auto& a,const auto& b){const auto& x=collection.cards[number(a,"id")%collection.cards.size()];const auto& y=collection.cards[number(b,"id")%collection.cards.size()];return x.tier!=y.tier?x.tier<y.tier:number(a,"price")!=number(b,"price")?number(a,"price")<number(b,"price"):x.name<y.name;});
    if(std::none_of(rows.begin(),rows.end(),[&](const auto& r){return number(r,"id")==shopSelected;})){shopSelected=rows.empty()?-1:number(rows.front(),"id");shopQuantity=1;}
    ImGui::SetCursorScreenPos(top);ImGui::BeginChild("shop-cards",ImVec2(width,at.y+size.y-top.y));const float spacing=17*scale;const float gridWidth=ImGui::GetContentRegionAvail().x-18*scale;const int columns=std::max(2,int(gridWidth/(180*scale)));const float w=(gridWidth-spacing*(columns-1))/columns,h=w/.75f,rowH=h+70*scale;
    ImGuiListClipper clip;clip.Begin((int(rows.size())+columns-1)/columns,rowH);int material=22;
    while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row){const auto p=ImGui::GetCursorScreenPos()+ImVec2(5,5)*scale;for(int col=0;col<columns;++col){int index=row*columns+col;if(index>=int(rows.size()))break;const auto& entry=rows[index];const int id=number(entry,"id");const auto pos=p+ImVec2(col*(w+spacing),0);ImGui::SetCursorScreenPos(pos);ImGui::PushID(id);
            if(ImGui::InvisibleButton("shop-card",ImVec2(w,h+48*scale),ImGuiButtonFlags_EnableNav)){shopSelected=id;shopQuantity=1;}if((ImGui::IsItemHovered()&&ImGui::IsMouseClicked(1))||(ImGui::IsItemFocused()&&(ImGui::IsKeyPressed(ImGuiKey_I)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp))))inspectCard(id);
            displayCard(id,pos,ImVec2(w,h),shopSelected==id,-1,material++);text(pos+ImVec2(0,h+9*scale),std::to_string(number(entry,"price"))+" gold",18,palette().ink);text(pos+ImVec2(w-57*scale,h+9*scale),"×"+std::to_string(number(entry,"count")),17,palette().muted);ImGui::PopID();}
        ImGui::SetCursorScreenPos(p+ImVec2(0,rowH-5*scale-ImGui::GetStyle().ItemSpacing.y));ImGui::Dummy(ImVec2(1,0));}
    if(rows.empty()&&shopTab!=2)text(ImGui::GetCursorScreenPos()+ImVec2(12,32)*scale,shopTab==1&&shopSurplus?"No surplus cards. Choose All cards to see your collection.":"No cards match these filters.",21,palette().muted,width-24*scale);ImGui::EndChild();
    const auto detail=at+ImVec2(size.x-right,144*scale);line(detail-ImVec2(gap/2,0),at+ImVec2(size.x-right-gap/2,size.y));
    auto chosen=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return number(r,"id")==shopSelected;});if(chosen==rows.end())return;const auto& row=*chosen;
    const float dw=std::min(right,(size.y-400*scale)*.75f),dh=dw/.75f;auto art=detail+ImVec2((right-dw)/2,0);ImGui::SetCursorScreenPos(art);if(ImGui::InvisibleButton("shop-preview",ImVec2(dw,dh),ImGuiButtonFlags_EnableNav))inspectCard(shopSelected);displayCard(shopSelected,art,ImVec2(dw,dh),false,-1,0,0,true);
    float y=art.y+dh+22*scale;const auto& card=collection.cards[shopSelected%collection.cards.size()];text(ImVec2(detail.x,y),std::to_string(card.owned[shopSelected>=int(collection.cards.size())])+" owned · Tier "+tiers[card.tier],17,palette().muted,right);y+=31*scale;
    text(ImVec2(detail.x,y),card.rarity+" · "+join(member(state["cards"][shopSelected],"groups")),18,palette().accent,right);y+=43*scale;
    const int price=number(row,"price");const int max=std::max(1,std::min({99,number(row,"count"),price?1000000/price:99}));shopQuantity=std::clamp(shopQuantity,1,max);
    if(max>1){
        const bool editable=!pendingCommand&&!flag(s,"pending");
        text(ImVec2(detail.x,y+8*scale),"Quantity",18,palette().muted);
        const auto controls=ImVec2(detail.x+right-128*scale,y);
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat,true);
        if(button("quantity-less","−",controls,ImVec2(34,34)*scale,false,editable&&shopQuantity>1))--shopQuantity;
        if(button("quantity-more","+",controls+ImVec2(94,0)*scale,ImVec2(34,34)*scale,false,editable&&shopQuantity<max))++shopQuantity;
        ImGui::PopItemFlag();
        text(controls+ImVec2(64,7)*scale,std::to_string(shopQuantity),20,palette().ink,0,true);y+=48*scale;
    }
    const bool sell=shopTab==1;const int total=price*shopQuantity;const bool enough=number(s,sell?"merchantGold":"gold")>=total;
    const auto action=std::string(sell?"Sell":shopTab==2?"Buy back":"Buy")+" · "+std::to_string(total)+" gold";
    if(button("trade",action.c_str(),ImVec2(detail.x,y),ImVec2(right,42*scale),true,enough&&!pendingCommand&&!flag(s,"pending"))){
        std::string warnings;const Json* owned=nullptr;for(const auto& r:arrayMember(s,"sell"))if(number(r,"id")==shopSelected)owned=&r;
        if(sell&&owned){if(number(*owned,"warnings")&1)warnings+="This is a unique card.\n";if(number(*owned,"warnings")&2)warnings+="This copy is foil.\n";if(number(*owned,"count")-shopQuantity<number(*owned,"reserved"))warnings+="This sale will leave an empty slot in a saved deck.\n";if(number(*owned,"lastHandAt")>0&&shopQuantity>=number(*owned,"lastHandAt"))warnings+="You won’t have enough cards for a game.";}
        const auto args=std::string(sell?"sell":shopTab==2?"buyback":"buy")+" "+std::to_string(shopSelected)+" "+std::to_string(shopQuantity);
        if(!warnings.empty())confirm("Sell "+card.name+" for "+std::to_string(total)+" gold?",warnings,"shop",args+" 1","Sell");else send("shop",args+" 0");
    }
    if(!enough||flag(s,"pending"))text(ImVec2(detail.x,y+56*scale),flag(s,"pending")?"Payment is pending. Reopen trading to retry.":sell?"The merchant doesn't have enough gold.":"You don't have enough gold.",17,palette().muted,right);
    if(!label(s,"notice").empty())text(at+ImVec2(0,size.y-10*scale),label(s,"notice"),16,palette().accent,size.x);
}
}
