#pragma once
#include "Exchange.h"
#include "QuestAliasText.h"
#include <mutex>

namespace exchangeIntegration {
struct View {
    unsigned serial = 0, status = 0;
    RE::ActorHandle collector;
    std::string requested, reward, requestName, rewardName;
    unsigned gold = 0;
};
inline std::mutex mutex;
inline std::map<RE::FormID, View> views;
inline std::atomic_bool recoverOnPublish{true};
inline std::set<std::pair<RE::FormID, unsigned>> confirming;
void reset();
bool beginConfirmation(RE::FormID owner, unsigned serial);
void endConfirmation(RE::FormID owner, unsigned serial);
void publish();
View snapshot(RE::TESQuest* owner);
int version(RE::StaticFunctionTag*);
int status(RE::StaticFunctionTag*, RE::TESQuest* q);
int serial(RE::StaticFunctionTag*, RE::TESQuest* q);
int gold(RE::StaticFunctionTag*, RE::TESQuest* q);
RE::Actor* collector(RE::StaticFunctionTag*, RE::TESQuest* q);
template <std::string View::* Field> std::string text(RE::StaticFunctionTag*, RE::TESQuest* q) {
    return snapshot(q).*Field;
}
void notify(RE::TESQuest* owner, const char* result);
template <class Task> void queue(RE::TESQuest* owner, Task task) {
    if (!owner) return;
    const auto form = owner->GetFormID();
    const auto generation = collectionIntegration::generation.load();
    collectionIntegration::queue([form, generation, task = std::move(task)] {
        if (generation != collectionIntegration::generation.load()) return;
        auto* q = RE::TESForm::LookupByID<RE::TESQuest>(form);
        if (!q) return;
        if (!collectionIntegration::ready || !campaign::available()) {
            notify(q, "unavailable");
            return;
        }
        task(q, campaign::persistentID(q));
    });
}
const char* resultName(ttcg::ExchangeResult r, const char* done);
void refresh(RE::StaticFunctionTag*, RE::TESQuest* q);
void refreshText(RE::StaticFunctionTag*, RE::TESQuest* q);
void accept(RE::StaticFunctionTag*, RE::TESQuest* q, int serial);
void cancel(RE::StaticFunctionTag*, RE::TESQuest* q, int serial);
bool eligible(RE::Actor* actor);
void create(RE::StaticFunctionTag*, RE::TESQuest* q, RE::Actor* actor, RE::BSFixedString requested,
            RE::BSFixedString reward, int gold);
void postLocal(RE::StaticFunctionTag*, RE::TESQuest* q, int hold);
bool delivering(const ttcg::CardExchange& e, RE::Actor* actor);
struct Confirm : RE::IMessageBoxCallback {
    RE::FormID owner = 0;
    unsigned serial = 0, warnings = 0;
    std::uint64_t generation = 0;
    RE::ActorHandle actor;
    void Run(std::uint8_t button) override {
        // A menu callback may arrive after loading another save. Check the
        // generation both before queueing and at the game-thread boundary.
        if (generation != collectionIntegration::generation.load()) return;
        auto* q = RE::TESForm::LookupByID<RE::TESQuest>(owner);
        if (!q) return;
        queue(q, [form = owner, serial = serial, warnings = warnings, handle = actor, expected = generation,
                  button](auto* q, unsigned key) {
            if (expected != collectionIntegration::generation.load()) return;
            endConfirmation(form, serial);
            if (button != 0) {
                notify(q, "declined");
                return;
            }
            auto a = handle.get();
            const auto* e = ttcg::exchange(campaign::saved, key, serial);
            if (!q->IsRunning() || !e || !delivering(*e, a.get())) {
                notify(q, "unavailable");
                return;
            }
            if (e->gold && campaign::physicalCount(RE::PlayerCharacter::GetSingleton(), campaign::goldForm) >
                               INT32_MAX - int(e->gold)) {
                notify(q, "busy");
                return;
            }
            const auto result = ttcg::completeExchange(campaign::saved, key, serial, warnings);
            if (result == ttcg::ExchangeResult::Done) {
                campaign::payPlayerGold();
            }
            notify(q, resultName(result, "completed"));
        });
    }
};
void complete(RE::StaticFunctionTag*, RE::TESQuest* q, int serial, RE::Actor* actor);
void bind(RE::BSScript::IVirtualMachine* vm);
}
