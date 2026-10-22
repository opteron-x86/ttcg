#include "Campaign.h"
#include <cassert>
#include <iostream>
struct Gold {
 int amount=200;
 int count(int,ttcg::CardID) { return amount; }
 int change(int,ttcg::CardID,int n) { n=std::max(-amount,n); amount+=n; return n; }
};
int main() {
 using ttcg::cards;
 {
   ttcg::CollectionSave dev;dev.player[cards[0].form]=2;dev.deck={cards[0].form,0,0,0,0};dev.opponents[123][cards[1].form]=3;
   const auto untouched=ttcg::encodeCollection(dev);
   assert(!ttcg::grantDevelopmentCards(dev,false,-1)&&ttcg::encodeCollection(dev)==untouched);
   assert(!ttcg::grantDevelopmentCards(dev,true,-2)&&!ttcg::grantDevelopmentCards(dev,true,cards.size())&&ttcg::encodeCollection(dev)==untouched);
   assert(ttcg::grantDevelopmentCards(dev,true,1)&&dev.player[cards[1].form]==1&&dev.discovered[cards[1].form]==1);
   assert(ttcg::grantDevelopmentCards(dev,true,-1)&&dev.player[cards[0].form]==3&&dev.player[cards[1].form]==2);
   for(auto c:cards)assert(dev.player[c.form]>0&&dev.discovered[c.form]==1);
   assert(dev.deck[0]==cards[0].form&&dev.deck[1]==0&&dev.opponents[123][cards[1].form]==3&&!dev.starter&&dev.players.empty());
   ttcg::CollectionSave loaded;assert(ttcg::decodeCollection(ttcg::encodeCollection(dev),loaded)&&loaded.player==dev.player&&loaded.discovered==dev.discovered);
   const auto owned=dev.player;dev.contract.held=true;assert(!ttcg::grantDevelopmentCards(dev,true,-1)&&dev.player==owned);dev.contract.held=false;dev.contract.paying=true;assert(!ttcg::grantDevelopmentCards(dev,true,0));dev.contract.paying=false;
   dev.goldCredit=1;assert(!ttcg::grantDevelopmentCards(dev,true,0));dev.goldCredit=0;
   dev.player[cards.back().form]=1000000;const auto capped=ttcg::encodeCollection(dev);assert(!ttcg::grantDevelopmentCards(dev,true,-1)&&ttcg::encodeCollection(dev)==capped);
 }
 {
   ttcg::CollectionSave s;
   assert(!ttcg::claimAlbum(s,false)&&!s.starter);
   assert(ttcg::claimAlbum(s,true)&&s.starter);
   unsigned starters=0;for(const auto& c:ttcg::cards)if(c.starter){++starters;assert(s.player[c.form]==1);}
   assert(starters==8&&ttcg::validHand(s.deck,s.player));
   const auto purchased=ttcg::encodeCollection(s);
   assert(!ttcg::claimAlbum(s,true)&&ttcg::encodeCollection(s)==purchased);
   ttcg::CollectionSave reloaded;assert(ttcg::decodeCollection(purchased,reloaded));
   assert(!ttcg::claimAlbum(reloaded,true)&&!ttcg::claimAlbum(reloaded,false));
   ttcg::CollectionSave pending;pending.contract.held=true;
   assert(!ttcg::claimAlbum(pending,true)&&!pending.starter);
   pending.contract.held=false;assert(ttcg::claimAlbum(pending,true));
   ttcg::CollectionSave dev;dev.developmentCardsGranted=true;
   assert(!ttcg::claimAlbum(dev,true));
 }
 {
   ttcg::CollectionSave s;Gold gold;gold.amount=99;
   auto before=ttcg::encodeCollection(s);assert(!ttcg::buyAlbum(s,gold)&&gold.amount==99&&ttcg::encodeCollection(s)==before);
   gold.amount=100;assert(ttcg::buyAlbum(s,gold)&&gold.amount==0&&s.starter);
   unsigned n=0;for(auto c:cards)if(c.starter){++n;assert(s.player[c.form]==1);}assert(n==8);
   before=ttcg::encodeCollection(s);gold.amount=100;
   assert(!ttcg::buyAlbum(s,gold)&&gold.amount==100&&ttcg::encodeCollection(s)==before);
   ttcg::CollectionSave reloaded;assert(ttcg::decodeCollection(before,reloaded)&&!ttcg::buyAlbum(reloaded,gold));
   ttcg::CollectionSave pending;pending.contract.held=true;assert(!ttcg::buyAlbum(pending,gold));
   pending.contract.held=false;pending.goldCredit=1;assert(!ttcg::buyAlbum(pending,gold));
   pending.goldCredit=0;pending.developmentCardsGranted=true;assert(!ttcg::buyAlbum(pending,gold));
   struct PartialGold:Gold {bool refuseRefund=false;int change(int p,ttcg::CardID id,int n){return Gold::change(p,id,n<0?-30:refuseRefund?0:n);}} partial;
   ttcg::CollectionSave rollback;assert(!ttcg::buyAlbum(rollback,partial)&&partial.amount==200&&!rollback.starter&&!rollback.goldCredit);
   partial.refuseRefund=true;assert(!ttcg::buyAlbum(rollback,partial)&&partial.amount==170&&rollback.goldCredit==30&&!rollback.starter);
   assert(!ttcg::buyAlbum(rollback,partial)&&partial.amount==170); // Outstanding refund prevents repeat charges.
 }
 ttcg::CollectionSave saved; saved.starter=true;
 for(int i=0;i<8;++i) {saved.player[ttcg::cards[i].form]=1;saved.discovered[ttcg::cards[i].form]=1;}
 saved.deck=ttcg::selectHand(saved.player,17);
 saved.opponents[123][ttcg::cards[9].form]=2;
 // Preserve/refund a pending adjustment migrated from an older custom charge.
 Gold failing;failing.amount=198;saved.goldCredit=2;
 ttcg::CollectionSave loaded;auto bytes=ttcg::encodeCollection(saved);assert(ttcg::decodeCollection(bytes,loaded));
 assert(loaded.player==saved.player&&loaded.discovered==saved.discovered&&loaded.opponents==saved.opponents&&loaded.goldCredit==2);
 loaded.goldCredit-=failing.change(0,0,loaded.goldCredit);assert(loaded.goldCredit==0&&failing.amount==200);
 for(std::size_t n=0;n<bytes.size();++n) {auto broken=bytes;broken.resize(n);ttcg::CollectionSave untouched;untouched.activeDeck=777;assert(!ttcg::decodeCollection(broken,untouched));assert(untouched.activeDeck==777);}
 std::vector<ttcg::CardID> starter;for(const auto& c:cards)if(c.starter)starter.push_back(c.form);assert(starter.size()==8);
 unsigned starterAttributes=0;bool neutralStarter=false;
 for(auto id:starter){const auto& c=cards[ttcg::cardIndex(id)];assert(c.tier==1);starterAttributes|=c.affinityMask;neutralStarter|=c.affinityMask==0;}
 assert(starterAttributes==31&&neutralStarter);
 for(int seed=0;seed<100;++seed) {auto pack=ttcg::packCards(seed);assert(pack.size()==5);std::sort(pack.begin(),pack.end());assert(std::adjacent_find(pack.begin(),pack.end())==pack.end());for(auto card:pack)assert(ttcg::cards[ttcg::cardIndex(card)].tier<=6);}
 // A save made with stakes held preserves both ledgers and the pending transfer.
 for(int winner:{-1,0,1}) {
   ttcg::CollectionSave matchSave; matchSave.starter=true;
   ttcg::Hand a{},b{};for(int i=0;i<5;++i) {a[i]=ttcg::cards[i].form;b[i]=ttcg::cards[i+5].form;++matchSave.player[a[i]];++matchSave.opponents[123][b[i]];}
   matchSave.deck=a;
   struct LedgerBank {
     ttcg::CollectionSave& saved; std::array<int,2> gold{100,50};
     int count(int p,ttcg::CardID id) {return id?ttcg::count(p?saved.opponents[123]:saved.player,id):gold[p];}
     int change(int p,ttcg::CardID id,int n) {n=std::max(-count(p,id),n);if(id)(p?saved.opponents[123]:saved.player)[id]+=n;else gold[p]+=n;return n;}
   } live{matchSave};
   assert(matchSave.contract.reserve(live,{a,b},true,25,123));matchSave.contract.outcome=winner;
   ttcg::CollectionSave loaded;assert(ttcg::decodeCollection(ttcg::encodeCollection(matchSave),loaded));
   LedgerBank resumed{loaded,live.gold};assert(loaded.contract.settleSelection(resumed,winner,winner>=0?4u:0u));
   assert(ttcg::cardCount(loaded.player)+ttcg::cardCount(loaded.opponents[123])==10);
   assert(resumed.gold[0]==(winner<0?100:winner==0?125:75));
   assert(ttcg::count(loaded.player,b[2])==(winner==0?1:0));
   assert(ttcg::count(loaded.opponents[123],a[2])==(winner==1?1:0));
   auto settled=ttcg::encodeCollection(loaded);assert(loaded.contract.settleSelection(resumed,winner,winner>=0?4u:0u));assert(ttcg::encodeCollection(loaded)==settled);
 }
 // Loss repair leaves the middle slot empty, including after save/load.
 ttcg::Hand ordered{cards[0].form,cards[1].form,cards[2].form,cards[3].form,cards[4].form};
 ttcg::Stock remaining;for(auto id:ordered)remaining[id]=1;remaining[ordered[2]]=0;
 auto repaired=ordered;ttcg::retainOwnedDeck(repaired,remaining);
 assert(repaired[0]==ordered[0]&&repaired[1]==ordered[1]&&repaired[2]==0&&repaired[3]==ordered[3]&&repaired[4]==ordered[4]);
 auto twice=repaired;ttcg::retainOwnedDeck(twice,remaining);assert(twice==repaired);
 repaired={cards[0].form,cards[0].form,0,cards[0].form,cards[1].form};remaining[cards[0].form]=2;
 ttcg::retainOwnedDeck(repaired,remaining);assert(repaired[0]&&repaired[1]&&!repaired[2]&&!repaired[3]&&repaired[4]);
 std::cout<<"Album: persistent player/NPC collections, real-gold accounting, deck protection, partial rollback, corrupt saves and pack contents passed\n";
}
