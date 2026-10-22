#pragma once
#include "Shop.h"
namespace ttcg {
// Any released non-unique card can be exchanged. Only standard copies count;
// requesting a card does not create a copy or change how it is acquired.
inline bool exchangeCard(CardID id) {
    const int i=cardIndex(id);if(i<0||isFoil(id))return false;
    const auto& c=cards[i];return !c.pending&&c.available&&!c.unique;
}
inline std::vector<CardID> exchangeRequestPool(const Stock& stock,unsigned hold) {
    std::vector<CardID> requests;
    for(const auto& c:cards)if(exchangeCard(c.form)){
        requests.push_back(c.form);
        if(regionalShopCard(c,hold))requests.push_back(c.form);
        if(count(stock,c.form)==0)requests.push_back(c.form);
    }
    return requests;
}
inline const CardExchange* exchange(const CollectionSave& s,unsigned owner) {
    auto it=s.exchanges.find(owner);return it!=s.exchanges.end()?&it->second:nullptr;
}
inline const CardExchange* exchange(const CollectionSave& s,unsigned owner,unsigned serial) {
    const auto* e=exchange(s,owner);return e&&serial&&e->serial==serial?e:nullptr;
}
enum class ExchangeResult { Done, Invalid, Busy, MissingCard, Confirm, Stale };
inline ExchangeResult postExchange(CollectionSave& s,unsigned owner,unsigned collector,unsigned ledger,CardID requested,CardID reward,unsigned gold) {
    const auto* prior=exchange(s,owner);
    if(s.contract.pending()||s.goldCredit)return ExchangeResult::Busy;
    if(!owner||!collector||!ledger||!exchangeCard(requested)||(reward&&(!exchangeCard(reward)||reward==requested))||
       (!reward&&!gold)||(reward&&gold)||gold>maximumExchangeGold||(!prior&&s.exchanges.size()>=maximumExchanges)||
       (prior&&prior->serial==INT32_MAX))return ExchangeResult::Invalid;
    if(prior&&prior->active())return ExchangeResult::Busy;
    auto rival=s.opponents.find(ledger);
    if(rival==s.opponents.end()||(reward&&count(rival->second,reward)<1))return ExchangeResult::MissingCard;
    const unsigned serial=prior?prior->serial+1:1;
    if(reward)--rival->second[reward]; // This copy cannot be played or won while offered.
    s.exchanges[owner]={serial,collector,ledger,requested,reward,gold,ExchangeStatus::Posted};
    return ExchangeResult::Done;
}
inline ExchangeResult acceptExchange(CollectionSave& s,unsigned owner,unsigned serial) {
    const auto* e=exchange(s,owner,serial);if(!e||!e->active())return ExchangeResult::Stale;
    s.exchanges.at(owner).status=ExchangeStatus::Accepted;return ExchangeResult::Done;
}
inline ExchangeResult cancelExchange(CollectionSave& s,unsigned owner,unsigned serial) {
    const auto* e=exchange(s,owner,serial);if(!e)return ExchangeResult::Stale;
    if(!e->active())return ExchangeResult::Done;
    if(e->reward){
        auto& stock=s.opponents[e->ledger];if(count(stock,e->reward)>=shopCopyLimit)return ExchangeResult::Busy;
        ++stock[e->reward]; // Also retains an unloaded expansion's reserved copy.
    }
    s.exchanges.at(owner).status=ExchangeStatus::Cancelled;return ExchangeResult::Done;
}
inline unsigned exchangeWarnings(const CollectionSave& s,const CardExchange& e){
    unsigned warnings=shopWarnings(s,e.requested,1);
    if((warnings&ShopLastHand)&&e.reward){
        auto after=s.player;--after[e.requested];++after[e.reward];
        if(playableCount(after)>=5)warnings&=~ShopLastHand;
    }
    return warnings;
}
inline ExchangeResult checkExchange(const CollectionSave& s,unsigned owner,unsigned serial,unsigned acknowledgedWarnings=0) {
    const auto* e=exchange(s,owner,serial);if(!e)return ExchangeResult::Stale;
    if(e->status==ExchangeStatus::Completed)return ExchangeResult::Done;
    if(e->status!=ExchangeStatus::Accepted)return ExchangeResult::Stale;
    if(s.contract.pending()||s.goldCredit)return ExchangeResult::Busy;
    if(!exchangeCard(e->requested)||(e->reward&&!exchangeCard(e->reward)))return ExchangeResult::Invalid;
    if(count(s.player,e->requested)<1)return ExchangeResult::MissingCard;
    const auto rival=s.opponents.find(e->ledger);
    if(rival==s.opponents.end()||count(rival->second,e->requested)>=shopCopyLimit||(e->reward&&count(s.player,e->reward)>=shopCopyLimit))return ExchangeResult::Busy;
    if(exchangeWarnings(s,*e)&~acknowledgedWarnings)return ExchangeResult::Confirm;
    return ExchangeResult::Done;
}
inline ExchangeResult completeExchange(CollectionSave& s,unsigned owner,unsigned serial,unsigned acknowledgedWarnings=0) {
    const auto result=checkExchange(s,owner,serial,acknowledgedWarnings);if(result!=ExchangeResult::Done)return result;
    auto& e=s.exchanges.at(owner);if(e.status==ExchangeStatus::Completed)return ExchangeResult::Done;
    --s.player[e.requested];++s.opponents[e.ledger][e.requested];
    if(e.reward){++s.player[e.reward];s.discovered[e.reward]=s.discovered[baseCardID(e.reward)]=1;}
    // This is a quest reward, not a withdrawal from the NPC's betting purse.
    // Use the existing saved payment credit for any interrupted gold delivery.
    s.goldCredit+=e.gold;e.status=ExchangeStatus::Completed;retainOwnedDecks(s);
    return ExchangeResult::Done;
}
}
