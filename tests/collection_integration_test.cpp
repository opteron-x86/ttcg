#include "CollectionAPIService.h"
#include <cassert>
#include <functional>
#include <iostream>

// Exercise the real bridge with queued tasks and notification receivers, without
// needing a running Skyrim VM. Only the engine transport is replaced.
namespace RE {
struct StaticFunctionTag {};
struct BSFixedString : std::string {using std::string::string;};
}
namespace campaign {
inline ttcg::CollectionSave saved;
inline bool saveValid=false;
inline bool available(){return saveValid;}
}
namespace SKSE {
struct ModCallbackEvent {RE::BSFixedString eventName,strArg;float numArg;void* sender;};
struct Tasks {
    std::vector<std::function<void()>> pending;
    void AddTask(std::function<void()> task){pending.push_back(std::move(task));}
    void run(){auto work=std::move(pending);pending.clear();for(auto& task:work)task();}
};
struct Messaging {
    std::vector<TesseraCollectionChanged> changes;
    void Dispatch(unsigned type,void* data,unsigned size,void*){
        assert(type==TESSERA_COLLECTION_CHANGED&&size==sizeof(TesseraCollectionChanged));
        auto change=*static_cast<TesseraCollectionChanged*>(data);changes.push_back(change);
        // Readers are safe even when called synchronously from a notification.
        assert(bool(ttcg::collectionAPI::store.acquire())==bool(change.ready));
    }
};
struct Events {std::vector<ModCallbackEvent> changes;void SendEvent(const ModCallbackEvent* e){changes.push_back(*e);}};
inline Tasks tasks;inline Messaging messaging;inline Events events;
inline auto* GetTaskInterface(){return &tasks;}
inline auto* GetMessagingInterface(){return &messaging;}
inline auto* GetModCallbackEventSource(){return &events;}
}
namespace exchangeIntegration {
inline unsigned publications=0,resets=0;
void publish(){++publications;}
void reset(){++resets;}
}
#include "NativeCollectionAPI.h"

int main(){
    using namespace collectionIntegration;
    const auto key=ttcg::namedCardKey(ttcg::cards[0].form);
    const RE::BSFixedString ident(key.c_str());
    activate();assert(!isReady(nullptr));
    campaign::saveValid=true;activate();assert(isReady(nullptr));
    assert(SKSE::messaging.changes.size()==1&&SKSE::events.changes.back().strArg=="ready");
    assert(cardTotal(nullptr,"")==int(ttcg::cards.size()));
    auto keys=cardKeys(nullptr,0,10000,"");assert(keys.size()==128);
    assert(cardKeys(nullptr,-1,10,"").empty()&&cardKeys(nullptr,0,10,"unknown").empty());
    assert(cardKeys(nullptr,0,0,"").empty());
    assert(text<&TesseraCollectionCard::name>(nullptr,ident)==ttcg::cards[0].name);
    assert(!strings<&TesseraCollectionCard::groupsJSON>(nullptr,ident).empty());
    assert(strings<&TesseraCollectionCard::groupsJSON>(nullptr,"missing:key").empty());
    const auto pending=std::find_if(ttcg::cards.begin(),ttcg::cards.end(),[](const auto& c){return c.pending;});
    assert(pending!=ttcg::cards.end()&&cardPending(nullptr,ttcg::namedCardKey(pending->form).c_str()));
    assert(!cardPending(nullptr,"missing:key"));
    queue([]{campaign::saved.player[ttcg::cards[0].form]=4;});
    assert(ownedCount(nullptr,ident,0)==0);SKSE::tasks.run();assert(ownedCount(nullptr,ident,0)==4);
    assert(total<Total::Copies>(nullptr,"tessera",1)==4&&total<Total::Owned>(nullptr,"tessera",0)==1);
    assert(total<Total::Copies>(nullptr,"",5)==0&&ownedCount(nullptr,ident,-1)==0);
    const auto updates=SKSE::messaging.changes.size();queue([]{return;});SKSE::tasks.run();
    assert(SKSE::messaging.changes.size()==updates); // no notifications for unchanged tasks
    const auto oldGeneration=generation.load();reset();
    assert(generation.load()>oldGeneration&&!isReady(nullptr)&&SKSE::events.changes.back().strArg=="unavailable");
    queue([]{});SKSE::tasks.run();assert(!isReady(nullptr)); // no republishing during load
    campaign::saved={};activate();assert(isReady(nullptr)&&ownedCount(nullptr,ident,0)==0);
    bool staleReward=false,currentReward=false;
    queueCurrent([&]{staleReward=true;campaign::saved.player[ttcg::cards[0].form]=9;});
    reset();campaign::saved={};activate();
    queueCurrent([&]{currentReward=true;campaign::saved.player[ttcg::cards[0].form]=1;});
    SKSE::tasks.run();assert(!staleReward&&currentReward&&ownedCount(nullptr,ident,0)==1);
    campaign::saveValid=false;publish();assert(!isReady(nullptr));
    assert(SKSE::messaging.changes.size()==SKSE::events.changes.size());
    assert(exchangeIntegration::publications>0&&exchangeIntegration::resets>0);
    std::cout<<"Collection bridge: Papyrus metadata/paging/counts, queued publication, reentrant reads, reset and save readiness passed\n";
}
