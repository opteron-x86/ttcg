#pragma once
#include "CollectionAPIService.h"
#include <nlohmann/json.hpp>

namespace collectionIntegration {
inline std::atomic_bool ready{false};
inline std::atomic_uint64_t generation{0};
inline void notify(bool available) {
    // No collection locks are held while invoking another plugin or the VM.
    TesseraCollectionChanged data{sizeof(data),TESSERA_COLLECTION_API_VERSION,ttcg::collectionAPI::store.getRevision(),available?1u:0u};
    SKSE::GetMessagingInterface()->Dispatch(TESSERA_COLLECTION_CHANGED,&data,sizeof(data),nullptr);
    if(auto* source=SKSE::GetModCallbackEventSource()) {
        SKSE::ModCallbackEvent event{RE::BSFixedString("Tessera_CollectionChanged"),RE::BSFixedString(available?"ready":"unavailable"),float(data.revision),nullptr};
        source->SendEvent(&event);
    }
}
inline void reset() {
    ready=false;++generation;
    if(ttcg::collectionAPI::store.reset())notify(false);
}
inline void publish() {
    if(!ready)return;
    if(!campaign::saveValid){reset();return;}
    if(ttcg::collectionAPI::store.refresh(campaign::saved))notify(true);
}
inline void activate() {ready=campaign::available();publish();}

// Existing game-thread work is the publication boundary. Reading the API never
// queues engine work and this adds no timer or background scan. Early-returning
// tasks also publish, so recovery, rewards and closed-window changes are covered.
template<class Task> void queue(Task task) {
    SKSE::GetTaskInterface()->AddTask([task=std::move(task)]() mutable {task();publish();});
}
inline int apiVersion(RE::StaticFunctionTag*) {return TESSERA_COLLECTION_API_VERSION;}
inline bool isReady(RE::StaticFunctionTag*) {return bool(ttcg::collectionAPI::store.acquire());}
inline int revision(RE::StaticFunctionTag*) {return int(ttcg::collectionAPI::store.getRevision());}
inline bool cardLoaded(RE::StaticFunctionTag*,RE::BSFixedString key) {
    const auto s=ttcg::collectionAPI::store.acquire();return s&&s->find(key.c_str());
}
inline bool cardPending(RE::StaticFunctionTag*,RE::BSFixedString key) {
    const auto s=ttcg::collectionAPI::store.acquire();const auto* c=s?s->find(key.c_str()):nullptr;
    return c&&(c->flags&TESSERA_CARD_PENDING);
}
inline std::vector<std::string> cardKeys(RE::StaticFunctionTag*,int offset,int limit,RE::BSFixedString setID) {
    std::vector<std::string> result;const auto s=ttcg::collectionAPI::store.acquire();
    if(!s||offset<0||limit<1)return result;
    limit=std::min(limit,128);result.reserve(limit);
    for(const auto& row:s->rows)if(setID.empty()||row.setID==setID.c_str()) {
        if(offset){--offset;continue;}
        result.push_back(row.key);if(result.size()==std::size_t(limit))break;
    }
    return result;
}
template<const char* TesseraCollectionCard::*Field>
std::string text(RE::StaticFunctionTag*,RE::BSFixedString key) {
    const auto s=ttcg::collectionAPI::store.acquire();const auto* c=s?s->find(key.c_str()):nullptr;
    return c?c->*Field:"";
}
template<const char* TesseraCollectionCard::*Field>
std::vector<std::string> strings(RE::StaticFunctionTag* tag,RE::BSFixedString key) {
    const auto value=text<Field>(tag,key);return value.empty()?std::vector<std::string>{}:nlohmann::json::parse(value).template get<std::vector<std::string>>();
}
inline int tier(RE::StaticFunctionTag*,RE::BSFixedString key) {
    const auto s=ttcg::collectionAPI::store.acquire();const auto* c=s?s->find(key.c_str()):nullptr;return c?int(c->tier):0;
}
inline bool discovered(RE::StaticFunctionTag*,RE::BSFixedString key,bool foil) {
    const auto s=ttcg::collectionAPI::store.acquire();const auto* c=s?s->find(key.c_str()):nullptr;
    return c&&(c->flags&(foil?TESSERA_CARD_FOIL_DISCOVERED:TESSERA_CARD_DISCOVERED));
}
inline unsigned copies(const TesseraCollectionCard& c,int finish) {
    return finish==0?c.standardCount+c.foilCount:finish==1?c.standardCount:finish==2?c.foilCount:0;
}
inline int ownedCount(RE::StaticFunctionTag*,RE::BSFixedString key,int finish) {
    const auto s=ttcg::collectionAPI::store.acquire();const auto* c=s?s->find(key.c_str()):nullptr;return c?int(copies(*c,finish)):0;
}
enum class Total { Cards,Owned,Discovered,Copies };
template<Total Kind> int total(RE::StaticFunctionTag*,RE::BSFixedString setID,int finish) {
    const auto s=ttcg::collectionAPI::store.acquire();if(!s||finish<0||finish>2)return 0;
    std::uint64_t result=0;
    for(const auto& row:s->rows)if(setID.empty()||row.setID==setID.c_str()) {
        if constexpr(Kind==Total::Cards)++result;
        if constexpr(Kind==Total::Owned)result+=copies(row.value,finish)>0;
        if constexpr(Kind==Total::Copies)result+=copies(row.value,finish);
        if constexpr(Kind==Total::Discovered)result+=bool(row.value.flags&(finish==2?TESSERA_CARD_FOIL_DISCOVERED:TESSERA_CARD_DISCOVERED));
    }
    return int(std::min(result,std::uint64_t(INT32_MAX)));
}
inline int cardTotal(RE::StaticFunctionTag* tag,RE::BSFixedString setID){return total<Total::Cards>(tag,setID,0);}
inline int discoveredTotal(RE::StaticFunctionTag* tag,RE::BSFixedString setID,bool foil){return total<Total::Discovered>(tag,setID,foil?2:0);}
}
