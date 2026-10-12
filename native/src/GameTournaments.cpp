#include "GameApp.h"
#include <bit>
#include "Announcements.h"
namespace preview {
void GameApp::tournaments(ImVec2 at,ImVec2 size){
    const auto& c=member(state,"collection");const auto& data=member(c,"tournaments");const auto& all=arrayMember(data,"events");
    const float nav=AlbumLayout::navigation*scale,gap=AlbumLayout::gap*scale;line(at+ImVec2(nav+gap/2,0),at+ImVec2(nav+gap/2,size.y));
    if(button("available","Available",at,ImVec2(103,35)*scale,!history)){history=false;eventID=0;}if(button("history","History",at+ImVec2(111,0)*scale,ImVec2(85,35)*scale,history)){history=true;eventID=0;}
    std::vector<const Json*> events;for(const auto& e:all)if(history?flag(e,"joined")&&number(e,"champion")!=0:!number(e,"champion")&&number(data,"hour")<number(e,"ends"))events.push_back(&e);
    const auto priority=[&](const Json* e){return flag(*e,"joined")&&!flag(*e,"eliminated")?0:flag(*e,"canEnter")?1:number(*e,"hold")==number(data,"currentHold")?2:3;};
    std::sort(events.begin(),events.end(),[&](const auto* a,const auto* b){if(history)return number(*a,"ends")>number(*b,"ends");if(priority(a)!=priority(b))return priority(a)<priority(b);return number(*a,"ends")<number(*b,"ends");});
    if(std::none_of(events.begin(),events.end(),[&](const auto* e){return number(*e,"id")==int(eventID);}))eventID=events.empty()?0:number(*events.front(),"id");
    ImGui::SetCursorScreenPos(at+ImVec2(0,54)*scale);ImGui::BeginChild("tournament-list",ImVec2(nav,size.y-54*scale));
    for(auto* e:events){const int id=number(*e,"id");const bool won=number(*e,"champion")==20;const bool unseen=flag(*e,"unseen")&&!seenEvents.contains(id);
        if(chapter("event"+std::to_string(id),label(*e,"name"),ImGui::GetCursorScreenPos(),ImVec2(nav-20*scale,75*scale),id==int(eventID),label(*e,"circuitName"),0,won?1:unseen?2:0))eventID=id;ImGui::Dummy(ImVec2(0,4*scale));}
    ImGui::EndChild();const auto origin=at+ImVec2(nav+gap,0);const float width=size.x-nav-gap;
    const auto* current=tournament(eventID);if(!current){text(origin,history?"No tournament history":"Speak with an innkeeper to find and enter tournaments.",23,palette().muted,width);return;}const auto& e=*current;
    if(flag(e,"unseen")&&!seenEvents.contains(eventID)){seenEvents.insert(eventID);send("tournament","seen "+std::to_string(eventID),false);}
    text(origin,label(e,"name")+" · "+label(e,"circuitName"),30,palette().accent,width-170*scale);
    const auto champion=number(e,"champion");text(origin+ImVec2(width-170*scale,14*scale),champion?(champion==20?"Champion":flag(e,"eliminated")?"Eliminated":"Complete"):flag(e,"eliminated")?"Withdrawn":flag(e,"joined")?"In progress":"Open for entry",17,palette().accent,170*scale);
    text(origin+ImVec2(0,44)*scale,label(e,"venue")+" · "+label(e,"host"),18,palette().muted);line(origin+ImVec2(0,82)*scale,origin+ImVec2(width,82*scale));
    ruleChips(origin+ImVec2(0,91)*scale,width,number(e,"rules"),-1,false,true);text(origin+ImVec2(0,135)*scale,(champion?"Ended ":"Closes ")+label(e,"endsLabel"),16,palette().muted);
    const float prizes=205*scale,boardW=width-prizes-gap,top=185*scale,availableH=size.y-top-90*scale,colGap=15*scale,colW=(boardW-2*colGap)/3;
    for(int col=0;col<3;++col){const char* title=col==0?"Quarterfinals":col==1?"Semifinals":"Final";text(origin+ImVec2(col*(colW+colGap),top-20*scale),title,17,palette().muted);
        const int start=col==0?8:col==1?12:14,count=col==0?4:col==1?2:1;const float section=availableH/count,pairH=std::min(86*scale,section-12*scale);
        for(int pair=0;pair<count;++pair){const int node=start+pair;const auto pos=origin+ImVec2(col*(colW+colGap),top+(pair+.5f)*section-pairH/2);auto* d=ImGui::GetWindowDrawList();d->AddRect(pos,pos+ImVec2(colW,pairH),palette().line);line(pos+ImVec2(0,pairH/2),pos+ImVec2(colW,pairH/2));
            for(int side=0;side<2;++side){const int slot=(node-8)*2+side;const auto& bracket=member(e,"bracket");if(!bracket.is_array()||slot>=int(bracket.size()))continue;const auto& player=bracket[slot];const int wins=itemNumber(member(e,"wins")[node-8],side,0);const auto row=pos+ImVec2(0,side*pairH/2);
                if(number(player,"id")==20)d->AddRectFilled(row,row+ImVec2(colW,pairH/2),(palette().selection&0xffffff)|0x55000000);
                text(row+ImVec2(10,10)*scale,flag(player,"reserved")?"Open":label(player,"name"),16,wins==2?palette().accent:palette().ink,colW-40*scale);
                if(number(player,"id"))text(row+ImVec2(colW-24*scale,10*scale),std::to_string(wins),18,palette().muted);
            }
        }
    }
    const auto prizeAt=origin+ImVec2(width-prizes,top);line(prizeAt-ImVec2(gap/2,15*scale),prizeAt+ImVec2(-gap/2,availableH));
    text(prizeAt,label(e,"prize"),22,palette().accent,prizes);text(prizeAt+ImVec2(0,58)*scale,std::to_string(number(e,"purse"))+" gold",18,palette().muted);
    int pindex=0;const auto& prizesList=arrayMember(e,"prizeCards");for(const auto& id:prizesList){const float cardW=prizesList.size()>1?(prizes-10*scale)/2:prizes;const auto pos=prizeAt+ImVec2((pindex%2)*(cardW+10*scale),103*scale+(pindex/2)*(cardW/.75f+15*scale));const ImVec2 extent(cardW,cardW/.75f);ImGui::SetCursorScreenPos(pos);if(ImGui::InvisibleButton(("prize"+std::to_string(pindex)).c_str(),extent,ImGuiButtonFlags_EnableNav))inspectCard(id.get<int>());displayCard(id.get<int>(),pos,extent,false,-1,135+pindex);++pindex;}
    if(number(e,"fee")&&!flag(e,"joined"))text(prizeAt+ImVec2(0,availableH-20*scale),"Entry: "+std::to_string(number(e,"fee"))+" gold",16,palette().muted);
    const auto footer=origin+ImVec2(0,size.y-63*scale);line(footer,footer+ImVec2(width,0));
    if(champion)return;
    const bool joined=flag(e,"joined"),play=joined&&!flag(e,"eliminated"),registration=!joined;
    const auto& interrupted=member(state,"interruptedMatch");const bool hasInterrupted=!interrupted.is_null()&&!interrupted.empty(),same=hasInterrupted&&number(interrupted,"tournament")==int(eventID);
    std::string reason;int playable=0;for(const auto& card:collection.cards)playable+=std::min(card.unique?1:2,card.owned[0]+card.owned[1]);
    if(!flag(member(state,"settings"),"gamesEnabled",true))reason="Turn on Tessera games in Settings to play.";
    else if(hasInterrupted)reason=same?flag(interrupted,"canResume")?"":"Return to "+label(e,"venue")+" to resume.":"Finish your interrupted game first.";
    else if(registration&&flag(data,"activeEntry"))reason="Finish your current tournament first.";
    else if(registration&&!flag(e,"canEnter"))reason="Speak to "+label(e,"host")+" at "+label(e,"venue")+" to enter.";
    else if(!flag(e,"atVenue"))reason="Return to "+label(e,"venue")+" to play.";
    else if(registration&&number(c,"gold")<number(e,"fee"))reason="Not enough gold for entry.";
    else if(registration&&playable<5)reason="You need five playable cards to enter.";
    text(footer+ImVec2(0,play&&!hasInterrupted?-28:23)*scale,reason,16,palette().muted,width-250*scale);
    if(play&&!hasInterrupted){if(button("tournament-deck","Edit deck",footer+ImVec2(0,20)*scale,ImVec2(110,36)*scale)){tab=1;screen=3;}if(button("withdraw","Withdraw",footer+ImVec2(120,20)*scale,ImVec2(110,36)*scale))confirm("Withdraw from tournament?",number(e,"fee")?"Your entry fee will not be refunded.":"This ends your tournament run.","tournament","withdraw "+std::to_string(eventID),"Withdraw");}
    if(registration||play||same){const auto action=same?flag(interrupted,"finished")?"Collect rewards":"Resume game":registration?"Enter tournament":number(e,"round")==1?"Play quarterfinal":number(e,"round")==2?"Play semifinal":"Play final";
        if(button("tournament-action",action,footer+ImVec2(width-215*scale,17*scale),ImVec2(215,40)*scale,true,reason.empty()&&!pendingCommand)){
            if(same)send("interrupted","resume");else if(registration)confirm(number(e,"fee")?"Enter for "+std::to_string(number(e,"fee"))+" gold?":"Enter tournament?","","tournament","enter "+std::to_string(eventID),"Enter");else send("tournament","round "+std::to_string(eventID));}
    }
}
void GameApp::tournamentOutcome(){
    if(!sourceReady||flag(state,"settling")||!member(state,"interruptedMatch").is_null())return;const auto& c=member(state,"collection");const Json* current=nullptr;
    if(label(state,"phase")=="result")current=tournament(number(c,"tournamentID"));
    if(!current)for(const auto& e:arrayMember(member(c,"tournaments"),"events"))if(number(e,"announcement")){current=&e;break;}
    if(!current||inspecting||showRuleReference||!confirmationTitle.empty())return;const auto& e=*current;eventID=number(e,"id");
    if(!ImGui::IsPopupOpen("Tournament result"))ImGui::OpenPopup("Tournament result");const float width=850*scale;ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize*.5f,ImGuiCond_Always,ImVec2(.5f,.5f));ImGui::SetNextWindowSize(ImVec2(width,std::min(775*scale,ImGui::GetIO().DisplaySize.y-45*scale)));ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(30,28)*scale);
    if(ImGui::BeginPopupModal("Tournament result",nullptr,ImGuiWindowFlags_NoDecoration)){
        const bool champion=number(e,"champion")==20,over=number(e,"champion")||flag(e,"eliminated");const int announce=number(e,"announcement"),upgrades=number(e,"foilUpgrades");
        const auto outcome=itemNumber(member(state,"score"),0)==itemNumber(member(state,"score"),1)?"Draw":itemNumber(member(state,"score"),0)>5?"Game won":"Game lost";
        const auto title=champion?"Tournament won":announce==1?"Through to the semifinals":announce==2?"Through to the final":flag(e,"eliminated")?"Tournament over":outcome;
        textBlock(title,width-60*scale,34,palette().accent);ImGui::Dummy(ImVec2(0,13*scale));textBlock(label(e,"name")+" · "+label(e,"circuitName"),width-60*scale,20,palette().muted);
        const int node=number(e,"lastNode",8);if(node>=8&&node<=14){const auto& wins=member(e,"wins")[node-8];textBlock(std::string(node<12?"Quarterfinals":node<14?"Semifinals":"Final")+" · "+std::to_string(itemNumber(wins,0,0))+"–"+std::to_string(itemNumber(wins,1,0)),width-60*scale,20,palette().muted);}
        ImGui::Dummy(ImVec2(0,20*scale));
        if(champion){textBlock(std::to_string(number(e,"purse"))+" gold",width-60*scale,23,palette().accent);if(!upgrades)textBlock(label(e,"prize"),width-60*scale,22,palette().ink);
            const auto p=ImGui::GetCursorScreenPos()+ImVec2(0,15*scale);int slot=0;const auto& cards=arrayMember(e,"prizeCards");for(const auto& id:cards){const auto pos=p+ImVec2(slot*198*scale,0);ImGui::SetCursorScreenPos(pos);if(ImGui::InvisibleButton(("reward-art"+std::to_string(slot)).c_str(),ImVec2(180,240)*scale,ImGuiButtonFlags_EnableNav))inspectCard(id.get<int>());displayCard(id.get<int>(),pos,ImVec2(180,240)*scale,false,-1,145+slot);++slot;}if(!cards.empty()){ImGui::SetCursorScreenPos(p+ImVec2(0,256)*scale);ImGui::Dummy(ImVec2(1,1));}
        }
        if(upgrades){textBlock(std::to_string(upgrades)+" foil upgrade"+(upgrades==1?"":"s")+" remaining",width-60*scale,23,palette().accent);
            std::vector<int> options;for(int i=0;i<int(collection.cards.size());++i)if(collection.cards[i].owned[0]>0&&collection.cards[i].owned[1]<1000000)options.push_back(i);std::sort(options.begin(),options.end(),[&](int a,int b){return collection.cards[a].name<collection.cards[b].name;});
            if(std::find(options.begin(),options.end(),int(foilSelection))==options.end())foilSelection=options.empty()?0:options.front();
            std::vector<std::string> names;int selected=0;for(int id:options){if(id==int(foilSelection))selected=int(names.size());names.push_back(collection.cards[id].name);}
            if(names.empty())textBlock("No cards to upgrade",500*scale,18,palette().muted);
            else if(select("##foil-card",selected,names,500*scale))foilSelection=options[selected];
            if(!options.empty()){const auto p=ImGui::GetCursorScreenPos();displayCard(foilSelection,p,ImVec2(156,208)*scale,false,-1,148);
                if(button("upgrade","Make foil",p+ImVec2(190,80)*scale,ImVec2(200,43)*scale,true,!pendingCommand))send("tournament","foil "+std::to_string(eventID)+" "+std::to_string(foilSelection));ImGui::SetCursorScreenPos(p+ImVec2(0,224)*scale);ImGui::Dummy(ImVec2(1,1));}
        }
        const auto bottom=ImGui::GetWindowPos()+ImVec2(30*scale,ImGui::GetWindowSize().y-65*scale);line(bottom-ImVec2(0,15*scale),bottom+ImVec2(width-60*scale,-15*scale));
        const auto leave=[&](std::string action){if(upgrades)confirm("Leave "+std::to_string(upgrades)+" foil upgrades unused?","They will be lost.","tournament",action+" "+std::to_string(eventID),"Leave");else send("tournament",action+" "+std::to_string(eventID));};
        if(button("leave-tournament","Leave",bottom,ImVec2(110,40)*scale)){leave("leave");ImGui::CloseCurrentPopup();}
        if(!over&&button("view-bracket","View bracket",bottom+ImVec2(125,0)*scale,ImVec2(160,40)*scale)){leave("ack");ImGui::CloseCurrentPopup();}
        const auto next=over?champion?"View tournament":"View results":announce==1?"Continue to semifinals":announce==2?"Continue to final":std::string_view(outcome)=="Draw"?"Replay game":"Next game";
        if(button("next-game",next,bottom+ImVec2(width-330*scale,0),ImVec2(270,40)*scale,true,!pendingCommand)){if(over)leave("ack");else send("tournament","continue "+std::to_string(eventID));ImGui::CloseCurrentPopup();}
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false)){leave("ack");ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }ImGui::PopStyleVar();
}
void GameApp::takeTrade(){const auto& c=member(state,"collection");if(std::popcount(claimMask)!=number(c,"required"))return;send(number(c,"trade")==1?"claim":"claim-many",std::to_string(number(c,"trade")==1?std::countr_zero(claimMask):claimMask));}
void GameApp::resultModal(){
    if(label(state,"phase")!="result"||flag(state,"settling"))return;
    const auto& c=member(state,"collection");if(number(c,"tournamentID"))return;
    // Inspection pauses this presentation, never the settlement transaction.
    if(inspecting||showRuleReference||!confirmationTitle.empty()){trade.started+=ImGui::GetIO().DeltaTime;return;}
    const bool choice=flag(c,"reward"),paying=flag(c,"paying");
    const int outcome=number(c,"outcome",-1),required=number(c,"required");
    const auto& hands=member(state,"originalHands");const auto& owners=member(c,"transferOwners");
    std::array<int,10> destinations{};int transfers=0;
    for(int origin=0;origin<10;++origin){destinations[origin]=itemNumber(owners,origin,origin/5);if(destinations[origin]!=origin/5)++transfers;}
    const auto now=ImGui::GetTime();
    const auto advance=[&](TradeView::Phase phase){trade.phase=phase;trade.started=now;};
    if(trade.phase==TradeView::None)advance(number(c,"trade")||number(c,"wager")?TradeView::Gathering:TradeView::Done);
    if(trade.phase==TradeView::Gathering&&now-trade.started>=1.0)advance(choice?TradeView::Selecting:TradeView::Reviewing);
    if(trade.phase==TradeView::Selecting&&!choice)advance(TradeView::Reviewing);
    if(trade.phase==TradeView::Reviewing&&now-trade.started>=1.0&&!flag(c,"staked")&&!paying)advance(TradeView::Transferring);
    if(trade.phase==TradeView::Transferring&&now-trade.started>=.58+std::max(0,transfers-1)*.12+.65)advance(TradeView::Done);
    const bool done=trade.phase==TradeView::Done;
    const bool can=flag(c,"canPlay");
    const std::string reason=done?(can?label(c,"notice"):label(c,"playUnavailableReason",label(state,"opponent")+" doesn't have enough cards for a game.")):std::string{};
    const auto* event=tournament(number(member(c,"tournaments"),"notification"));
    const float reasonHeight=reason.empty()?0:font()->CalcTextSizeA(19*scale,1e5f,490*scale,reason.c_str()).y+24*scale;
    const auto extent=done?ImVec2(560*scale,150*scale+reasonHeight+(event?78*scale:0)):ImVec2(1220,724)*scale;
    if(!ImGui::IsPopupOpen("Game result"))ImGui::OpenPopup("Game result");
    ImGui::SetNextWindowPos(frameAt+frameSize*.5f,ImGuiCond_Always,ImVec2(.5f,.5f));ImGui::SetNextWindowSize(extent);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(32,28)*scale);
    if(ImGui::BeginPopupModal("Game result",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar)){
        const auto panel=ImGui::GetWindowPos();surface(panel,extent);auto* d=ImGui::GetWindowDrawList();d->AddRect(panel,panel+extent,palette().accent);
        const auto origin=panel+ImVec2(32,28)*scale;const float width=extent.x-64*scale;
        if(done){
            float y=origin.y+4*scale;
            if(!reason.empty()){text(ImVec2(panel.x+extent.x/2,y),reason,19,palette().muted,width,true);y+=reasonHeight;}
            if(event){text(ImVec2(panel.x+extent.x/2,y),label(*event,"name"),19,palette().accent,width,true);
                if(button("tournament-notice","View tournament",ImVec2(panel.x+extent.x/2-95*scale,y+28*scale),ImVec2(190,36)*scale)){eventID=number(*event,"id");send("tournament","board");ImGui::CloseCurrentPopup();}y+=78*scale;}
            const float total=(can?340:150)*scale;const auto buttons=ImVec2(panel.x+(extent.x-total)/2,panel.y+extent.y-80*scale);
            if(can&&button("rematch","Play again",buttons,ImVec2(172,42)*scale,true,!pendingCommand)){send("prepare");ImGui::CloseCurrentPopup();}
            if(button("leave","Leave",buttons+ImVec2(can?190*scale:0,0),ImVec2(150,42)*scale,!can)||ImGui::IsKeyPressed(ImGuiKey_Escape,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false)){send("close");ImGui::CloseCurrentPopup();}
        }else{
            std::string title=choice?required==1?"Choose a card":"Choose "+std::to_string(required)+" cards · "+std::to_string(std::popcount(claimMask))+"/"+std::to_string(required):outcome<0?"Draw":outcome==0?"You Win!":label(state,"opponent")+"’s choice";
            if(trade.phase==TradeView::Transferring)title=outcome<0?"Draw":outcome==0?"You Win!":"You Lose";
            text(origin+ImVec2(width/2,0),title,28,palette().ink,width,true);
            const float gap=16*scale,w=160*scale,h=w/.75f;
            std::array<ImVec2,10> positions{};std::array<ImVec2,2> stacks{};
            const float elapsed=float(now-trade.started);
            const bool selectable=trade.phase==TradeView::Selecting&&!pendingCommand;
            for(int player=0;player<2;++player){
                const float y=origin.y+(64+(1-player)*268)*scale;
                text(ImVec2(origin.x,y),player?label(state,"opponent"):"You",20,palette().muted);
                stacks[player]=ImVec2(origin.x+width-126*scale,y+58*scale);
                // Empty receiving tray: subdued, without a redundant label.
                for(int edge=0;edge<4;++edge){const auto a=stacks[player]+ImVec2(edge==1?120:0,edge==2?158:0)*scale;
                    const auto b=stacks[player]+ImVec2(edge==3?0:120,edge==0?0:158)*scale;
                    const auto delta=b-a;const float length=std::sqrt(delta.x*delta.x+delta.y*delta.y);const auto direction=delta*(1/length);
                    for(float n=0;n<length;n+=8*scale)d->AddLine(a+direction*n,a+direction*std::min(n+4*scale,length),palette().line,1);}
                for(int slot=0;slot<5;++slot){const int index=player*5+slot,id=hands.is_array()?itemNumber(hands[player],slot):-1;if(id<0)continue;
                    const bool moving=destinations[index]!=player;
                    const bool selected=choice?player==1&&(claimMask&(1u<<slot)):moving;
                    const auto pos=ImVec2(origin.x+slot*(w+gap),y+34*scale);positions[index]=pos;
                    ImGui::PushID(index);ImGui::SetCursorScreenPos(pos-ImVec2(0,selected?14*scale:0));
                    const bool hit=ImGui::InvisibleButton("trade-card",ImVec2(w,h),ImGuiButtonFlags_EnableNav);
                    if(ImGui::IsItemHovered()||ImGui::IsItemFocused())trade.inspected=index;
                    if(hit&&selectable&&player==1){const auto bit=1u<<slot;if(claimMask&bit)claimMask&=~bit;else if(required==1)claimMask=bit;else if(std::popcount(claimMask)<required)claimMask|=bit;}
                    if((ImGui::IsItemHovered()&&ImGui::IsMouseClicked(1))||(ImGui::IsItemFocused()&&(ImGui::IsKeyPressed(ImGuiKey_I)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp)))||(hit&&(!choice||player==0)))inspectCard(id);
                    const bool inFlight=trade.phase==TradeView::Gathering&&(trade.sourceMask&(1u<<index))&&elapsed<.58+index*.045;
                    if(!inFlight&&!(trade.phase==TradeView::Transferring&&moving))displayCard(id,pos-ImVec2(0,selected?14*scale:0),ImVec2(w,h),selected,player,150+index);
                    else if(trade.phase==TradeView::Transferring&&moving)d->AddRect(pos,pos+ImVec2(w,h),palette().line);
                    ImGui::PopID();
                }
            }
            auto footer=origin+ImVec2(0,extent.y-96*scale);line(footer,footer+ImVec2(width,0));footer.y+=22*scale;
            const int wager=number(c,"wager");
            if(wager)text(footer+ImVec2(0,5*scale),(outcome<0?std::to_string(wager)+" gold returned":std::string(outcome==0?"+":"−")+std::to_string(wager)+" gold"),24,outcome==1?IM_COL32(223,149,133,255):palette().accent);
            if(!label(c,"notice").empty())text(footer+ImVec2(250*scale,8*scale),label(c,"notice"),16,palette().muted,width-610*scale);
            if(button("trade-inspect","Inspect",footer+ImVec2(width-302*scale,0),ImVec2(126,40)*scale)){
                const int index=trade.inspected;inspectCard(itemNumber(hands[index/5],index%5));
            }
            if(choice||paying)if(button("take-cards",paying?"Retry":"Confirm",footer+ImVec2(width-150*scale,0),ImVec2(150,40)*scale,true,paying||selectable&&std::popcount(claimMask)==required)){
                if(paying)send("payout");else takeTrade();
            }
            // Render flights above the panel, at full colour. All simultaneous
            // transfers share a clock; ownership is still settled by the backend.
            d->PushClipRect(frameAt,frameAt+frameSize,false);
            std::array<int,2> stackCount{};int order=0;
            for(int index=0;index<10;++index){const int player=index/5,id=hands.is_array()?itemNumber(hands[player],index%5):-1;if(id<0)continue;
                if(trade.phase==TradeView::Gathering&&(trade.sourceMask&(1u<<index))){
                    const float t=std::clamp((elapsed-index*.045f)/.58f,0.f,1.f),ease=1-std::pow(1-t,3);
                    if(t<1)displayCard(id,trade.sources[index]+(positions[index]-trade.sources[index])*ease,trade.sizes[index]+(ImVec2(w,h)-trade.sizes[index])*ease,false,player,150+index);
                }else if(trade.phase==TradeView::Transferring&&destinations[index]!=player){
                    const int receiver=destinations[index],n=stackCount[receiver]++;
                    const auto target=stacks[receiver]+ImVec2(n*4,n*5)*scale;
                    const auto source=positions[index]-ImVec2(0,14*scale);
                    const float t=std::clamp((elapsed-order++*.12f)/.58f,0.f,1.f),ease=1-std::pow(1-t,3);
                    displayCard(id,source+(target-source)*ease,ImVec2(w,h)+(ImVec2(100,100/.75f)*scale-ImVec2(w,h))*ease,false,t<.5f?player:receiver,150+index);
                }
            }
            d->PopClipRect();
        }
        ImGui::EndPopup();
    }ImGui::PopStyleVar();
}
}
