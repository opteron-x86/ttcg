#include "reputation_support.h"
#include <iostream>
using namespace ttcg;
int player(unsigned level){for(int i=1;i<int(opponents.size());++i)if(opponents[i].hold==1&&opponents[i].level==level)return i;assert(false);return -1;}
int main(){
 struct Bank {std::array<Stock,2> stock;int count(int side,CardID id){return ttcg::count(stock[side],id);}int change(int side,CardID id,int n){stock[side][id]+=n;return n;}};
 for(auto [trade,gold]:{std::pair{TradeNone,0},std::pair{TradeNone,5},std::pair{TradeOne,0}}){
  Bank bank;bank.stock={practiceStock(),practiceStock()};bank.stock[0][0]=bank.stock[1][0]=100;
  const Hand hand=selectHand(practiceStock(),17);Contract contract;
  assert(contract.reserve(bank,{hand,hand},trade,gold,123));assert(!reputationStake(contract));
  Match match(indices(hand),indices(hand),17,Open);while(!match.finished())assert(match.play(match.legalMoves().front()).legal);
  assert(contract.capture(match));assert(reputationStake(contract)==(trade!=TradeNone||gold>0));
  contract.forfeited=true;assert(!reputationStake(contract));
 }
 CollectionSave untracked;
 const auto empty=encodeCollection(untracked);
 for(int result:{-1,0,1}){
  recordCampaignResult(untracked,player(2),result,false,Open,true);
  recordCampaignResult(untracked,player(2),result,true,Open,false);
  assert(encodeCollection(untracked)==empty);
 }
 assert(reputationObjective(untracked,1)=="Unknown");
 CollectionSave s;const int regular=player(2),experienced=player(3),expert=player(4),master=player(5),beginner=player(0),learner=player(1);
 assert(localReputation(s,1).rank==Unknown);
 assert(challengeReputationMet(s,regular)&&!challengeReputationMet(s,experienced)&&!challengeReputationMet(s,expert)&&!challengeReputationMet(s,master));
 for(int outcome:{-1,0,1}){
  recordCampaignResult(s,regular,outcome,false,Open,true); // Friendly, even with a bogus stake flag.
  recordCampaignResult(s,regular,outcome,true,Open); // Competitive with no risk.
  if(outcome!=0)recordCampaignResult(s,regular,outcome,true,Open,true);
 }
 assert(s.reputation.empty()); // Friendly, zero-stakes, losses and draws never count.
 // Hulda now records staked games and earns the same Beginner reputation as other adults.
 CollectionSave hulda;recordCampaignResult(hulda,0,0,true,Open,true);
 assert(progress(hulda,0).competitive.wins==1&&localReputation(hulda,1).quarters==4&&localReputation(hulda,1).players==1);
 CollectionSave restoredHulda;assert(decodeCollection(encodeCollection(hulda),restoredHulda)&&progress(restoredHulda,0).competitive.wins==1);
 recordCampaignResult(s,regular,0,true,Open,true);assert(localReputation(s,1).quarters==12&&localReputation(s,1).rank==Unknown);
 recordCampaignResult(s,regular,0,true,Open,true);assert(localReputation(s,1).quarters==18&&localReputation(s,1).rank==Unknown);
 recordCampaignResult(s,regular,0,true,Open,true);assert(localReputation(s,1).quarters==21&&localReputation(s,1).rank==Unknown);
 recordCampaignResult(s,regular,0,true,Open,true);assert(localReputation(s,1).quarters==24&&localReputation(s,1).rank==Unknown);
 recordCampaignResult(s,learner,0,true,Open,true);assert(localReputation(s,1).rank==Unsung&&localReputation(s,1).quarters==32);
 assert(reputationPoints(1)=="0.25"&&reputationPoints(2)=="0.5"&&reputationPoints(3)=="0.75"&&reputationPoints(4)=="1");
 CollectionSave roundtrip;assert(decodeCollection(encodeCollection(s),roundtrip));assert(reputationAward(roundtrip,regular)==3&&reputationAward(roundtrip,learner)==4);
 auto bad=s;bad.reputation[opponents[regular].base].quarters=1000000000;assert(!decodeCollection(encodeCollection(bad),roundtrip));
 // Earn each rank through legal opponents; earned points survive each new low-tier cutoff.
 for(unsigned rank=Recognized;rank<=Legendary;++rank){
  const auto before=localReputation(s,1).quarters;earnReputation(s,1,rank);assert(localReputation(s,1).rank==rank&&localReputation(s,1).quarters>=before);
  for(unsigned level=0;level<6;++level)assert((reputationAward(s,player(level))>0)==(level>=reputationFloor(rank)));
  const auto total=localReputation(s,1).quarters;recordCampaignResult(s,beginner,0,true,Open,true);assert(localReputation(s,1).quarters==total);
  if(rank==Recognized){assert(challengeReputationMet(s,experienced)&&!challengeReputationMet(s,expert));}
  if(rank==Respected){assert(challengeReputationMet(s,expert)&&!challengeReputationMet(s,master));}
  if(rank==Famous){assert(challengeReputationMet(s,master));}
 }
 assert(localReputation(s,2).rank==Unknown);
 // Repeated eligible wins never replace the distinct-opponent and stronger-opponent proofs.
 s={};for(unsigned n=0;n<100;++n)recordCampaignResult(s,regular,0,true,Open,true);assert(localReputation(s,1).rank==Unknown);
 recordCampaignResult(s,learner,0,true,Open,true);assert(localReputation(s,1).rank==Unsung);
 // All ten authored holds remain traversable with fixed regional targets.
 for(const auto& h:holds){CollectionSave local;earnReputation(local,h.id,Legendary);auto r=localReputation(local,h.id);assert(r.masterWins==r.mastersRequired);CollectionSave loaded;assert(decodeCollection(encodeCollection(local),loaded));assert(localReputation(loaded,h.id).rank==Legendary);std::cout<<h.name<<": "<<r.wins<<" staked wins, "<<r.players<<" opponents, "<<reputationPoints(r.quarters)<<" points\n";}
 // Temporary unavailability does not waive a Master. A confirmed death does.
 s={};earnReputation(s,1,Famous);int last=-1;
 for(int i=1;i<int(opponents.size());++i)if(opponents[i].hold==1&&opponents[i].level==5){if(last<0)last=i;else recordCampaignResult(s,i,0,true,Open,true);}
 progress(s,last).unavailable=true;assert(localReputation(s,1).rank==Famous);
 s.deadPlayers.insert(opponents[last].base);assert(localReputation(s,1).rank==Legendary);
 CollectionSave loaded;assert(decodeCollection(encodeCollection(s),loaded)&&localReputation(loaded,1).rank==Legendary);
 std::cout<<"Reputation: six ranks, actual stakes, fractional diminishing returns, persistent awards, gates, tier cutoffs, ten-hold progression and deceased Master handling passed\n";
}
