#include "Campaign.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
int main(){
 CollectionSave s;grantStarter(s);const auto original=s.deck;const auto owned=s.player;
 assert(s.decks.size()==1&&s.decks[0].cards==original);
 assert(manageDeck(s,"copy",1,"Same & Plus"));const auto second=s.activeDeck;
 std::reverse(s.deck.begin(),s.deck.end());saveActiveDeck(s);const auto reversed=s.deck;
 assert(manageDeck(s,"select",1)&&s.deck==original);
 assert(manageDeck(s,"select",second)&&s.deck==reversed&&s.player==owned);
 assert(manageDeck(s,"rename",second,"Azura's \"hand\""));
 assert(!manageDeck(s,"rename",second,"")&&!manageDeck(s,"rename",second,"bad\nname"));
 assert(!manageDeck(s,"rename",second,std::string(49,'x')));
 assert(validDeckName("Azura’s hand")&&!validDeckName("\xC0\xAF")&&!validDeckName("\xED\xA0\x80"));
 assert(manageDeck(s,"new",0,"Empty"));assert(s.deck==Hand{});
 assert(!validHand(s.deck,s.player));assert(manageDeck(s,"delete",s.activeDeck)&&s.activeDeck==1);
 auto wire=encodeCollection(s);CollectionSave loaded;
 assert(decodeCollection(wire,loaded)&&encodeCollection(loaded)==wire);
 assert(loaded.decks.size()==2&&loaded.decks[1].name=="Azura's \"hand\"");
 // Inactive decks protect copies from commerce without requiring summed ownership.
 assert(manageDeck(s,"new",0,"Trading"));auto id=original[0];assert(deckCopiesNeeded(s,id)==1);
 struct Gold {int amount=100;int count(int,CardID){return amount;}int change(int,CardID,int n){amount+=n;return n;}} gold;
 // Depletion clears the same missing copy from every deck, preserving surviving order.
 s.player[id]=0;retainOwnedDecks(s);
 for(const auto& d:s.decks)assert(std::count(d.cards.begin(),d.cards.end(),id)==0);
 assert(s.decks[0].cards[1]==original[1]);assert(s.decks[1].cards[0]==reversed[0]);
 // No changes during held stakes or partial gold recovery.
 s.contract.held=true;assert(!manageDeck(s,"new",0,"Blocked"));s.contract.held=false;s.goldCredit=5;
 assert(!manageDeck(s,"select",1));s.goldCredit=0;
 while(s.decks.size()<maxSavedDecks)assert(manageDeck(s,"new",0,"More"));assert(!manageDeck(s,"new",0,"Too many"));
 assert(decodeCollection(encodeCollection(s),loaded)&&loaded.decks.size()==20);
 while(s.decks.size()>1)assert(manageDeck(s,"delete",s.activeDeck));assert(!manageDeck(s,"delete",s.activeDeck));
 // Every truncated record leaves the destination untouched.
 auto good=encodeCollection(s);const auto unchanged=encodeCollection(loaded);
 for(std::size_t n=0;n<good.size();++n){auto bad=good;bad.resize(n);assert(!decodeCollection(bad,loaded)&&encodeCollection(loaded)==unchanged);}
 auto bad=good;bad.push_back(999);assert(!decodeCollection(bad,loaded));
 std::cout<<"Saved decks: creation, ordering, commerce protection, depletion, limits and corrupt current saves passed\n";
}
