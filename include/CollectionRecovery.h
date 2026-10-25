#pragma once
#include "Tournaments.h"
namespace ttcg {
inline std::string_view collectionRecoveryBlocked(const CollectionSave& s,bool interrupted=false) {
 if(interrupted)return "Finish or forfeit your interrupted game first.";
 if(s.contract.pending()||s.goldCredit)return "Finish the pending card trade or payment first.";
 if(pendingTournamentFoils(s))return "Use or leave your tournament rewards first.";
 if(tournamentEntryActive(s))return "Finish or withdraw from your tournament first.";
 return {};
}
inline std::string_view tesseraResetBlocked(const CollectionSave& s,bool interrupted=false) {
 if(const auto blocked=collectionRecoveryBlocked(s,interrupted);!blocked.empty())return blocked;
 for(const auto& [id,merchant]:s.merchants)if(merchant.goldCredit)return "Finish pending merchant payments first.";
 if(s.tournaments.packCredit||s.tournaments.standardPackCredit)return "Collect pending tournament packs first.";
 for(const auto& [id,gold]:s.tournaments.npcGold)if(gold)return "Finish pending tournament payments first.";
 return {};
}
inline CollectionSave freshTesseraSave(const CollectionSave& previous) {
 CollectionSave fresh;
 // Old courier acknowledgements must never match a newly scheduled tournament.
 fresh.tournaments.sequence=previous.tournaments.sequence;
 // Quests can retain offer serials in their own saves. Keep cancelled tombstones
 // so a delayed completion cannot match a new offer after resetting Tessera.
 fresh.exchanges=previous.exchanges;
 for(auto& [owner,offer]:fresh.exchanges){
  offer.status=ExchangeStatus::Cancelled;
  for(auto id:{offer.requested,offer.reward})if(auto found=previous.missingCards.find(baseCardID(id));found!=previous.missingCards.end())fresh.missingCards.emplace(*found);
 }
 grantStarter(fresh);
 return fresh;
}
inline bool unavailableCollectionCard(const CollectionSave& s,CardID id) {
 return id&&cardIndex(id)<0&&s.missingCards.contains(baseCardID(id));
}
inline std::set<std::string> missingCollectionSets(const CollectionSave& s) {
 std::set<std::string> result;
 for(const auto& [id,n]:s.player)if(n>0&&unavailableCollectionCard(s,id)){
  const auto& key=s.missingCards.at(baseCardID(id));result.insert(key.substr(0,key.find(':')));
 }
 return result;
}
inline bool removeUnavailableCards(CollectionSave& s) {
 if(!collectionRecoveryBlocked(s).empty())return false;
 for(auto* stock:{&s.player,&s.discovered})std::erase_if(*stock,[&](const auto& entry){return unavailableCollectionCard(s,entry.first);});
 retainOwnedDecks(s);
 // Other ledgers still need the identity dictionary to survive save/load.
 // Serialization drops dictionary entries once nothing references them.
 return true;
}
}
