#include "FreePlay.h"
#include "reputation_support.h"
#include <cassert>
#include <iostream>
struct ReadinessBank {
 std::array<ttcg::Stock*,2> stocks;
 int count(int p,ttcg::CardID id){return ttcg::count(*stocks[p],id);}
 int change(int p,ttcg::CardID id,int amount){auto& stock=*stocks[p];if(amount<0)amount=-std::min(-amount,count(p,id));stock[id]+=amount;return amount;}
};
int main(){
 using namespace ttcg;
 CollectionSave s;grantStarter(s);
 const int i=opponentIndex(0x13BAA);const auto& p=opponents[i];const auto key=opponentLedgerKey(p.base,p.reference);
 assert(opponentHasCards(s,i));assert(s.opponents.empty()); // Reading never discovers or seeds a player.
 replenishOpponent(s,i,key,3);earnReputation(s,p.hold);s.invitations[p.base]={24,true};
 assert(willingToPlay(s,i,3));auto& rival=s.opponents[key];
 auto hand=opponentHand(rival,i,12);rival.clear();for(auto id:hand)++rival[id];
 assert(opponentHasCards(s,i));assert(std::string_view(cardsPreventingPlay(s.player,rival,i)).empty());
 ReadinessBank bank{{&s.player,&rival}};
 auto player=selectHand(s.player,11);assert(s.contract.reserve(bank,{player,hand},TradeAll,0,key));
 Match match(indices(player),indices(hand),7,Open);match.placed=9;
 for(int owner=0;owner<2;++owner)match.used[owner].fill(true);match.used[0][4]=false;
 for(int square=0;square<9;++square){const int origin=square<4?square:square+1;match.board[square]={match.hands[origin/5][origin%5],0,origin};}
 assert(s.contract.capture(match));assert(s.contract.outcome==0);
 assert(s.contract.settleSelection(bank,0,s.contract.automaticChoice(0)));
 assert(!opponentHasCards(s,i));assert(!willingToPlay(s,i,4));assert(challengeAcceptance(s,i)==0);
 assert(std::string_view(cardsPreventingPlay(s.player,rival,i))=="They don't have enough cards for a game.");
 CollectionSave restored;assert(decodeCollection(encodeCollection(s),restored));assert(!willingToPlay(restored,i,4));
 replenishOpponent(s,i,key,74);assert(!opponentHasCards(s,i));
 replenishOpponent(s,i,key,75);assert(opponentHasCards(s,i));s.invitations[p.base]={100,true};assert(willingToPlay(s,i,75));
 assert(std::string_view(cardsPreventingPlay({},rival,i))=="You don't have enough cards for a game.");
 // Five physical copies are insufficient when they exceed identity limits.
 const auto low=personalPool(i).begin()->first;rival={{low,3},{low|foilFlag,2}};assert(!opponentHasCards(s,i));
 // High-tier cards already owned count for both readiness and actual selection.
 rival.clear();for(const auto& card:cards)if(card.tier>=8&&!card.pending){rival[card.form]=1;if(cardCount(rival)==5)break;}
 assert(cardCount(rival)==5&&opponentHasCards(s,i));assert(validHand(opponentHand(rival,i,12),rival));
 assert(willingToPlay(s,i,75)&&challengeAcceptance(s,i)>0);
 // Masters do not initiate conversations when their collection is depleted.
 const int master=opponentIndex(0x13BBF);earnReputation(s,opponents[master].hold);assert(challengeAcceptance(s,master)>0);
 s.opponents[opponentLedgerKey(opponents[master].base,opponents[master].reference)]={};assert(challengeAcceptance(s,master)==0);
 std::cout<<"Readiness: settled Trade All, dialogue refusal, reload, restock, deck limits, high-tier acquisitions, player shortage and master invitations passed\n";
}
