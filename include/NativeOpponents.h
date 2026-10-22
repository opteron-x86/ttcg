#pragma once
#include "NpcFactionTracking.h"
// Included after NativeCollection: integration uses the same campaign ledger,
// availability checks, actor identity, and restocking as authored vanilla play.
namespace campaign {
inline RE::TESFaction *npcAvailabilityFaction{},*npcGenericFaction{};
inline std::mutex npcCacheMutex;
inline std::map<RE::FormID,int> npcAvailabilityCache;
inline std::map<RE::FormID,std::string> npcIdentityCache;
inline std::set<std::string> npcGrantedCache;
inline ttcg::NpcFactionTracking npcFactionTracking;
inline void clearNpcCache(){authoredEncounterActors.clear();npcFactionTracking.reset();std::scoped_lock lock(npcCacheMutex);npcAvailabilityCache.clear();npcIdentityCache.clear();npcGrantedCache.clear();}
inline void initNpcForms(){auto data=RE::TESDataHandler::GetSingleton();npcAvailabilityFaction=data->LookupForm<RE::TESFaction>(0x886,plugin);npcGenericFaction=data->LookupForm<RE::TESFaction>(0x887,plugin);}
inline bool npcGatesMet(RE::Actor* actor,const std::vector<ttcg::OpponentGate>& gates){
 const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::Integration);
 for(const auto& g:gates){const auto id=ttcg::worldFormID(g.form);
  if(g.kind<=1){auto quest=resolveID<RE::TESQuest>(id);if(!quest)return false;
   if(g.kind==1){if(quest->GetCurrentStageID()<g.value)return false;}
   else {bool done=false;if(quest->executedStages)for(const auto& stage:*quest->executedStages)if(stage.data.index==g.value){done=true;break;}if(!done)return false;}
  }else if(g.kind==2){auto faction=resolveID<RE::TESFaction>(id);auto subject=g.player?static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton()):actor;if(!faction||!subject||!ttcg::measureNpcSync(ttcg::NpcSyncWork::Factions,[&]{return subject->IsInFaction(faction);}))return false;}
  else {auto value=resolveID<RE::TESGlobal>(id);if(!value||value->value<float(g.value))return false;}
 }return true;
}
inline bool npcDefinitionAvailable(const ttcg::OpponentDefinition& d){
 const auto base=ttcg::worldFormID(d.actor);if(!resolveID<RE::TESNPC>(base))return false;
 if(d.reference.local){const auto ref=resolveID<RE::Actor>(ttcg::worldFormID(d.reference));if(!ref||persistentID(ref->GetActorBase())!=base)return false;}
 for(const auto& g:d.gates){const auto id=ttcg::worldFormID(g.form);if(g.kind<=1?!resolveID<RE::TESQuest>(id):g.kind==2?!resolveID<RE::TESFaction>(id):!resolveID<RE::TESGlobal>(id))return false;}
 for(const auto& info:{d.accept,d.decline,d.noCards})if(info.local&&!resolveID<RE::TESTopicInfo>(ttcg::worldFormID(info)))return false;
 ttcg::Stock required;return ttcg::resolveNpcCards(d.cards,required);
}
inline void logNpcDependencies(){
 for(const auto& d:ttcg::opponentDefinitions)if(!npcDefinitionAvailable(d))SKSE::log::warn("NPC profile {} dormant: required actor, reference, gate, dialogue INFO or card is missing/invalid",d.key);
 for(const auto& g:ttcg::npcGrantDefinitions){ttcg::Stock required;if(!resolveID<RE::TESNPC>(ttcg::worldFormID(g.actor))||!ttcg::resolveNpcCards(g.cards,required))SKSE::log::warn("NPC grant {} dormant: actor or card dependency is missing/invalid",g.key);}
}
inline std::string npcEligibilityReason(RE::Actor* actor,bool automatic=true){
 const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::Integration);
 if(!actor||actor==RE::PlayerCharacter::GetSingleton())return "Face a nearby resident.";
 auto base=actor->GetActorBase();if(!base||!actor->GetRace())return "No stable NPC identity.";
 // Dialogue flags and a voice type also occur on pets. Use Skyrim's NPC
 // classification, independently of voice origin or authored enrollment.
 if(!ttcg::measureNpcSync(ttcg::NpcSyncWork::Humanoid,[&]{return actor->IsHumanoid();}))return "Only humanoid residents can play Tessera.";
 if(actor->IsChild())return "Children use the practice roster.";
 const auto id=persistentID(base);if(ttcg::excludedNpc(id))return "Excluded by an opponent integration.";
 if(!base->IsUnique()||base->Respawns()||!actor->IsPersistent()||actor->GetFormID()>>24==0xFF)return "Requires a unique, persistent resident.";
 if(actor->IsDead()||actor->IsDisabled()||ttcg::measureNpcSync(ttcg::NpcSyncWork::Hostility,[&]{return actor->IsHostileToActor(RE::PlayerCharacter::GetSingleton());}))return "They are unavailable for games.";
 if(actor->IsSummoned()||actor->IsReanimated()||actor->IsGhost())return "This actor is not a resident.";
 if(!actor->GetRace()->AllowsPCDialogue()||!base->GetVoiceType())return "This NPC does not support ordinary dialogue.";
 const auto voiceFile=base->GetVoiceType()->GetFile(0);
 if(!ttcg::residentVoiceAllowed(voiceFile?voiceFile->GetFilename():"",ttcg::opponentDefinition(id)!=nullptr,interfaceSettings.vanillaVoicesOnly))return "This NPC uses a custom voice. Turn off Vanilla voices only in Settings or install an authored Tessera profile.";
 const std::string name=actor->GetName();if(name.empty()||name.size()>128)return "This NPC needs a name.";
 if(automatic){const auto file=base->GetFile(0);const auto master=file?ttcg::normalizedPlugin(file->GetFilename()):"";if(master.empty()||ttcg::vanillaPlugin(master))return "Vanilla residents require an authored profile or manual enrollment.";}
 return {};
}
inline bool npcTemporarilyReady(RE::Actor* actor){
 const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::ActorState);
 return actorReadyForCards(actor)&&!actor->IsBleedingOut()&&!actor->IsUnconscious()&&actor->GetLifeState()!=RE::ACTOR_LIFE_STATE::kRestrained&&actor->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kIsSleeping;
}
inline std::string npcTheme(RE::Actor* actor){
 const auto race=actor&&actor->GetRace()?actor->GetRace()->GetFormID():0;
 switch(race){case 0x13740:return "Argonian";case 0x13741:return "Breton";case 0x13742:return "Dark Elf";case 0x13743:return "High Elf";case 0x13744:return "Imperial";case 0x13745:return "Khajiit";case 0x13746:return "Nord";case 0x13747:return "Orc";case 0x13748:return "Redguard";case 0x13749:return "Wood Elf";default:return {};}
}
inline bool npcIntegrationReady(RE::Actor* actor){
 const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::Integration);
 if(!actor||!ttcg::measureNpcSync(ttcg::NpcSyncWork::Humanoid,[&]{return actor->IsHumanoid();}))return false;const auto base=persistentID(actor->GetActorBase());if(ttcg::excludedNpc(base))return false;
 const auto* d=ttcg::opponentDefinition(base);if(d&&!npcDefinitionAvailable(*d))return false;
 auto it=saved.residents.profiles.find(base);if(it==saved.residents.profiles.end())return !d||npcGatesMet(actor,d->gates);
 const auto& r=it->second;if(!r.enabled||!r.selected||r.reference!=persistentID(actor))return false;
 if(r.source==0&&interfaceSettings.residents==0)return false;
 if(r.source==2&&(!d||d->key!=r.definition))return false;
 ttcg::Stock required;if(d&&!ttcg::resolveNpcCards(d->cards,required))return false;
 return (!d||npcGatesMet(actor,d->gates))&&npcEligibilityReason(actor,false).empty();
}
inline bool registerNpcActor(RE::Actor* actor,bool force){
 if(!actor||!saveValid)return false;const auto base=persistentID(actor->GetActorBase());if(!base)return false;
 const auto* definition=ttcg::opponentDefinition(base);const int index=ttcg::opponentIndex(base);
 if(definition&&!npcDefinitionAvailable(*definition))return false;
 if(index>=0&&!ttcg::opponents[index].dynamic){
  if(ttcg::excludedNpc(base))return false;
  if(ttcg::opponents[index].spawned)authoredEncounterActors[base]=actor->GetHandle();
  return true;
 }
 if(!definition&&!force&&!interfaceSettings.residents)return false;
 if(!npcEligibilityReason(actor,!definition&&!force).empty())return false;
 if(force&&(!npcTemporarilyReady(actor)||(definition&&!npcGatesMet(actor,definition->gates))))return false;
 if(definition&&definition->reference.local&&ttcg::worldFormID(definition->reference)!=persistentID(actor))return false;
 auto location=actor->GetEditorLocation();if(!location)location=actor->GetCurrentLocation();
 const auto place=namedPlace(location);unsigned hold=0;
 constexpr std::array<RE::FormID,10> locations{0x16772,0x1676F,0x16770,0x1676E,0x1676D,0x1676C,0x1676B,0x1676A,0x16769,0xDB016E2A};
 for(unsigned depth=0;location&&depth<32;++depth,location=location->parentLoc)for(unsigned n=0;n<locations.size();++n)if(persistentID(location)==locations[n])hold=n+1;
 if(!saved.residents.seed){std::random_device rng;saved.residents.seed=rng();if(!saved.residents.seed)saved.residents.seed=1;}
 const bool fresh=!saved.residents.profiles.contains(base);
 const bool result=ttcg::createResident(saved,base,persistentID(actor),actor->GetName(),place?place:ttcg::holdName(hold),npcTheme(actor),hold,interfaceSettings.residents,force,definition);
 if(fresh&&saved.residents.profiles.contains(base)){const auto& r=saved.residents.profiles.at(base);SKSE::log::info("NPC profile: {} ({}) source {}, selected {}, skill {}, region {}",r.name,ttcg::worldKey(base),r.source,r.selected,ttcg::opponentSkillNames[r.level],ttcg::holdName(r.hold));}
 if(result&&force){const int i=ttcg::opponentIndex(base);if(i>=0&&saved.residents.profiles[base].source==1)ttcg::opponents[i].automatic=false;}
 return result;
}
inline void restoreNpcProfiles(){
 ttcg::resetRuntimeOpponents();
 for(auto& [base,r]:saved.residents.profiles){const auto* d=ttcg::opponentDefinition(base);if(d&&!npcDefinitionAvailable(*d))d=nullptr;if(d)ttcg::adoptOpponentDefinition(r,*d);if(r.selected){ttcg::registerResident(base,r,d);saved.players[base].unavailable=true;}}
}
inline void applyNpcGrants(RE::Actor* actor){
 if(!actor||saved.contract.pending())return;const auto base=persistentID(actor->GetActorBase());
 for(const auto& g:ttcg::npcGrantDefinitions)if(ttcg::worldFormID(g.actor)==base&&npcGatesMet(actor,g.gates)){
  const auto token="json:"+g.key+":"+ttcg::worldKey(base);if(saved.residents.grants.contains(token))continue;
  if(ttcg::grantNpcCards(saved,ledgerKey(actor),token,g.cards))SKSE::log::info("NPC card grant {} delivered to {}",g.key,actor->GetName());
 }
}
inline void registerNearbyNpcs(){
 if(!saveValid||!RE::PlayerCharacter::GetSingleton())return;
 // Explicit references can participate before the player visits their cell.
 for(const auto& d:ttcg::opponentDefinitions)if(d.reference.local){auto actor=resolveID<RE::Actor>(ttcg::worldFormID(d.reference));if(actor)registerNpcActor(actor,false);}
 if(auto* lists=RE::ProcessLists::GetSingleton())lists->ForAllActors([&](RE::Actor* actor){
  if(!actor||!actor->Is3DLoaded())return RE::BSContainer::ForEachResult::kContinue;
  bool assigned=false;const auto base=persistentID(actor->GetActorBase());for(const auto& g:ttcg::npcGrantDefinitions)if(ttcg::worldFormID(g.actor)==base){assigned=true;break;}
  registerNpcActor(actor,assigned);const int i=profileIndex(actor);
  if(i>=0&&assigned)seedOpponent(actor);
  return RE::BSContainer::ForEachResult::kContinue;
 });
}
inline void setNpcFaction(RE::Actor* actor,RE::TESFaction* faction,int rank){
 const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::Factions);
 if(!actor||!faction)return;if(rank<0){if(actor->IsInFaction(faction))actor->RemoveFromFaction(faction);}
 else if(!actor->IsInFaction(faction)||actor->GetFactionRank(faction,false)!=rank)actor->AddToFaction(faction,rank);
}
inline int npcAvailability(RE::Actor* actor,int knownProfile){
 const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::Readiness);
 if(!actor||!interfaceSettings.gamesEnabled||!hasAlbum()||!npcTemporarilyReady(actor)||!npcIntegrationReady(actor))return 0;
 // A supplied index belongs to this actor in the current refresh only.
 const auto i=knownProfile==-2?profileIndex(actor):knownProfile;if(i<0||!unlocked(i))return 0;
 if(resumingWith(actor))return 1;
 if(!ttcg::opponentHasCards(saved,i))return 3;
 if(!ttcg::encounterReady(saved,i,gameHour())||(!development.unlockAllPlayers&&(!ttcg::challengeReputationMet(saved,i)||!ttcg::willingToPlay(saved,i,gameHour()))))return 2;
 return 1;
}
inline bool observeNpcFactions(RE::Actor* actor,std::uint64_t generation){
 const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::Factions);
 if(!actor||!((npcAvailabilityFaction&&actor->IsInFaction(npcAvailabilityFaction))||(npcGenericFaction&&actor->IsInFaction(npcGenericFaction))))return false;
 return npcFactionTracking.observe(generation,actor->GetFormID());
}
inline void syncNpcDialogue(){
 if(!npcAvailabilityFaction)return;
 const auto generation=npcFactionTracking.generation();
 std::map<RE::FormID,int> cache;std::map<RE::FormID,std::string> identities;
 std::set<RE::FormID> dynamicReferences;
 {
 const ttcg::NpcSyncPhaseScope phase(ttcg::NpcSyncPhase::Dialogue);
 for(unsigned i=0;i<ttcg::opponents.size();++i){const auto& p=ttcg::opponents[i];auto actor=resolveOpponent(p);if(!actor)continue;
  const int actorProfile=profileIndex(actor),ready=npcAvailability(actor,actorProfile);
  {const ttcg::NpcSyncWorkScope cacheTiming(ttcg::NpcSyncWork::Cache);cache[actor->GetFormID()]=ready;identities[actor->GetFormID()]=ttcg::worldKey(p.base);}
  if(!p.dynamic||actorProfile!=int(i))continue;
  setNpcFaction(actor,npcAvailabilityFaction,ready);dynamicReferences.insert(actor->GetFormID());
  const auto* d=ttcg::opponentDefinition(p.base);setNpcFaction(actor,npcGenericFaction,d&&d->customDialogue?-1:0);
 }
 }
 const ttcg::NpcSyncPhaseScope phase(ttcg::NpcSyncPhase::Cleanup);
 // Reconcile opponent markers with the loaded roster.
 if(auto* lists=RE::ProcessLists::GetSingleton();lists&&npcFactionTracking.beginDiscovery(generation)){
  unsigned tagged=0;
  lists->ForAllActors([&](RE::Actor* actor){if(observeNpcFactions(actor,generation))++tagged;return RE::BSContainer::ForEachResult::kContinue;});
  SKSE::log::info("NPC dialogue recovery: {} actors with Tessera markers",tagged);
 }
 // Ordinary refreshes only consider actors we tagged or saw carrying a tag.
 // An unresolved reference is forgotten; its load event can rediscover it.
 for(const auto id:npcFactionTracking.reconcile(generation,dynamicReferences)){
  if(!npcFactionTracking.current(generation))break;
  if(auto* actor=RE::TESForm::LookupByID<RE::Actor>(id)){setNpcFaction(actor,npcAvailabilityFaction,-1);setNpcFaction(actor,npcGenericFaction,-1);}
 }
 const ttcg::NpcSyncWorkScope cacheTiming(ttcg::NpcSyncWork::Cache);
 std::scoped_lock lock(npcCacheMutex);npcAvailabilityCache=std::move(cache);npcIdentityCache=std::move(identities);npcGrantedCache=saved.residents.grants;
}
inline RE::TESTopicInfo* npcResponse(RE::Actor* actor,int state){
 const auto* d=actor?ttcg::opponentDefinition(persistentID(actor->GetActorBase())):nullptr;
 if(d&&d->customDialogue){const auto& form=state==1?d->accept:state==3&&d->noCards.local?d->noCards:d->decline;return form.local?resolveID<RE::TESTopicInfo>(ttcg::worldFormID(form)):nullptr;}
 return RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESTopicInfo>(state==1?0x888:state==3?0x88A:0x889,plugin);
}
inline void notifyNpcMatch(RE::Actor* actor,int winner,unsigned rules,unsigned tournament,bool forfeited){
 if(!actor)return;auto* events=SKSE::GetModCallbackEventSource();if(!events)return;
 const auto text=std::format("{{\"rules\":{},\"tournament\":{},\"forfeited\":{}}}",rules,tournament,forfeited?"true":"false");
 SKSE::ModCallbackEvent event{RE::BSFixedString("TTCG_MatchCompleted"),RE::BSFixedString(text),float(winner),actor};events->SendEvent(&event);
}
inline void notifyNpcTransfers(RE::Actor* actor,const ttcg::Contract& contract){
 if(!actor||!contract.completed)return;auto* events=SKSE::GetModCallbackEventSource();if(!events)return;
 const auto owners=contract.transfers(contract.outcome,contract.selected);
 for(unsigned n=0;n<10;++n)if(owners[n]!=int(n/5)){
  const auto id=contract.hands[n/5][n%5];const auto text=std::format("{{\"card\":{},\"foil\":{}}}",ttcg::quote(ttcg::namedCardKey(id)),ttcg::isFoil(id)?"true":"false");
  SKSE::ModCallbackEvent event{RE::BSFixedString("TTCG_CardTransferred"),RE::BSFixedString(text),owners[n]==0?1.f:-1.f,actor};events->SendEvent(&event);
 }
}
inline bool canEnrollNpc(RE::Actor* actor){
 if(!actor||!npcEligibilityReason(actor,false).empty()||!npcTemporarilyReady(actor))return false;
 const int i=profileIndex(actor);if(i>=0&&!(ttcg::opponents[i].dynamic&&ttcg::opponents[i].automatic&&interfaceSettings.residents==0))return false;
 const auto* d=ttcg::opponentDefinition(persistentID(actor->GetActorBase()));
 return !d||(d->enabled&&npcDefinitionAvailable(*d)&&npcGatesMet(actor,d->gates));
}
inline std::string npcInspector(RE::Actor* actor){
 if(!actor)return "Face a nearby resident to add them.";
 if(!actor->IsHumanoid())return npcEligibilityReason(actor,false);
 const int i=profileIndex(actor);if(i>=0){if(ttcg::opponents[i].dynamic&&ttcg::opponents[i].automatic&&interfaceSettings.residents==0)return std::string("Keep ")+actor->GetName()+" available with automatic players turned off.";return std::string(actor->GetName())+" already plays Tessera.";}
 auto reason=npcEligibilityReason(actor,false);if(!reason.empty())return reason;
 const auto* d=ttcg::opponentDefinition(persistentID(actor->GetActorBase()));
 if(d&&!npcDefinitionAvailable(*d))return "Their opponent integration has missing requirements.";
 if(d&&(!d->enabled||!npcGatesMet(actor,d->gates)))return "Their quest requirements have not been met.";
 if(!npcTemporarilyReady(actor))return "They are busy or unavailable.";
 return std::string("Add ")+actor->GetName()+" as a Tessera player.";
}
}
