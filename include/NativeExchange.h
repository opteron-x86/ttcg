#pragma once
#include "Exchange.h"
#include "QuestAliasText.h"
#include <mutex>

namespace exchangeIntegration {
struct View {
    unsigned serial=0,status=0;
    RE::ActorHandle collector;
    std::string requested,reward,requestName,rewardName;
    unsigned gold=0;
};
inline std::mutex mutex;
inline std::map<RE::FormID,View> views;
inline std::atomic_bool recoverOnPublish{true};
inline std::set<std::pair<RE::FormID,unsigned>> confirming;
inline void reset(){std::scoped_lock lock(mutex);views.clear();recoverOnPublish=true;confirming.clear();}
inline bool beginConfirmation(RE::FormID owner,unsigned serial){std::scoped_lock lock(mutex);return confirming.emplace(owner,serial).second;}
inline void endConfirmation(RE::FormID owner,unsigned serial){std::scoped_lock lock(mutex);confirming.erase({owner,serial});}
inline void publish(){
    auto& s=campaign::saved;
    if(recoverOnPublish.exchange(false)){
        for(const auto& [owner,e]:s.exchanges)if(e.active()){
            const auto* quest=campaign::resolveID<RE::TESQuest>(owner);
            const auto* actor=campaign::resolveID<RE::Actor>(e.collector);
            if(!quest||!quest->IsRunning()||!actor||actor->IsDead()||!ttcg::exchangeCard(e.requested)||(e.reward&&!ttcg::exchangeCard(e.reward)))
                ttcg::cancelExchange(s,owner,e.serial);
        }
    }
    std::map<RE::FormID,View> next;
    for(const auto& [owner,e]:s.exchanges)if(auto* quest=campaign::resolveID<RE::TESQuest>(owner)){
        View v;v.serial=e.serial;v.status=unsigned(e.status);v.gold=e.gold;
        if(auto* actor=campaign::resolveID<RE::Actor>(e.collector))v.collector=actor->GetHandle();
        v.requested=ttcg::namedCardKey(e.requested);v.reward=ttcg::namedCardKey(e.reward);
        if(int i=ttcg::cardIndex(e.requested);i>=0)v.requestName=ttcg::cards[i].name;
        if(int i=ttcg::cardIndex(e.reward);i>=0)v.rewardName=ttcg::cards[i].name;
        if(e.gold)v.rewardName=std::to_string(e.gold)+" gold";
        next.emplace(quest->GetFormID(),std::move(v));
    }
    std::scoped_lock lock(mutex);views=std::move(next);
}
inline View snapshot(RE::TESQuest* owner){
    std::scoped_lock lock(mutex);auto it=owner?views.find(owner->GetFormID()):views.end();return it==views.end()?View{}:it->second;
}
inline int version(RE::StaticFunctionTag*){return 2;}
inline int status(RE::StaticFunctionTag*,RE::TESQuest* q){return snapshot(q).status;}
inline int serial(RE::StaticFunctionTag*,RE::TESQuest* q){return snapshot(q).serial;}
inline int gold(RE::StaticFunctionTag*,RE::TESQuest* q){return snapshot(q).gold;}
inline RE::Actor* collector(RE::StaticFunctionTag*,RE::TESQuest* q){auto a=snapshot(q).collector.get();return a.get();}
template<std::string View::*Field> std::string text(RE::StaticFunctionTag*,RE::TESQuest* q){return snapshot(q).*Field;}
inline void notify(RE::TESQuest* owner,const char* result){
    collectionIntegration::publish();
    if(auto* source=SKSE::GetModCallbackEventSource()){
        const auto v=snapshot(owner);
        SKSE::ModCallbackEvent event{RE::BSFixedString("Tessera_ExchangeChanged"),RE::BSFixedString(result),float(v.serial),owner};
        source->SendEvent(&event);
    }
}
template<class Task> void queue(RE::TESQuest* owner,Task task){
    if(!owner)return;
    const auto form=owner->GetFormID();const auto generation=collectionIntegration::generation.load();
    collectionIntegration::queue([form,generation,task=std::move(task)]{
        if(generation!=collectionIntegration::generation.load())return;
        auto* q=RE::TESForm::LookupByID<RE::TESQuest>(form);if(!q)return;
        if(!collectionIntegration::ready||!campaign::available()){notify(q,"unavailable");return;}
        task(q,campaign::persistentID(q));
    });
}
inline const char* resultName(ttcg::ExchangeResult r,const char* done){
    switch(r){case ttcg::ExchangeResult::Done:return done;case ttcg::ExchangeResult::Busy:return "busy";
    case ttcg::ExchangeResult::MissingCard:return "missing-card";case ttcg::ExchangeResult::Confirm:return "changed";
    case ttcg::ExchangeResult::Stale:return "stale";default:return "unavailable";}
}
inline void refresh(RE::StaticFunctionTag*,RE::TESQuest* q){queue(q,[](auto* q,unsigned){notify(q,"refreshed");});}
inline void refreshText(RE::StaticFunctionTag*,RE::TESQuest* q){
    queue(q,[](auto* q,unsigned owner){
        const auto* offer=ttcg::exchange(campaign::saved,owner);
        if(!q->IsEnabled()||!q->IsRunning()||!offer||!offer->active()){notify(q,"unavailable");return;}
        std::vector<ttcg::AliasTextName> names;
        for(auto* alias:q->aliases){
            if(!alias||!alias->flags.all(RE::BGSBaseAlias::FLAGS::kStoreName)||alias->QType()!=RE::BSFixedString("Ref"))continue;
            auto ref=q->GetAliasedRef(alias->aliasID).get();
            // Stored alias substitutions resolve FULL on the base form, not
            // ExtraTextDisplayData/SetDisplayName on the placed reference.
            if(auto* base=ref?ref->GetBaseObject():nullptr)names.push_back({alias->aliasID,base->GetFormID()});
        }
        bool restored=false;
        {
            RE::BSWriteLockGuard lock(q->aliasAccessLock);
            restored=ttcg::refreshQuestAliasText(*q,names,[](std::uint32_t id){
                auto* memory=RE::malloc<RE::BGSQuestInstanceText>();
                if(!memory)return static_cast<RE::BGSQuestInstanceText*>(nullptr);
                auto* instance=std::construct_at(memory);
                instance->id=id;
                return instance;
            });
        }
        if(restored)q->AddChange(RE::TESQuest::ChangeFlags::kQuestInstanceData);
        notify(q,restored?"text-refreshed":"unavailable");
    });
}
inline void accept(RE::StaticFunctionTag*,RE::TESQuest* q,int serial){queue(q,[serial](auto* q,unsigned owner){if(!q->IsRunning()){notify(q,"unavailable");return;}notify(q,resultName(ttcg::acceptExchange(campaign::saved,owner,serial),"accepted"));});}
inline void cancel(RE::StaticFunctionTag*,RE::TESQuest* q,int serial){queue(q,[serial](auto* q,unsigned owner){notify(q,resultName(ttcg::cancelExchange(campaign::saved,owner,serial),"cancelled"));});}
inline bool eligible(RE::Actor* actor){
    const int i=campaign::profileIndex(actor);
    return i>=0&&!campaign::isChild(actor)&&campaign::actorReadyForCards(actor)&&campaign::npcIntegrationReady(actor)&&campaign::unlocked(i);
}
inline void create(RE::StaticFunctionTag*,RE::TESQuest* q,RE::Actor* actor,RE::BSFixedString requested,RE::BSFixedString reward,int gold){
    const auto handle=actor?actor->GetHandle():RE::ActorHandle{};
    queue(q,[handle,request=std::string(requested.c_str()),reward=std::string(reward.c_str()),gold](auto* q,unsigned owner){
        auto actor=handle.get();if(!q->IsRunning()||!eligible(actor.get())||gold<0||(!reward.empty()&&!ttcg::namedCardID(reward))){notify(q,"unavailable");return;}
        campaign::seedOpponent(actor.get());
        const auto r=ttcg::postExchange(campaign::saved,owner,campaign::persistentID(actor.get()),campaign::ledgerKey(actor.get()),ttcg::namedCardID(request),ttcg::namedCardID(reward),gold);
        notify(q,resultName(r,"posted"));
    });
}
inline void postLocal(RE::StaticFunctionTag*,RE::TESQuest* q,int hold){
    queue(q,[hold](auto* q,unsigned owner){
        auto& s=campaign::saved;
        if(!q->IsRunning()||hold<1||hold>9||!campaign::hasAlbum()){notify(q,"unavailable");return;}
        if(const auto* e=ttcg::exchange(s,owner);e&&e->active()){notify(q,"refreshed");return;}
        if(s.contract.pending()||s.goldCredit){notify(q,"busy");return;}
        std::vector<RE::Actor*> pool;
        for(const auto& p:ttcg::opponents)if(p.hold==unsigned(hold)&&!p.traveller&&!p.spawned){
            auto* a=campaign::resolveOpponent(p);if(eligible(a))pool.push_back(a);
        }
        if(pool.empty()){notify(q,"unavailable");return;}
        std::mt19937 rng(s.economyRandom^owner^campaign::gameHour());s.economyRandom=rng();
        auto* actor=pool[rng()%pool.size()];campaign::seedOpponent(actor);
        const auto ledger=campaign::ledgerKey(actor);const auto& stock=s.opponents[ledger];
        const auto requests=ttcg::exchangeRequestPool(stock,hold);
        if(requests.empty()){notify(q,"unavailable");return;}
        const auto requested=requests[rng()%requests.size()];ttcg::CardID reward=0;
        const int value=ttcg::shopPrice(requested);
        if(rng()%2==0){
            std::vector<ttcg::CardID> rewards;
            for(const auto& [id,n]:stock)if(n>0&&id!=requested&&ttcg::exchangeCard(id)&&ttcg::shopPrice(id)>=value*3/4&&ttcg::shopPrice(id)<=value*3/2)rewards.push_back(id);
            if(!rewards.empty())reward=rewards[rng()%rewards.size()];
        }
        const unsigned gold=reward?0u:unsigned(std::clamp(ttcg::shopSalePrice(requested)*3,50,2500));
        const auto r=ttcg::postExchange(s,owner,campaign::persistentID(actor),ledger,requested,reward,gold);
        notify(q,resultName(r,"posted"));
    });
}
inline bool delivering(const ttcg::CardExchange& e,RE::Actor* actor){
    auto* player=RE::PlayerCharacter::GetSingleton();
    return actor&&player&&campaign::persistentID(actor)==e.collector&&campaign::actorReadyForCards(actor)&&
        actor->GetParentCell()==player->GetParentCell()&&actor->GetPosition().GetDistance(player->GetPosition())<600.f;
}
struct Confirm:RE::IMessageBoxCallback {
    RE::FormID owner=0;unsigned serial=0,warnings=0;std::uint64_t generation=0;RE::ActorHandle actor;
    void Run(std::uint8_t button) override {
        // A menu callback may arrive after loading another save. Check the
        // generation both before queueing and at the game-thread boundary.
        if(generation!=collectionIntegration::generation.load())return;
        auto* q=RE::TESForm::LookupByID<RE::TESQuest>(owner);if(!q)return;
        queue(q,[form=owner,serial=serial,warnings=warnings,handle=actor,expected=generation,button](auto* q,unsigned key){
            if(expected!=collectionIntegration::generation.load())return;
            endConfirmation(form,serial);if(button!=0){notify(q,"declined");return;}
            auto a=handle.get();const auto* e=ttcg::exchange(campaign::saved,key,serial);
            if(!q->IsRunning()||!e||!delivering(*e,a.get())){notify(q,"unavailable");return;}
            if(e->gold&&campaign::physicalCount(RE::PlayerCharacter::GetSingleton(),campaign::goldForm)>INT32_MAX-int(e->gold)){notify(q,"busy");return;}
            const auto result=ttcg::completeExchange(campaign::saved,key,serial,warnings);
            if(result==ttcg::ExchangeResult::Done){
                campaign::Bank bank(0);auto& credit=campaign::saved.goldCredit;
                if(credit>0)credit-=std::clamp(bank.change(0,0,credit),0,credit);
            }
            notify(q,resultName(result,"completed"));
        });
    }
};
inline void complete(RE::StaticFunctionTag*,RE::TESQuest* q,int serial,RE::Actor* actor){
    const auto handle=actor?actor->GetHandle():RE::ActorHandle{};
    queue(q,[serial,handle](auto* q,unsigned owner){
        const auto* e=ttcg::exchange(campaign::saved,owner,serial);auto a=handle.get();
        if(!q->IsRunning()||!e||!delivering(*e,a.get())){notify(q,"unavailable");return;}
        if(e->status==ttcg::ExchangeStatus::Completed){notify(q,"completed");return;}
        auto& s=campaign::saved;const unsigned warnings=ttcg::exchangeWarnings(s,*e);
        const auto result=ttcg::checkExchange(s,owner,serial,warnings);
        if(result!=ttcg::ExchangeResult::Done){notify(q,resultName(result,"completed"));return;}
        if(!beginConfirmation(q->GetFormID(),serial))return;
        publish();const auto v=snapshot(q);
        std::string body="Give one standard "+v.requestName+" to "+a->GetName()+" for "+v.rewardName+"?";
        if(warnings&ttcg::ShopDeck)body+="\n\nThis copy is used in a saved deck. Its slot will be cleared.";
        if(warnings&ttcg::ShopLastHand)body+="\n\nYou will have fewer than five playable cards.";
        RE::BSTSmartPointer<RE::IMessageBoxCallback> callback(new Confirm());
        auto* c=static_cast<Confirm*>(callback.get());c->owner=q->GetFormID();c->serial=serial;c->warnings=warnings;c->generation=collectionIntegration::generation.load();c->actor=handle;
        RE::BSString message(body.c_str());RE::BSTArray<RE::BSString> buttons;buttons.emplace_back("Exchange");buttons.emplace_back("Cancel");
        if(!RE::MessageBoxMenu::Create(message,callback,0,0,10,buttons)){endConfirmation(q->GetFormID(),serial);notify(q,"busy");}
    });
}
inline void bind(RE::BSScript::IVirtualMachine* vm){
    vm->RegisterFunction("GetAPIVersion","Tessera_Exchange",version);
    vm->RegisterFunction("GetStatus","Tessera_Exchange",status);
    vm->RegisterFunction("GetOfferID","Tessera_Exchange",serial);
    vm->RegisterFunction("GetCollector","Tessera_Exchange",collector);
    vm->RegisterFunction("GetRequestedCard","Tessera_Exchange",text<&View::requested>);
    vm->RegisterFunction("GetRewardCard","Tessera_Exchange",text<&View::reward>);
    vm->RegisterFunction("GetRequestedName","Tessera_Exchange",text<&View::requestName>);
    vm->RegisterFunction("GetRewardName","Tessera_Exchange",text<&View::rewardName>);
    vm->RegisterFunction("GetGoldReward","Tessera_Exchange",gold);
    vm->RegisterFunction("CreateOffer","Tessera_Exchange",create);
    vm->RegisterFunction("PostLocalOffer","Tessera_Exchange",postLocal);
    vm->RegisterFunction("RefreshOffer","Tessera_Exchange",refresh);
    vm->RegisterFunction("RefreshText","Tessera_Exchange",refreshText);
    vm->RegisterFunction("AcceptOffer","Tessera_Exchange",accept);
    vm->RegisterFunction("CancelOffer","Tessera_Exchange",cancel);
    vm->RegisterFunction("CompleteOffer","Tessera_Exchange",complete);
}
}
