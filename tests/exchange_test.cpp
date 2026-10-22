#include "Exchange.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
int main(){
    std::vector<CardID> pool;for(const auto& c:cards)if(exchangeCard(c.form))pool.push_back(c.form);assert(pool.size()>20);
    const auto a=pool[0],b=pool[1];const unsigned quest=worldFormID({"Collector.esp",0x800}),npc=worldFormID({"Follower.esp",0x900});
    CollectionSave s;s.player[a]=3;s.opponents[npc][b]=2;
    assert(postExchange(s,quest,npc,npc,a,b,0)==ExchangeResult::Done);
    assert(count(s.opponents[npc],b)==1&&exchange(s,quest)->serial==1);
    assert(postExchange(s,quest,npc,npc,a,b,0)==ExchangeResult::Busy);
    const auto posted=encodeCollection(s);
    assert(!exchange(s,quest,0)); // An uninitialised ID must never match the current offer.
    assert(acceptExchange(s,quest,0)==ExchangeResult::Stale);
    assert(cancelExchange(s,quest,0)==ExchangeResult::Stale);
    assert(completeExchange(s,quest,0)==ExchangeResult::Stale&&encodeCollection(s)==posted);
    assert(completeExchange(s,quest,1)==ExchangeResult::Stale); // Must accept first.
    assert(acceptExchange(s,quest,1)==ExchangeResult::Done);
    assert(completeExchange(s,quest,1)==ExchangeResult::Done);
    assert(count(s.player,a)==2&&count(s.player,b)==1&&count(s.opponents[npc],a)==1&&s.discovered[b]==1);
    const auto paid=encodeCollection(s);
    for(int i=0;i<10;++i)assert(completeExchange(s,quest,1)==ExchangeResult::Done);
    assert(encodeCollection(s)==paid);
    CollectionSave loaded;assert(decodeCollection(paid,loaded));
    assert(completeExchange(loaded,quest,1)==ExchangeResult::Done&&encodeCollection(loaded)==paid);
    assert(postExchange(s,quest,npc,npc,a,b,0)==ExchangeResult::Done);
    assert(exchange(s,quest)->serial==2&&count(s.opponents[npc],b)==0);
    assert(completeExchange(s,quest,1)==ExchangeResult::Stale&&cancelExchange(s,quest,1)==ExchangeResult::Stale);
    assert(cancelExchange(s,quest,2)==ExchangeResult::Done&&count(s.opponents[npc],b)==1);
    assert(cancelExchange(s,quest,2)==ExchangeResult::Done&&count(s.opponents[npc],b)==1);
    // Gold is credited once, persisted, and uses the existing payment recovery.
    assert(postExchange(s,quest,npc,npc,a,0,250)==ExchangeResult::Done);
    assert(acceptExchange(s,quest,3)==ExchangeResult::Done);
    s.contract.held=true;assert(completeExchange(s,quest,3)==ExchangeResult::Busy);s.contract.held=false;
    assert(completeExchange(s,quest,3)==ExchangeResult::Done&&s.goldCredit==250);
    assert(decodeCollection(encodeCollection(s),loaded));
    assert(completeExchange(loaded,quest,3)==ExchangeResult::Done&&loaded.goldCredit==250);
    assert(postExchange(s,quest,npc,npc,a,0,250)==ExchangeResult::Busy);s.goldCredit=0;
    // Standard and foil copies are not interchangeable.
    s.player[a]=0;s.player[a|foilFlag]=1;
    assert(postExchange(s,quest,npc,npc,a,0,100)==ExchangeResult::Done);
    assert(acceptExchange(s,quest,4)==ExchangeResult::Done);
    assert(completeExchange(s,quest,4)==ExchangeResult::MissingCard&&s.player[a|foilFlag]==1);
    s.player[a]=1;s.deck[0]=a;ensureSavedDecks(s);
    assert(completeExchange(s,quest,4)==ExchangeResult::Confirm);
    assert(completeExchange(s,quest,4,ShopDeck|ShopLastHand)==ExchangeResult::Done&&s.deck[0]==0&&s.player[a|foilFlag]==1);
    s.goldCredit=0;
    for(auto id:pool)s.player[id]=0;
    s.player[a|foilFlag]=0;
    for(unsigned i=0;i<5;++i)s.player[pool[i]]=1;
    assert(postExchange(s,quest,npc,npc,a,0,100)==ExchangeResult::Done);acceptExchange(s,quest,5);
    assert(completeExchange(s,quest,5)==ExchangeResult::Confirm); // Last playable hand.
    assert(completeExchange(s,quest,5,ShopLastHand)==ExchangeResult::Done);
    s.goldCredit=0;
    {
        CollectionSave swap;for(unsigned i=0;i<5;++i)swap.player[pool[i]]=1;
        swap.opponents[npc][pool[5]]=1;
        assert(postExchange(swap,quest,npc,npc,a,pool[5],0)==ExchangeResult::Done);acceptExchange(swap,quest,1);
        assert(!(exchangeWarnings(swap,*exchange(swap,quest))&ShopLastHand));
        assert(completeExchange(swap,quest,1)==ExchangeResult::Done&&playableCount(swap.player)==5);
        swap.opponents[npc][pool[5]]=1;
        assert(postExchange(swap,quest,npc,npc,b,pool[5],0)==ExchangeResult::Done);
        swap.opponents[npc][pool[5]]=shopCopyLimit;
        assert(cancelExchange(swap,quest,2)==ExchangeResult::Busy&&exchange(swap,quest)->active());
        --swap.opponents[npc][pool[5]];
        assert(cancelExchange(swap,quest,2)==ExchangeResult::Done&&count(swap.opponents[npc],pool[5])==shopCopyLimit);
    }
    assert(postExchange(s,quest,npc,npc,a,a,0)==ExchangeResult::Invalid);
    assert(postExchange(s,quest,npc,npc,a,b,100)==ExchangeResult::Invalid);
    assert(postExchange(s,quest,npc,npc,a,0,maximumExchangeGold+1)==ExchangeResult::Invalid);
    assert(postExchange(s,quest,npc,npc,a|foilFlag,b,0)==ExchangeResult::Invalid);
    for(const auto& c:cards)if(c.unique||c.pending||!c.available)assert(!exchangeCard(c.form));
    // Removed expansions preserve a reserved reward and restore it on cancel.
    ExpansionCard e;e.key="exchange:reward";e.name="Expansion reward";e.art="reward.webp";e.rarity="Common";e.tier=1;e.standardPacks=true;e.groups="[]";e.affinities="[]";
    registerExpansion("exchange","Exchange",{e});auto custom=expansionCardID(e.key);s.opponents[npc][custom]=1;
    assert(postExchange(s,quest,npc,npc,a,custom,0)==ExchangeResult::Done);
    const auto reserve=encodeCollection(s);assert(decodeCollection(reserve,loaded));
    assert(exchange(loaded,quest)->reward==custom&&count(loaded.opponents[npc],custom)==0);
    cards.assign(baseCards.begin(),baseCards.end());expansionKeys.clear();expansionCards.clear();cardSets.clear();
    assert(decodeCollection(reserve,loaded));auto missing=exchange(loaded,quest)->reward;
    assert(missing>=missingCardBase&&missing<expansionCardBase);
    assert(cancelExchange(loaded,quest,6)==ExchangeResult::Done&&count(loaded.opponents[npc],missing)==1);
    assert(decodeCollection(encodeCollection(loaded),s));
    // Corrupt or truncated data must not partially apply an exchange.
    const auto wire=encodeCollection(s);const auto before=encodeCollection(loaded);
    for(std::size_t n=0;n<wire.size();++n){auto cut=wire;cut.resize(n);assert(!decodeCollection(cut,loaded));assert(encodeCollection(loaded)==before);}
    auto broken=wire;broken.back()=5;assert(!decodeCollection(broken,loaded));
    // Pre-addon v32 saves have no offer section and remain readable.
    CollectionSave old;auto v32=encodeCollection(old);assert(v32.back()==0);v32.pop_back();assert(decodeCollection(v32,loaded,32)&&loaded.exchanges.empty());
    // A real base Tier VII card can now be requested in every hold, delivered
    // after reloading an accepted offer, and retained by the collector.
    const auto high=std::find_if(cards.begin(),cards.end(),[](const auto& c){return c.tier==7&&!c.unique&&!c.pending&&c.available;});
    assert(high!=cards.end());const auto tierSeven=high->form;
    for(unsigned hold=1;hold<=9;++hold){
        const auto requests=exchangeRequestPool({},hold);
        assert(std::find(requests.begin(),requests.end(),tierSeven)!=requests.end());
        for(auto id:requests)assert(!cards[cardIndex(id)].unique);
    }
    CollectionSave higher;for(unsigned i=0;i<5;++i)higher.player[pool[i]]=1;
    higher.player[tierSeven]=1;higher.player[tierSeven|foilFlag]=1;higher.opponents[npc]={};
    assert(postExchange(higher,quest,npc,npc,tierSeven,0,300)==ExchangeResult::Done);
    assert(acceptExchange(higher,quest,1)==ExchangeResult::Done);
    assert(decodeCollection(encodeCollection(higher),loaded));
    assert(completeExchange(loaded,quest,1)==ExchangeResult::Done);
    assert(count(loaded.player,tierSeven)==0&&count(loaded.player,tierSeven|foilFlag)==1);
    assert(count(loaded.opponents[npc],tierSeven)==1&&loaded.goldCredit==300);
    assert(completeExchange(loaded,quest,1)==ExchangeResult::Done&&loaded.goldCredit==300);
    // Expansion authors can define non-unique Tier VIII-X cards of any rarity,
    // including quest-only cards with no pack or merchant acquisition route.
    std::vector<ExpansionCard> higherCards;
    for(int tier=8;tier<=10;++tier){
        ExpansionCard card;card.key="high-exchange:tier-"+std::to_string(tier);card.name="High-tier request";card.art="request.webp";
        card.tier=tier;card.rarity="Legendary";card.sides={8,8,8,8};card.groups="[\"Nord\"]";card.affinities="[]";
        higherCards.push_back(card);
    }
    auto unique=higherCards.back();unique.key="high-exchange:unique";unique.unique=true;higherCards.push_back(unique);
    auto unavailable=higherCards.back();unavailable.key="high-exchange:unavailable";unavailable.unique=false;higherCards.push_back(unavailable);
    auto pending=unavailable;pending.key="high-exchange:pending";higherCards.push_back(pending);
    registerExpansion("high-exchange","High-tier exchanges",std::move(higherCards));
    cards[cardIndex(expansionCardID(unavailable.key))].available=false;
    cards[cardIndex(expansionCardID(pending.key))].pending=true;
    auto requests=exchangeRequestPool({},1);
    for(int tier=8;tier<=10;++tier){
        const auto id=expansionCardID("high-exchange:tier-"+std::to_string(tier));
        // Regional preference and missing-collection preference still apply.
        assert(std::count(requests.begin(),requests.end(),id)==3);
        const auto owned=exchangeRequestPool(Stock{{id,1}},1);
        assert(std::count(owned.begin(),owned.end(),id)==2);
        CollectionSave customOffer;for(unsigned i=0;i<5;++i)customOffer.player[pool[i]]=1;
        customOffer.player[id]=1;customOffer.opponents[npc][a]=1;
        assert(postExchange(customOffer,quest,npc,npc,id,a,0)==ExchangeResult::Done);
        assert(acceptExchange(customOffer,quest,1)==ExchangeResult::Done);
        assert(completeExchange(customOffer,quest,1)==ExchangeResult::Done);
        assert(count(customOffer.player,id)==0&&count(customOffer.opponents[npc],id)==1);
        assert(count(customOffer.opponents[npc],a)==0);
    }
    for(auto key:{unique.key,unavailable.key,pending.key}){
        const auto id=expansionCardID(key);
        assert(!exchangeCard(id)&&std::find(requests.begin(),requests.end(),id)==requests.end());
        assert(postExchange(higher,quest,npc,npc,id,0,300)==ExchangeResult::Invalid);
    }
    assert(!exchangeCard(expansionCardID("high-exchange:tier-10")|foilFlag));
    assert(!exchangeCard(0)&&!exchangeCard(missingCardBase));
    std::cout<<"Collector exchanges: all non-unique tiers, reservations, transfers, consent, duplicate callbacks, save migration and removed expansions passed\n";
}
