#pragma once
#include "NpcFactionTracking.h"
#include "Campaign.h"
#include "OpponentLoader.h"
#include <map>
#include <mutex>
#include <set>
// Opponents added by other mods. They use the same ledger, availability checks, actor
// identity and restocking as the authored ones; src/NpcIntegration.cpp has the code.
namespace campaign {
inline RE::TESFaction *npcAvailabilityFaction{}, *npcGenericFaction{};
inline std::mutex npcCacheMutex;
inline std::map<RE::FormID, int> npcAvailabilityCache;
inline std::map<RE::FormID, std::string> npcIdentityCache;
inline std::set<std::string> npcGrantedCache;
inline ttcg::NpcFactionTracking npcFactionTracking;
void clearNpcCache();
void initNpcForms();
bool npcGatesMet(RE::Actor* actor, const std::vector<ttcg::OpponentGate>& gates);
bool npcDefinitionAvailable(const ttcg::OpponentDefinition& d);
void logNpcDependencies();
std::string npcEligibilityReason(RE::Actor* actor, bool automatic = true);
bool npcTemporarilyReady(RE::Actor* actor);
std::string npcTheme(RE::Actor* actor);
bool npcIntegrationReady(RE::Actor* actor);
bool registerNpcActor(RE::Actor* actor, bool force = false);
void restoreNpcProfiles();
void applyNpcGrants(RE::Actor* actor);
void registerNearbyNpcs();
void setNpcFaction(RE::Actor* actor, RE::TESFaction* faction, int rank);
int npcAvailability(RE::Actor* actor, int knownProfile = -2);
bool observeNpcFactions(RE::Actor* actor, std::uint64_t generation);
void syncNpcDialogue();
RE::TESTopicInfo* npcResponse(RE::Actor* actor, int state);
void notifyNpcMatch(RE::Actor* actor, int winner, unsigned rules, unsigned tournament, bool forfeited);
void notifyNpcTransfers(RE::Actor* actor, const ttcg::Contract& contract);
bool canEnrollNpc(RE::Actor* actor);
std::string npcInspector(RE::Actor* actor);
}
