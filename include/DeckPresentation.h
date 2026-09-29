#pragma once
#include "Collection.h"
#include "Presentation.h"
namespace ttcg {
// JSON members shared by the native bridge and browser integration harness.
inline std::string savedDeckFields(const CollectionSave& saved) {
 std::string out="\"activeDeck\":"+std::to_string(saved.activeDeck)+",\"savedDecks\":[";
 for(std::size_t i=0;i<saved.decks.size();++i){const auto& d=saved.decks[i];if(i)out+=',';
   out+="{\"id\":"+std::to_string(d.id)+",\"name\":"+quote(d.name)+",\"cards\":[";
   for(int h=0;h<5;++h){if(h)out+=',';out+=std::to_string(displayIndex(d.cards[h]));}out+="]}";
 }
 out+="],\"deckReserved\":[";
 for(std::size_t i=0;i<cards.size()*2;++i){if(i)out+=',';out+=std::to_string(deckCopiesNeeded(saved,displayCardID(static_cast<int>(i))));}
 return out+"]";
}
}
