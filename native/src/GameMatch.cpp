#include "GameApp.h"
#include "Presentation.h"
#include "Announcements.h"
#include <bit>
#include <cmath>
#include <numbers>
namespace preview {
bool GameApp::ruleRequestModal(){
    const auto& c=member(state,"collection");const auto& culture=member(c,"culture");
    const int offer=number(culture,"offerRule"),request=number(c,"ruleRequest");
    if(label(state,"screen")!="lobby"||label(state,"phase")!="ready"||(!offer&&!request)){ruleAnswerPending=false;return false;}
    std::string copy=offer?"You brought "+rulesLabel(offer,false)+" from "+label(culture,"source")+". Shall we try it?":"Can we play with "+rulesLabel(request,false)+"?";
    if(offer){if(const auto replaced=number(culture,"replaces"))copy+=" It replaces "+rulesLabel(replaced,false)+" for this game.";copy+=" If it catches on, local players may adopt it.";}
    const float width=560*scale,content=width-64*scale;
    const auto wrap=[&](const std::string& value,float pixels){
        std::vector<std::string> lines;const char* begin=value.c_str();const char* end=begin+value.size();
        while(begin<end){const char* stop=font()->CalcWordWrapPosition(pixels*scale,begin,end,content);if(stop<=begin)stop=end;
            lines.emplace_back(begin,stop);begin=stop;while(begin<end&&(*begin==' '||*begin=='\n'))++begin;}
        return lines;
    };
    const auto speaker=wrap(label(state,"opponent"),26),message=wrap(copy,23),notice=wrap(collection.notice,16);
    const float noticeHeight=notice.empty()?0:(18+notice.size()*21)*scale;
    const ImVec2 extent(width,(36+speaker.size()*32+18+message.size()*29+28+42+36)*scale+noticeHeight);
    if(!ImGui::IsPopupOpen("Rule request"))ImGui::OpenPopup("Rule request");
    ImGui::SetNextWindowPos(frameAt+frameSize*.5f,ImGuiCond_Always,ImVec2(.5f,.5f));ImGui::SetNextWindowSize(extent);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(32,36)*scale);
    if(ImGui::BeginPopupModal("Rule request",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar)){
        const auto at=ImGui::GetWindowPos();surface(at,extent);ImGui::GetWindowDrawList()->AddRect(at,at+extent,palette().accent);
        float y=at.y+36*scale;
        for(const auto& line:speaker){text(ImVec2(at.x+width/2,y),line,26,palette().accent,0,true);y+=32*scale;}y+=18*scale;
        for(const auto& line:message){text(ImVec2(at.x+width/2,y),line,23,palette().ink,0,true);y+=29*scale;}
        if(!notice.empty()){y+=18*scale;for(const auto& line:notice){text(ImVec2(at.x+width/2,y),line,16,palette().muted,0,true);y+=21*scale;}}
        const bool ready=!pendingCommand&&!ruleAnswerPending;
        const auto answer=[&](bool accept){send(offer?"culture":"terms",offer?std::string("trial ")+(accept?"1":"0"):"rule "+std::to_string(accept?request:0));ruleAnswerPending=true;};
        const auto buttons=ImVec2(at.x+(width-320*scale)/2,y+28*scale);
        if(button("accept-rule","Yes",buttons,ImVec2(150,42)*scale,true,ready))answer(true);
        if(ImGui::IsWindowAppearing()){ImGui::SetItemDefaultFocus();ImGui::SetNavCursorVisible(true);}
        if(button("decline-rule","No",buttons+ImVec2(170,0)*scale,ImVec2(150,42)*scale,false,ready))answer(false);
        // Escape leaves the conversation, as in the original dialogue; it is
        // not an answer to the rule offer. Wait for the backend after a choice.
        const bool back=escapeRequested||ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false);escapeRequested=false;
        if(back&&ready){send("close");ruleAnswerPending=true;}
        ImGui::EndPopup();
    }ImGui::PopStyleVar();
    return true;
}
void GameApp::lobby(ImVec2 at,ImVec2 size){
    const auto& c=member(state,"collection");text(at,label(state,"opponent"),35,palette().ink);
    const auto subtitle=label(c,"hold")+" · "+(number(c,"tournamentID")?"Tournament":label(c,"skill"));
    text(at+ImVec2(0,46)*scale,subtitle,18,palette().muted,size.x);
    const float ruleX=font()->CalcTextSizeA(18*scale,1e5f,0,subtitle.c_str()).x+16*scale;
    const float rulesHeight=ruleChips(at+ImVec2(ruleX,41*scale),size.x-ruleX,number(state,"rules"),number(c,"tournamentID")?-1:number(c,"fixedTrade"),flag(c,"practice"),true);
    float y=at.y+std::max(100*scale,62*scale+rulesHeight);
    const auto& culture=member(c,"culture");const int offer=number(culture,"offerRule");
    text(ImVec2(at.x,y+7*scale),"Deck",18,palette().muted);ImGui::SetCursorScreenPos(ImVec2(at.x+55*scale,y));ImGui::SetNextItemWidth(295*scale);
    std::vector<std::string> decks;int active=0;for(const auto& deck:collection.decks){if(deck.id==collection.activeDeck)active=int(decks.size());decks.push_back(deck.name);}
    if(select("##lobby-deck",active,decks,295*scale))request({{"action","select"},{"deckID",collection.decks[active].id}});
    if(button("edit-deck","Edit deck",ImVec2(at.x+size.x-125*scale,y),ImVec2(125,38)*scale)){tab=1;send("browse","album");}
    y+=72*scale;const float gap=22*scale,w=std::min((size.x-4*gap)/5,(at.y+size.y-y-114*scale)*.75f),h=w/.75f;
    const float left=at.x+(size.x-(w*5+gap*4))/2;
    for(int i=0;i<5;++i){const auto p=ImVec2(left+i*(w+gap),y);const int id=itemNumber(member(c,"deck"),i);ImGui::PushID(i);ImGui::SetCursorScreenPos(p);
        if(ImGui::InvisibleButton("setup-card",ImVec2(w,h),ImGuiButtonFlags_EnableNav)){if(id>=0)inspectCard(id);else{tab=1;deckSlot=i;send("browse","album");}}
        if(id>=0&&ImGui::IsItemHovered()&&ImGui::IsMouseClicked(1))inspectCard(id);
        if(id>=0)displayCard(id,p,ImVec2(w,h),false,-1,1+i);else{ImGui::GetWindowDrawList()->AddRect(p,p+ImVec2(w,h),palette().line);text(p+ImVec2(w/2,h/2),"+",30,palette().muted,0,true);}text(p+ImVec2(0,h+12*scale),std::to_string(i+1),13,palette().muted);ImGui::PopID();}
    const auto footer=at+ImVec2(0,size.y-73*scale);line(footer,footer+ImVec2(size.x,0));
    if(button("album","Album",footer+ImVec2(0,23)*scale,ImVec2(95,38)*scale)){tab=0;send("browse","album");}
    if(button("lobby-rules","Rules",footer+ImVec2(105,23)*scale,ImVec2(95,38)*scale)){showRuleReference=true;topic="play";demoStep=0;}
    if(flag(culture,"canAbolish")){
        if(button("abolish-menu","Challenge a rule",footer+ImVec2(210,23)*scale,ImVec2(165,38)*scale))ImGui::OpenPopup("Abolish a rule");
        if(ImGui::BeginPopup("Abolish a rule")){const unsigned allowed=number(culture,"abolishableRules");for(auto [bit,name]:ruleNames)if(allowed&bit)if(ImGui::Selectable(name)){confirm("Challenge "+std::string(name)+"?","Win to try abolishing this rule in the hold.","culture","abolish "+std::to_string(bit),"Challenge");}ImGui::EndPopup();}
    }
    if(number(c,"ruleLesson")||number(member(member(state,"settings"),"development"),"RuleLesson"))if(button("lesson","Practice rule",footer+ImVec2(390,23)*scale,ImVec2(150,38)*scale))send("rule-lesson");
    const bool tournamentGame=number(c,"tournamentID")!=0,casual=flag(c,"practice");
    const int fixed=number(c,"fixedWager",-1),affordable=std::max(0,std::min({number(c,"gold"),number(c,"rivalGold"),number(c,"maxWager")}));
    if(fixed<0&&(wager>affordable||!flag(c,"canStake")))wager=0;
    if(!tournamentGame&&!casual&&fixed<0&&number(c,"maxWager")){text(footer+ImVec2(size.x-349*scale,33*scale),"Wager",17,palette().muted);ImGui::SetCursorScreenPos(footer+ImVec2(size.x-290*scale,23*scale));ImGui::SetNextItemWidth(110*scale);
        std::vector<std::string> amounts;std::vector<int> values;int chosen=0;for(int value:{0,5,10,25,50,100,200})if(value<=affordable){if(wager==value)chosen=int(values.size());values.push_back(value);amounts.push_back(std::to_string(value)+" gold");}
        ImGui::BeginDisabled(!flag(c,"canStake"));if(select("##wager",chosen,amounts,125*scale))wager=values[chosen];ImGui::EndDisabled();}
    else if(!tournamentGame&&!casual&&fixed>0)text(footer+ImVec2(size.x-350*scale,33*scale),"Wager: "+std::to_string(fixed)+" gold",17,palette().muted);
    const bool complete=std::all_of(collection.deck.begin(),collection.deck.end(),[](auto id){return id!=0;});
    const bool ready=!pendingCommand&&!offer&&!number(c,"ruleRequest")&&(!complete||(flag(c,"canPlay")&&fixed<=affordable));
    if(button("play",complete?"Play":"Complete deck",footer+ImVec2(size.x-165*scale,23*scale),ImVec2(165,38)*scale,true,ready)){
        if(!complete){tab=1;deckSlot=-1;send("browse","album");}
        else{std::string args=std::to_string(number(state,"rules"))+" "+std::to_string(number(c,"fixedTrade"))+" "+std::to_string(tournamentGame||casual?0:wager);
            for(const auto& id:member(c,"deck"))args+=" "+std::to_string(id.get<int>());send("start",args);}
    }
    if(!ready&&complete){auto reason=fixed>affordable?"Not enough gold for this wager.":label(c,"playUnavailableReason");text(footer+ImVec2(size.x-390*scale,-30*scale),reason,16,palette().muted,390*scale);}
}
void GameApp::place(int hand,int square){
    if(hand<0||hand>=5||square<0||square>=9||pendingCommand||presentation.pending(state)||flag(state,"thinking"))return;
    const int id=itemNumber(member(state,"hands")[0],hand);if(id<0||!member(state,"board")[square].is_null())return;
    const auto mask=collection.cards[id%collection.cards.size()].groups;
    if((number(state,"rules")&(128|256))&&label(state,"activeGroup").empty()&&std::popcount(mask)>1){groupHand=hand;groupSquare=square;groupChoices.clear();for(int g=0;g<int(ttcg::creatureGroups.size());++g)if(mask&(1ull<<g))groupChoices.push_back(g);return;}
    send("play",std::to_string(hand)+" "+std::to_string(square));
}
void GameApp::groupModal(){
    if(groupHand<0)return;if(!ImGui::IsPopupOpen("Choose group"))ImGui::OpenPopup("Choose group");
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize*.5f,ImGuiCond_Always,ImVec2(.5f,.5f));ImGui::SetNextWindowSize(ImVec2(460*scale,0));ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(25,25)*scale);
    if(ImGui::BeginPopupModal("Choose group",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)){
        textBlock(number(state,"rules")&128?"Choose the Legion group":"Choose the Decimation group",400*scale,25,palette().accent);ImGui::Dummy(ImVec2(0,15*scale));
        for(int g:groupChoices){auto p=ImGui::GetCursorScreenPos();if(button(("group"+std::to_string(g)).c_str(),ttcg::creatureGroups[g].data(),p,ImVec2(400,40)*scale)){
                send("play",std::to_string(groupHand)+" "+std::to_string(groupSquare)+" "+std::to_string(g));groupHand=-1;ImGui::CloseCurrentPopup();break;}}
        if(ImGui::Button("Cancel")||ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false)){groupHand=-1;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }ImGui::PopStyleVar();
}
std::pair<ImVec2,ImVec2> GameApp::table(const Json& data,ImVec2 at,ImVec2 size,bool demo,bool liveLesson){
    if(demo){
        const std::string identity=(liveLesson?"lesson"+std::to_string(state.value("session",0ull)):topic);
        if(identity!=demoIdentity||number(demoAnimation,"step",-1)!=number(data,"step")){
            Json next=data;next["session"]=std::hash<std::string>{}(identity);next["revision"]=number(data,"step");next["phase"]="match";
            next["settling"]=identity==demoIdentity&&number(data,"step")==number(demoAnimation,"step",-2)+1;
            for(auto& stage:next["stages"])stage["label"]=ttcg::captureLabel(number(stage,"rule"));
            demoPresentation.begin(next,ImGui::GetTime(),demoAnimation);demoAnimation=std::move(next);demoIdentity=identity;
        }
        if(liveLesson&&demoPresentation.pending(demoAnimation))ruleSounds(demoAnimation,demoPresentation);
        if(ImGui::GetTime()-demoPresentation.start>=demoPresentation.duration)demoPresentation.acknowledged=true;
    }
    const auto& clock=demo?demoPresentation:presentation;const auto& animation=demo?demoAnimation:state;
    const bool animating=clock.pending(animation);
    const bool collected=!demo&&label(state,"phase")=="result"&&trade.phase!=TradeView::None;
    if(!demo&&!collected)trade.sourceMask=0;
    const bool staticDemo=demo&&!liveLesson&&(topic=="open"||topic=="threeopen");
    const bool busy=animating||(!demo&&(flag(state,"thinking")||pendingCommand||label(state,"phase")!="match"));
    const bool playerTurn=demo?number(data,"hand",-1)>=0:number(state,"turn")==0;
    const float gap=10*scale,boardH=size.y-55*scale,tileH=(boardH-2*gap-22*scale)/3,tileW=tileH*.75f,boardW=3*tileW+2*gap+22*scale;
    const ImVec2 boardAt=at+ImVec2((size.x-boardW)/2,45*scale);auto* d=ImGui::GetWindowDrawList();
    const int defaults[]{3,2,5,4,6};const int configuredSurface=std::clamp(number(member(state,"settings"),"boardSurface"),0,6),surface=configuredSurface?configuredSurface:defaults[theme];
    struct BoardColours{ImU32 edge,grid,mark,cell;};
    constexpr BoardColours colours[]{
        {IM_COL32(146,151,132,119),IM_COL32(210,216,197,33),IM_COL32(203,209,186,82),IM_COL32(255,255,255,3)},
        {IM_COL32(139,151,155,153),IM_COL32(224,232,225,43),IM_COL32(180,193,199,82),IM_COL32(0,0,0,5)},
        {IM_COL32(179,157,107,153),IM_COL32(186,204,171,54),IM_COL32(217,202,156,82),IM_COL32(0,0,0,8)},
        {IM_COL32(118,85,55,199),IM_COL32(103,67,35,82),IM_COL32(114,78,41,82),IM_COL32(137,94,34,3)},
        {IM_COL32(169,154,133,130),IM_COL32(201,185,164,43),IM_COL32(186,170,143,82),IM_COL32(10,4,3,16)},
        {IM_COL32(196,158,105,140),IM_COL32(226,193,144,56),IM_COL32(211,182,136,82),IM_COL32(31,17,10,21)}
    };const auto boardColour=colours[surface-1];
    boardSurface(surface,boardAt,ImVec2(boardW,boardH));d->AddRect(boardAt,boardAt+ImVec2(boardW,boardH),boardColour.edge);d->AddRect(boardAt-ImVec2(5,5)*scale,boardAt+ImVec2(boardW,boardH)+ImVec2(5,5)*scale,boardColour.edge);
    const char* materials[]{"","","slate","fine","paper","leather","wood"};
    if(surface!=1&&face.grain>0){auto texture=assets.get(std::string("../assets/themes/")+materials[surface]+".png");d->AddImage(texture.id,boardAt,boardAt+ImVec2(boardW,boardH),ImVec2(0,0),ImVec2(1,1),IM_COL32(255,255,255,int(face.grain*255*.75f)));}
    const float handArea=(size.x-boardW)/2-30*scale;
    const bool stack=demo;const float hw=std::min(stack?handArea*.70f:(handArea-gap)/2,tileW*.84f),hh=hw/.75f;
    const float handY=at.y+55*scale+(boardH-(stack?hh*2.15f:hh*3+2*gap))*.5f;
    const double elapsed=ImGui::GetTime()-clock.start;
    const double swapLength=flag(state,"opening")&&itemNumber(member(state,"swapped"),0)>=0&&number(state,"redeals")==0?2.2:0;
    std::array<std::array<ImVec2,5>,2> handPositions{};
    std::array<int,2> scores{};
    if(member(data,"hands").is_array())for(int p=0;p<2;++p)for(const auto& id:data["hands"][p])if(!id.is_null()&&(!id.is_number_integer()||id.get<int>()!=-1))++scores[p];
    for(int q=0;q<int(arrayMember(data,"board").size());++q){const auto& cell=data["board"][q];if(cell.is_null())continue;int owner=number(cell,"owner");
        if(animating)for(int i=0;i<int(arrayMember(animation,"stages").size());++i){const auto& stage=animation["stages"][i];if(number(stage,"captures")&(1<<q))if(elapsed<clock.stageStarts[i]+(label(stage,"label").empty()?0:.76)+.2)owner=1-owner;}
        ++scores[owner];
    }
    for(int player=0;player<2;++player){const float left=player?boardAt.x+boardW+30*scale:at.x;
        const ImU32 colour=controlColour(player,true);
        const float firstProgress=float((elapsed-swapLength-1.15)/1.1);
        const float firstGlow=!demo&&animating&&flag(state,"opening")&&number(state,"turn")==player?std::sin(std::numbers::pi_v<float>*std::clamp(firstProgress,0.f,1.f)):0;
        const auto name=player?(demo?"Opponent":label(state,"opponent","Opponent")):"You";
        const float nameSize=std::clamp(22.f*(handArea-86*scale)/std::max(1.f,font()->CalcTextSizeA(22*scale,1e5f,0,name.c_str()).x),12.f,22.f);
        const float nameWidth=font()->CalcTextSizeA(nameSize*scale,1e5f,0,name.c_str()).x;
        text(ImVec2(player?left+handArea-nameWidth-24*scale:left+24*scale,at.y+3*scale),name,nameSize,firstGlow>0?colour:palette().ink);
        text(ImVec2(player?left+8*scale:left+handArea-26*scale,at.y-3*scale),std::to_string(scores[player]),36,colour);
        line(ImVec2(left,at.y+35*scale),ImVec2(left+handArea,at.y+35*scale),playerTurn==(player==0)?colour:palette().line);
        const auto& hand=member(data,"hands");if(!hand.is_array()||hand.size()<2)continue;
        for(int order=0;order<5;++order){const int target=number(data,"hand",-1);const int h=stack&&player==0&&target>=0?(order==4?target:order<target?order:order+1):order;const int display=collected?-1:itemNumber(hand[player],h);
            const float x=stack?left+(handArea-hw)/2:left+(h==4?(handArea-hw)/2:(handArea-2*hw-gap)/2+(h%2)*(hw+gap));
            const float y=handY+(stack?h*hh*.29f:(h/2)*(hh+gap))-firstGlow*6*scale;const ImVec2 pos(x,y);handPositions[player][h]=pos;if(display==-1)continue;
            if(!demo&&!collected){const int origin=member(data,"handOrigins").is_array()?itemNumber(data["handOrigins"][player],h,player*5+h):player*5+h;
                if(origin>=0&&origin<10){trade.sources[origin]=pos;trade.sizes[origin]=ImVec2(hw,hh);trade.sourceMask|=1u<<origin;}}
            const int forced=number(data,"forcedHand",-1);const bool selectable=!busy&&player==0&&playerTurn&&(forced<0||forced==h)&&(!demo||staticDemo||number(data,"hand",-1)==h);
            ImGui::PushID(player*5+h);
            ImVec2 hitAt=pos,hitSize(hw,stack&&h<4&&h!=number(data,"hand",-1)?hh*.29f:hh);
            // The raised tutorial card also owns the covered input area. Merely
            // drawing it last would leave earlier, hidden cards catching clicks.
            if(stack&&player==0&&target>=0&&h>target){hitAt.y=std::max(pos.y,handY+target*hh*.29f+hh);hitSize.y=pos.y+hitSize.y-hitAt.y;}
            ImGui::SetCursorScreenPos(hitAt);const bool hittable=hitSize.y>0;
            const bool hit=hittable&&ImGui::InvisibleButton("hand",hitSize,ImGuiButtonFlags_EnableNav);
            if(hit&&selectable&&!staticDemo){selectedHand=h;demoSelected=demo;}
            if(hittable&&display>=0&&((ImGui::IsItemHovered()&&ImGui::IsMouseClicked(1))||(ImGui::IsItemFocused()&&(ImGui::IsKeyPressed(ImGuiKey_I)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp)))))inspectCard(display,true,itemNumber(member(data,"handModifiers").is_array()?member(data,"handModifiers")[player]:Json{},h,0));
            if(selectable&&!demo&&ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("TESSERA_HAND",&h,sizeof(h));selectedHand=h;dragCard(display%collection.cards.size(),display>=int(collection.cards.size()),0,CardContext::Hand);ImGui::EndDragDropSource();}
            const bool selected=player==0&&(demo?!staticDemo&&number(data,"hand",-1)==h:selectedHand==h&&playerTurn);
            const bool swapped=!demo&&presentation.pending(state)&&elapsed<swapLength&&itemNumber(member(state,"swapped"),player)==h;
            if(!swapped){
                if(firstGlow>0)for(int r=16;r>0;--r){const float padding=r*scale;d->AddRect(pos-ImVec2(padding,padding),pos+ImVec2(hw+padding,hh+padding),fade(colour,firstGlow*.065f*(1-r/17.f)),padding,0,2*scale);}
                displayCard(display,pos,ImVec2(hw,hh),selected,player,(demo?110:1)+player*5+h,itemNumber(member(data,"handModifiers").is_array()?member(data,"handModifiers")[player]:Json{},h,0),false,CardContext::Hand);
            }
            if(forced>=0&&player==0&&forced==h)text(pos+ImVec2(hw/2,hh-20*scale),number(data,"rules")&512?"Order":"Chaos",13,palette().accent,0,true);
            ImGui::PopID();
        }
    }
    for(int square=0;square<9;++square){const auto pos=boardAt+ImVec2(11*scale+(square%3)*(tileW+gap),11*scale+(square/3)*(tileH+gap));const ImVec2 cardSize(tileW,tileH);const auto& board=member(data,"board");if(!board.is_array()||square>=int(board.size()))continue;const auto& cell=board[square];
        ImGui::PushID(30+square);ImGui::SetCursorScreenPos(pos);const bool hit=ImGui::InvisibleButton("square",cardSize,ImGuiButtonFlags_EnableNav);
        if(!cell.is_null()&&!collected){
            if(!demo){const int origin=number(cell,"origin",-1);if(origin>=0&&origin<10){trade.sources[origin]=pos;trade.sizes[origin]=cardSize;trade.sourceMask|=1u<<origin;}}
            int owner=number(cell,"owner");float turnWidth=1;bool highlight=false;
            if(animating){
                const auto& stages=arrayMember(animation,"stages");for(int i=0;i<int(stages.size());++i){const auto& stage=stages[i];const double start=clock.stageStarts[i];const double labelDelay=label(stage,"label").empty()?0:.76;
                    if((number(stage,"highlights")&(1<<square))&&elapsed>=start&&elapsed<start+labelDelay)highlight=true;
                    if(number(stage,"captures")&(1<<square)){const double progress=(elapsed-start-labelDelay)/.4;if(progress<.5)owner=1-owner;if(progress>=0&&progress<1)turnWidth=std::max(.025f,float(std::abs(std::cos(progress*std::numbers::pi))));}
                }
            }
            const int firstVertex=d->VtxBuffer.Size;
            const bool moving=square==clock.placedSquare&&clock.placedHand>=0&&animating&&elapsed<.36;
            if(!moving)displayCard(number(cell,"card",-1),pos,cardSize,highlight,owner,(demo?120:11)+square,itemNumber(member(data,"modifiers"),square,0),false,CardContext::Board);
            if(turnWidth<1)for(int v=firstVertex;v<d->VtxBuffer.Size;++v)d->VtxBuffer[v].pos.x=pos.x+tileW/2+(d->VtxBuffer[v].pos.x-pos.x-tileW/2)*turnWidth;
            if((hit||ImGui::IsItemHovered()&&ImGui::IsMouseClicked(1)||ImGui::IsItemFocused()&&(ImGui::IsKeyPressed(ImGuiKey_I)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp)))&&!busy)inspectCard(number(cell,"card",-1),true,itemNumber(member(data,"modifiers"),square,0));
        }else{
            d->AddRectFilled(pos,pos+cardSize,boardColour.cell);d->AddRect(pos,pos+cardSize,boardColour.grid,1);
            const int affinity=itemNumber(member(data,"tiles"),square,0);if(affinity){const int a=std::countr_zero(unsigned(affinity));const float icon=tileW*.24f;auto image=assets.get("../icons/affinity-"+std::to_string(a)+".png");d->AddImage(image.id,pos+(cardSize-ImVec2(icon,icon))*.5f,pos+(cardSize+ImVec2(icon,icon))*.5f,ImVec2(0,0),ImVec2(1,1),IM_COL32(255,255,255,150));}else {const auto centre=pos+cardSize*.5f;const float r=3*scale;d->AddQuadFilled(centre+ImVec2(0,-r),centre+ImVec2(r,0),centre+ImVec2(0,r),centre+ImVec2(-r,0),boardColour.mark);}
            const bool target=!busy&&playerTurn&&selectedHand>=0&&(!demo||!staticDemo&&demoSelected&&square==number(data,"square",-1));
            if(target&&(ImGui::IsItemHovered()||ImGui::IsItemFocused()||demo))d->AddRect(pos,pos+cardSize,palette().accent,2,0,2*scale);
            if(hit&&target){if(demo){if(liveLesson){demoSelected=false;send("lesson","place "+std::to_string(number(data,"hand"))+" "+std::to_string(square));}else{++demoStep;demoSelected=false;demoChanged=ImGui::GetTime();}}else place(selectedHand,square);}
            if(!demo&&ImGui::BeginDragDropTarget()){if(auto* payload=ImGui::AcceptDragDropPayload("TESSERA_HAND"))if(payload->DataSize==sizeof(int)&&!busy)place(*static_cast<const int*>(payload->Data),square);ImGui::EndDragDropTarget();}
        }
        ImGui::PopID();
    }
    if(!demo&&animating&&elapsed<swapLength){
        const float t=std::clamp(float((elapsed-.60)/.70),0.f,1.f),ease=t*t*(3-2*t);
        for(int player=0;player<2;++player){const int hand=itemNumber(member(state,"swapped"),player),other=itemNumber(member(state,"swapped"),1-player);
            if(hand<0||other<0)continue;const auto source=handPositions[1-player][other],target=handPositions[player][hand];
            displayCard(itemNumber(state["hands"][player],hand),source+(target-source)*ease,ImVec2(hw,hh),true,t<.5f?1-player:player,162+player,0,false,CardContext::Hand);
        }
    }
    if(animating&&clock.placedSquare>=0&&clock.placedHand>=0&&elapsed<.36){
        const int q=clock.placedSquare;const auto target=boardAt+ImVec2(11*scale+(q%3)*(tileW+gap),11*scale+(q/3)*(tileH+gap));
        const auto source=handPositions[clock.placedPlayer][clock.placedHand];const float t=1-std::pow(1-std::clamp(float(elapsed/.36),0.f,1.f),3);
        displayCard(number(data["board"][q],"card",-1),source+(target-source)*t,ImVec2(hw,hh)+(ImVec2(tileW,tileH)-ImVec2(hw,hh))*t,false,clock.placedPlayer,160,0,false,t<.5f?CardContext::Hand:CardContext::Board);
    }
    if(demo&&animating){for(int i=0;i<int(arrayMember(animation,"stages").size());++i){const auto& stage=animation["stages"][i];const auto title=label(stage,"label");if(!title.empty()&&elapsed>=clock.stageStarts[i]&&elapsed<clock.stageStarts[i]+.76){
        drawAnnouncement(d,font(),boardAt,ImVec2(boardW,boardH),scale,title,float((elapsed-clock.stageStarts[i])/.76),title=="Combo"?"combo":"rule");
    }}}
    if(demo){for(const auto& comparison:arrayMember(data,"comparisons")){if(!comparison.is_array()||comparison.size()<2)continue;const int sq=itemNumber(comparison,0),side=itemNumber(comparison,1);if(sq<0||sq>8||side<0||side>3)continue;const auto p=boardAt+ImVec2(11*scale+(sq%3)*(tileW+gap),11*scale+(sq/3)*(tileH+gap));const ImVec2 places[]{ImVec2(face.classic?33.f/180:.5f,face.classic?20.f/240:.083f),ImVec2(face.classic?49.f/180:.917f,face.classic?39.f/240:face.names?.42f:.5f),ImVec2(face.classic?33.f/180:.5f,face.classic?58.f/240:face.names?.65f:.92f),ImVec2(face.classic?17.f/180:.083f,face.classic?39.f/240:face.names?.42f:.5f)};d->AddCircle(p+ImVec2(tileW*places[side].x,tileH*places[side].y),tileW*.105f,palette().accent,32,2*scale);}}
    return {boardAt,ImVec2(boardW,boardH)};
}
void GameApp::matchPanel(ImVec2 at,ImVec2 size){
    const float rulesHeight=ruleChips(at+ImVec2(size.x*.28f,-4*scale),size.x*.44f,number(state,"rules"),number(member(state,"collection"),"tournamentID")?-1:number(member(state,"collection"),"trade"),flag(member(state,"collection"),"practice"),true,nullptr,true);
    const float tableTop=std::max(0.f,rulesHeight-32*scale);
    const auto [boardAt,boardSize]=table(state,at+ImVec2(0,tableTop),ImVec2(size.x,size.y-tableTop-40*scale));
    const auto foot=at+ImVec2(0,size.y-24*scale);text(foot,label(state,"phase")=="result"?"":flag(state,"thinking")?label(state,"opponent")+" is thinking…":number(state,"turn")==0?"Your turn":label(state,"opponent")+"’s turn",17,palette().muted);
    const auto group=label(state,"activeGroup");if(!group.empty())text(foot+ImVec2(size.x*.38f,0),group+" "+(number(state,"groupModifier")>0?"+1":"−1"),18,palette().accent);
    if(button("reference","Rules",foot+ImVec2(size.x-100*scale,-7*scale),ImVec2(100,32)*scale,false,!presentation.pending(state))){showRuleReference=true;demoStep=0;}
    if(!presentation.pending(state))return;
    const double elapsed=ImGui::GetTime()-presentation.start;
    std::string callout,tone="rule";double start=0,duration=.76;bool result=false;
    if(flag(state,"opening")){
        const double swap=itemNumber(member(state,"swapped"),0)>=0&&number(state,"redeals")==0?2.2:0;
        if(elapsed<swap){callout="Swap";duration=swap;}
        else if(elapsed<swap+1.15){drawStarter(ImGui::GetWindowDrawList(),boardAt,boardSize,scale,float((elapsed-swap)/1.15),number(state,"turn"));return;}
        else{callout=number(state,"turn")==0?"You start":label(state,"opponent")+" starts";tone="first";start=swap+1.15;duration=1.1;}
    }else{
        for(int i=0;i<int(arrayMember(state,"stages").size());++i){const auto& s=state["stages"][i];
            if(elapsed>=presentation.stageStarts[i]&&elapsed<presentation.stageStarts[i]+.76&&!label(s,"label").empty()){
                callout=label(s,"label");start=presentation.stageStarts[i];
                tone=callout=="Combo"?"combo":callout.find("Legion")!=std::string::npos||callout.find("Decimation")!=std::string::npos?"group":"rule";
            }
        }
        const double stagesEnd=presentation.duration-(flag(state,"suddenDeath")?1.6:0)-(label(state,"phase")=="result"?1.8:0);
        if(flag(state,"suddenDeath")&&elapsed>=stagesEnd){callout="Sudden Death";start=stagesEnd;duration=1.6;}
        if(label(state,"phase")=="result"&&elapsed>=presentation.duration-1.8){
            const int yours=itemNumber(member(state,"score"),0),theirs=itemNumber(member(state,"score"),1);
            callout=yours==theirs?"Draw":yours>theirs?"You Win!":"You Lose";tone=yours==theirs?"draw":yours>theirs?"win":"lose";
            start=presentation.duration-1.8;duration=1.8;result=true;
        }
    }
    drawAnnouncement(ImGui::GetWindowDrawList(),font(),boardAt,boardSize,scale,callout,float((elapsed-start)/duration),tone,result);
}
void GameApp::ruleSounds(const Json& data,Presentation& clock){
    const double elapsed=ImGui::GetTime()-clock.start;
    for(int i=0;i<int(clock.stageStarts.size());++i)if(!clock.stageSounds[i]&&elapsed>=clock.stageStarts[i]){
        const auto name=lower(label(data["stages"][i],"label"));for(const auto& pair:std::array<std::pair<const char*,const char*>,6>{{{"same wall","samewall"},{"same","same"},{"plus","plus"},{"combo","combo"},{"legion","legion"},{"decimation","decimation"}}})if(name.find(pair.first)!=std::string::npos){send("rule",pair.second,false);if(name=="same wall")break;}clock.stageSounds[i]=true;
    }
}
void GameApp::tickPresentation(){
    if(!sourceReady||!presentation.pending(state))return;const double elapsed=ImGui::GetTime()-presentation.start;
    const double swap=flag(state,"opening")&&itemNumber(member(state,"swapped"),0)>=0&&number(state,"redeals")==0?2.2:0;
    if(flag(state,"opening")&&!presentation.openingSound&&elapsed>=swap+1.15){send("opening",{},false);presentation.openingSound=true;}
    ruleSounds(state,presentation);
    if(label(state,"phase")=="result"&&!presentation.resultSound&&elapsed>=presentation.duration-1.8){send("result",{},false);presentation.resultSound=true;}
    if(elapsed>=presentation.duration){presentation.acknowledged=true;send("settled",{},false);}
}
}
