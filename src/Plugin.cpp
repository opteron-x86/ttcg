#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <atomic>
#include <chrono>
#include <format>
#include <filesystem>
#include <sstream>
#include <thread>
#include "BuildInfo.h"
#include "UIHost.h"
#include "WorldFocus.h"
#include "PresentationWait.h"
#include "PanelOpenRequest.h"
#include "Match.h"
#include "Presentation.h"
#include "ExpansionLoader.h"
#include "ExpansionRewards.h"
#include "Lesson.h"
#include "MusicPlayback.h"
#include "DropInMusic.h"
#ifdef GetObject
#undef GetObject
#endif
#include "NativeCollection.h"
#include "NativeCollectionAPI.h"
#include "NativeExchange.h"
#include "NativeShop.h"
#include "DevelopmentPresentation.h"
#include "EncounterRecords.h"

namespace {
namespace log=SKSE::log;
ttcg::ui::Host* api{};
ttcg::ui::View view{};
bool uiReady=false, visible=false, active=false, thinking=false, settling=false;
std::uint64_t session=0, revision=0;
std::atomic<std::uint64_t> epoch{0};
RE::ActorHandle opponent, pending, enrollmentTarget;
ttcg::PanelOpenRequest panelOpenRequest;
std::string panelWaitReason;
void cancelPanelRequest(){panelOpenRequest.cancel();pending={};panelWaitReason.clear();}
std::string albumSection;
std::string opponentName,screen="lobby",pendingScreen="lobby";
ttcg::HotkeyBindings hotkeys;
auto& collectionKey=hotkeys.collection;
auto& challengeKey=hotkeys.challenge;
std::string bindingCapture,bindingNotice;
unsigned heldHotkeyModifiers(){
 const auto manager=RE::BSInputDeviceManager::GetSingleton();
 const auto keyboard=manager?manager->GetKeyboard():nullptr;
 if(!keyboard)return 0;
 const auto& keys=keyboard->GetRuntimeData().curState;
 return ttcg::heldHotkeyModifiers([&](unsigned key){return key<sizeof(keys)&&(keys[key]&0x80)!=0;});
}
std::string developmentNotice;
std::string cardBackNotice;
ttcg::WorldPauseSettings worldPauseSettings;
struct PauseOption {std::string_view command,jsonKey;const wchar_t* iniKey;bool ttcg::WorldPauseSettings::* member;};
constexpr std::array pauseOptions{
 PauseOption{"pause-album","pauseWorldInAlbum",L"PauseWorldInAlbum",&ttcg::WorldPauseSettings::album},
 PauseOption{"pause-shop","pauseWorldInShops",L"PauseWorldInShops",&ttcg::WorldPauseSettings::shops},
 PauseOption{"pause-setup","pauseWorldInMatchSetup",L"PauseWorldInMatchSetup",&ttcg::WorldPauseSettings::setup},
 PauseOption{"pause-match","pauseWorldDuringMatches",L"PauseWorldDuringMatches",&ttcg::WorldPauseSettings::matches}
};
std::string interfaceNotice;
ttcg::WorldFocus worldFocus;
ttcg::PresentationWait presentationWait;
bool panelMonitoring=false;
std::atomic<std::uint64_t> unpausedWorldEpoch{0},worldMonitorGeneration{0};
std::atomic<RE::FormID> watchedOpponent{0};
RE::ActorHandle waitingOpponent;
RE::TESPackage* waitingPackage{};
bool pendingAlbum=false,challengeContext=false;
ttcg::Match match;
ttcg::Result lastResult;
ttcg::Lesson lesson;
unsigned playedRuleSounds=0;
bool playedResultSound=false,playedOpeningSound=false;
RE::BGSSoundDescriptorForm* musicDescriptor{};
std::vector<std::filesystem::path> musicFiles;
ttcg::FolderPlaylist folderPlaylist;
std::atomic<std::uint64_t> musicGeneration{0};
bool musicEnabled=true, musicPlaying=false;
ttcg::SuspendedMusic<RE::BSIMusicType> suspendedMusic;
const auto settingsPath=std::filesystem::absolute(L"Data\\SKSE\\Plugins\\TTCG.ini").wstring();
void playSound(const char* editorID) {
 log::debug("Audio request: {} (session {}, revision {})",editorID,session,revision);
 RE::PlaySound(editorID);
}
void playRuleSound(unsigned bit) {
 const RE::FormID localID=bit==1?0x813:bit==2?0x811:bit==ttcg::Legion?0x86B:bit==ttcg::Decimation?0x86C:0x812;
 auto descriptor=RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSSoundDescriptorForm>(localID,"Tessera TCG.esp");
 auto audio=RE::BSAudioManager::GetSingleton();
 if(!descriptor||!descriptor->soundDescriptor||!audio) {
   log::error("Rule sound unavailable: {:06X}",localID); return;
 }
 const bool started=audio->Play(descriptor);
 if(started)log::debug("Rule sound {:08X}: playback submitted",descriptor->GetFormID());
 else log::warn("Rule sound {:08X}: playback failed",descriptor->GetFormID());
}
void publish();
std::uint64_t musicTime() {
 return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void scanMusicFolder() {
 auto scan=ttcg::scanMusic(std::filesystem::path(L"Data\\Music\\TTCG"));
 musicFiles=std::move(scan.files);
 log::debug("Music folder: {} tracks, {} duplicate copies, {} rejected files",musicFiles.size(),scan.duplicates,scan.rejected.size());
 for(const auto& path:scan.rejected) log::warn("Invalid XWM: {}",path.string());
}
struct MusicAudio {
 RE::BSSoundHandle handle;
 std::string path;
 bool play(const std::filesystem::path& file) {
   auto audio=RE::BSAudioManager::GetSingleton();
   auto definition=musicDescriptor?skyrim_cast<RE::BGSStandardSoundDef*>(musicDescriptor->soundDescriptor):nullptr;
   auto defaults=RE::BGSDefaultObjectManager::GetSingleton();
   auto category=defaults?defaults->GetObject<RE::BGSSoundCategory>(RE::DEFAULT_OBJECTS::kMusicSoundCategory):nullptr;
   if(!audio||!definition||!category||!ttcg::validXwm(file)) return false;
   path=file.lexically_relative(std::filesystem::path(L"Data")).string();
   std::replace(path.begin(),path.end(),'/','\\');
   RE::BSResource::ID id{}; id.GenerateFromPath(path.c_str());
   // This dedicated descriptor is private to the one-at-a-time playlist.
   // Resource IDs are Data-relative, including Music\\TTCG\\.
   definition->soundFiles.clear(); definition->soundFiles.push_back(id);
   definition->category=category;
   definition->soundCharacteristics.staticAttenuation=0;
   handle=RE::BSSoundHandle{};
   const bool built=audio->GetSoundHandle(handle,musicDescriptor);
   const bool submitted=built&&handle.Play();
   if(submitted)log::debug("Direct music: {} (built {}, submitted {}, handle {})",path,built,submitted,handle.soundID);
   else log::warn("Music playback failed: {} (built {})",path,built);
   return submitted;
 }
 bool playing() { return handle.IsValid()&&handle.IsPlaying(); }
 void stop() { if(handle.IsValid()) handle.Stop(); handle=RE::BSSoundHandle{}; }
 void failed() { log::warn("Music did not start within 3 seconds: {}",path); }
 void suspend() {
   log::debug("Music playing: {}",path);
   auto manager=RE::BSMusicManager::GetSingleton();
   if(!manager) return;
   const bool paused=suspendedMusic.suspend(manager->current,static_cast<RE::BSIMusicType*>(nullptr),
     [](auto* music) { return music->typeStatus==RE::BSIMusicType::MUSIC_STATUS::kPlaying; },
     [](auto* music) { music->DoPause(); return music->typeStatus==RE::BSIMusicType::MUSIC_STATUS::kPaused; });
   if(paused) log::debug("Location music paused");
 }
 void restore() {
   auto manager=RE::BSMusicManager::GetSingleton();
   const bool restored=suspendedMusic.restore([manager](auto* music) { return manager&&manager->current==music; },
     [](auto* music) { return music->typeStatus==RE::BSIMusicType::MUSIC_STATUS::kPaused; },
     [](auto* music) { music->DoPlay(); });
   if(restored) log::debug("Location music resumed");
 }

} musicAudio;
void stopMusic() {
 ++musicGeneration;
 folderPlaylist.stop(musicAudio); musicPlaying=false;
}
void monitorMusic() {
 const auto generation=++musicGeneration;
 auto pendingTick=std::make_shared<std::atomic<bool>>(false);
 std::thread([generation,pendingTick]() {
   while(musicGeneration.load()==generation) {
     std::this_thread::sleep_for(std::chrono::milliseconds(250));
     if(musicGeneration.load()!=generation) break;
     if(pendingTick->exchange(true)) continue;
     collectionIntegration::queue([generation,pendingTick]() {
       pendingTick->store(false);
       if(musicGeneration.load()!=generation) return;
       folderPlaylist.tick(musicAudio,musicTime());
       musicPlaying=folderPlaylist.active();
       if(!musicPlaying) { ++musicGeneration; log::warn("Playlist stopped: no playable tracks"); publish(); }
     });
   }
 }).detach();
}
void updateMusic() {
 const bool wanted=musicEnabled&&visible&&(screen=="lobby"||challengeContext)&&musicDescriptor&&!musicFiles.empty();
 if(wanted==musicPlaying) return;
 if(!wanted) { stopMusic(); return; }
 musicPlaying=folderPlaylist.start(musicFiles,musicAudio,musicTime());
 if(musicPlaying) monitorMusic();
 else log::error("Could not start any music from Data/Music/TTCG");
}

void close(bool unwind=true);
void runAI();
void checkpointMatch() {
 if(!active)return;
 auto actor=opponent.get();if(!actor)return;
 auto& saved=campaign::savedMatch;
 saved.present=true;saved.opponent=campaign::persistentID(actor.get());saved.ledger=campaign::ledgerKey(actor.get());
 saved.tournament=campaign::tournamentID;saved.table=match;saved.practice=campaign::practice;
 saved.competitive=campaign::competitiveMatch;saved.culture=campaign::cultureGame;
 saved.waiting=bool(waitingPackage);saved.waitPackage=waitingPackage?campaign::persistentID(waitingPackage):0;
}
void releaseOpponent() {
 auto actor=waitingOpponent.get();auto package=waitingPackage;
 waitingOpponent={};waitingPackage=nullptr;
 if(package){campaign::savedMatch.waiting=false;campaign::savedMatch.waitPackage=0;}
 if(actor&&package&&actor->GetCurrentPackage()==package){actor->EndInterruptPackage(false);actor->EvaluatePackage();}
}
void waitOpponent() {
 if(waitingPackage||!active||!worldFocus.unpaused())return;
 auto actor=opponent.get();auto player=RE::PlayerCharacter::GetSingleton();
 // Tournament opponents need not be physically present. Leave remote actors' AI alone.
 if(!actor||!player||actor->IsDead()||actor->IsInCombat()||actor->GetCurrentScene()||
    actor->GetParentCell()!=player->GetParentCell()||actor->GetPosition().GetDistance(player->GetPosition())>=600.f)return;
 auto previous=actor->GetCurrentPackage();if(previous&&previous->packData.packType==RE::PACKAGE_TYPE::kDoNothing)return;
 actor->InitiateDoNothingPackage();auto package=actor->GetCurrentPackage();
 if(package&&package!=previous&&package->packData.packType==RE::PACKAGE_TYPE::kDoNothing){waitingOpponent=actor->GetHandle();waitingPackage=package;checkpointMatch();}
}
void releaseSavedOpponent() {
 auto& saved=campaign::savedMatch;
 if(saved.present&&saved.waiting)if(auto actor=campaign::resolveID<RE::Actor>(saved.opponent)){
  auto package=actor->GetCurrentPackage();
  if(package&&package->packData.packType==RE::PACKAGE_TYPE::kDoNothing&&campaign::persistentID(package)==saved.waitPackage){actor->EndInterruptPackage(false);actor->EvaluatePackage();}
 }
 saved.waiting=false;saved.waitPackage=0;
}
void interruptPanel() {
 if(!visible)return;
 const bool interrupted=active;checkpointMatch();close(!interrupted);
 if(interrupted)RE::SendHUDMessage::ShowHUDMessage("Tessera game interrupted. You can resume it later.");
}
bool checkLiveWorld() {
 if(!visible||!worldFocus.unpaused())return true;
 auto player=RE::PlayerCharacter::GetSingleton();auto actor=opponent.get();
 if(!player||player->IsDead()||player->IsInCombat()){interruptPanel();return false;}
 if(active){
  if(campaign::tournamentID&&!match.finished()){
   const auto* event=ttcg::tournament(campaign::saved,campaign::tournamentID);
   if(!event||!ttcg::tournamentOpen(*event,campaign::gameHour())){
    releaseOpponent();campaign::resolveSavedMatch(false);active=false;close(false);
    RE::SendHUDMessage::ShowHUDMessage("The tournament has ended.");return false;
   }
  }
  if(!actor||actor->IsDead()||actor->IsDisabled()){
   // A completed table already owns its result. Preserve its prize screen;
   // ordinary recovery can settle any remaining stakes when the album opens.
   if(match.finished()){interruptPanel();return false;}
   releaseOpponent();campaign::resolveSavedMatch(false);active=false;close(false);
   RE::SendHUDMessage::ShowHUDMessage("Your Tessera game has been canceled.");return false;
  }
  if(actor->IsInCombat()||actor->IsHostileToActor(player)||actor->GetCurrentScene()||
     (!campaign::tournamentID&&(actor->GetParentCell()!=player->GetParentCell()||actor->GetPosition().GetDistance(player->GetPosition())>=900.f))){interruptPanel();return false;}
 }else if(screen=="shop"||challengeContext){
  // Setup and shops have no live table to save. Close if their NPC leaves or
  // becomes unavailable, including while browsing an album from match setup.
  if(!campaign::actorReadyForCards(actor.get())||(!campaign::tournamentID&&
     (actor->GetParentCell()!=player->GetParentCell()||actor->GetPosition().GetDistance(player->GetPosition())>=900.f))){interruptPanel();return false;}
 }
 return true;
}
void monitorWorld() {
 const auto generation=++worldMonitorGeneration;
 auto pendingTick=std::make_shared<std::atomic<bool>>(false);
 std::thread([generation,pendingTick](){
  while(worldMonitorGeneration.load()==generation){
   std::this_thread::sleep_for(std::chrono::milliseconds(250));
   if(worldMonitorGeneration.load()!=generation)break;
   if(pendingTick->exchange(true))continue;
   collectionIntegration::queue([generation,pendingTick](){
    pendingTick->store(false);
    if(worldMonitorGeneration.load()!=generation||!visible||!checkLiveWorld())return;
    if(presentationWait.expired(session,revision,active?(settling?1u:thinking?2u:0u):0u,musicTime())){
     log::error("Match stalled: session {}, revision {}, placed {}, turn {}, settling {}, thinking {}; preserving table and releasing UI",session,revision,match.placed,match.turn,settling,thinking);
     interruptPanel();
    }
   });
  }
 }).detach();
}
bool updateWorldFocus() {
 const auto result=worldFocus.apply(*api,view,ttcg::shouldPauseWorld(worldPauseSettings,screen,active));
 const auto token=result==ttcg::FocusUpdate::Ready&&worldFocus.unpaused()?epoch.load()+1:0;
 unpausedWorldEpoch.store(token);
 // Paused matches also need recovery if presentation or AI stops responding.
 if(!panelMonitoring){panelMonitoring=true;presentationWait.reset();monitorWorld();}
 watchedOpponent.store(token&&(active||challengeContext||screen=="shop")&&opponent.get()?opponent.get()->GetFormID():0);
 if(result==ttcg::FocusUpdate::Lost){interruptPanel();return false;}
 if(result==ttcg::FocusUpdate::Releasing) {
   const auto token=epoch.load();
   // The native menu queued its release first. Cross that UI-task boundary before
   // returning to the game thread to acquire the new mode and publish state.
   SKSE::GetTaskInterface()->AddUITask([token]() {
     collectionIntegration::queue([token]() {
       if(epoch.load()!=token||!visible||!worldFocus.switching())return;
       auto player=RE::PlayerCharacter::GetSingleton();
       if(!player||player->IsDead()||player->IsInCombat()||!worldFocus.complete(*api,view)){interruptPanel();return;}
       publish();runAI();
     });
   });
 }
 if(result==ttcg::FocusUpdate::Ready){waitOpponent();return checkLiveWorld();}
 return false;
}
void publish() {
 if(!api||!uiReady||!visible) return;
 if(!updateWorldFocus())return;
 campaign::updateRegionalRules();
 auto data=ttcg::snapshotJson(match,session,revision,opponentName,active,thinking,lastResult,settling,musicEnabled,(musicDescriptor&&!musicFiles.empty()));
 auto actor=opponent.get();
 const bool conceal=ttcg::concealRivalHand(match,active);
 data.pop_back(); data+=",\"collection\":"+campaign::json(screen=="shop"?nullptr:actor.get(),screen=="lobby",conceal)+",\"screen\":"+ttcg::quote(screen)+",\"revealCards\":"+(campaign::development.revealAllCards?"true":"false")+"}";
 data.pop_back();data+=",\"albumSection\":"+ttcg::quote(albumSection)+"}";
 data.pop_back();data+=",\"matchContext\":"+std::string(challengeContext?"true":"false")+"}";
 if(screen=="shop"){data.pop_back();data+=",\"shop\":"+campaign::shopJson(actor.get())+"}";}
 if(screen=="lesson"){data.pop_back();data+=",\"lesson\":"+lesson.json()+"}";}
 data.pop_back();data+=std::format(",\"settings\":{{\"collection\":{},\"challenge\":{},\"capturing\":{},\"notice\":{},\"cardBackNotice\":{},\"interfaceNotice\":{},\"development\":{}",collectionKey,challengeKey,ttcg::quote(bindingCapture),ttcg::quote(bindingNotice),ttcg::quote(cardBackNotice),ttcg::quote(interfaceNotice),ttcg::developmentJson(campaign::development,developmentNotice));
 data+=",\"toggleGames\":"+std::to_string(hotkeys.toggleGames);
 data+=",\"heldModifiers\":"+std::to_string(heldHotkeyModifiers());
 for(const auto& f:ttcg::InterfaceSettings::fields){const int value=campaign::interfaceSettings.get(f);data+=","+ttcg::quote(f.json)+":"+(f.boolean()?std::string(value?"true":"false"):std::to_string(value));}
 auto enroll=enrollmentTarget.get();const auto candidate=enroll.get();
 const bool canEnroll=campaign::canEnrollNpc(candidate);
 data+=",\"residentCandidate\":"+ttcg::quote(campaign::npcInspector(candidate))+",\"canAddResident\":"+(canEnroll?"true":"false");
 for(const auto& option:pauseOptions)data+=","+ttcg::quote(option.jsonKey)+":"+(worldPauseSettings.*option.member?"true":"false");
 data+="}}";
 const auto& saved=campaign::savedMatch;
 data.pop_back();data+=",\"interruptedMatch\":";
 if(saved.present&&!active){const auto* e=ttcg::tournament(campaign::saved,saved.tournament);
  data+="{\"opponent\":"+ttcg::quote(campaign::savedMatchName())+",\"tournament\":"+std::to_string(saved.tournament)+",\"finished\":"+(saved.table.finished()?"true":"false")+",\"venue\":"+ttcg::quote(e?ttcg::tournamentVenues[e->hold-1].name:"")+",\"canResume\":"+(campaign::interfaceSettings.gamesEnabled&&e&&campaign::tournamentVenue()==e->hold?"true":"false")+"}";
 }else data+="null";
 data+="}";
 api->State(view,data);albumSection.clear();
}
void close(bool unwind) {
 if(!unwind)checkpointMatch();
 campaign::updateRegionalRules();campaign::cultureHeldHold=0;
 unpausedWorldEpoch.store(0);watchedOpponent.store(0);++worldMonitorGeneration;panelMonitoring=false;presentationWait.reset();releaseOpponent();
 campaign::closeShop();
 if(unwind&&active)campaign::savedMatch={};
 if(!campaign::savedMatch.present)ttcg::forfeitTournamentFoils(campaign::saved);
 if(unwind&&campaign::tournamentID&&active&&!match.finished())ttcg::withdrawTournament(campaign::saved,campaign::tournamentID);
 campaign::tournamentID=0;campaign::tournamentRegistration=0;albumSection.clear();
 pendingAlbum=false;challengeContext=false;bindingCapture.clear();bindingNotice.clear();developmentNotice.clear();cardBackNotice.clear();interfaceNotice.clear();
 if(unwind&&!campaign::savedMatch.present){ttcg::abandonRuleCulture(campaign::saved,campaign::gameHour());campaign::recover();}
 campaign::cultureGame=false;
 stopMusic();
 ++epoch; ++session; revision=0; cancelPanelRequest(); opponent={};
 visible=false; active=false; thinking=false; settling=false; screen="lobby"; pendingScreen="lobby";worldFocus.reset();
 if(api&&api->IsValid(view)) { if(uiReady) api->Reset(view); if(api->HasFocus(view)) api->Unfocus(view); api->Hide(view); }
 collectionIntegration::publish();
}
bool eligible(RE::Actor* actor) {
 auto player=RE::PlayerCharacter::GetSingleton();
 return actor&&player&&actor!=player&&!player->IsDead()&&!player->IsInCombat()&&campaign::actorReadyForCards(actor)
   &&actor->GetParentCell()==player->GetParentCell()&&actor->GetPosition().GetDistance(player->GetPosition())<600.0F;
}
bool worldReady(){
 auto player=RE::PlayerCharacter::GetSingleton();auto ui=RE::UI::GetSingleton();
 return player&&ui&&api&&uiReady&&!visible&&!player->IsDead()&&!player->IsInCombat()&&!player->IsSneaking()&&!player->GetCurrentScene()&&!ui->GameIsPaused()&&!ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)&&!ui->IsMenuOpen(RE::MainMenu::MENU_NAME)&&!api->HasAnyActiveFocus();
}
void showPanel(RE::Actor* actor,const std::string& requested);
bool persistInterfaceSetting(const char* section,const char* key,int value){
 const std::string a(section),b(key);const std::wstring wideSection(a.begin(),a.end()),wideKey(b.begin(),b.end());
 return WritePrivateProfileStringW(wideSection.c_str(),wideKey.c_str(),std::to_wstring(value).c_str(),settingsPath.c_str())!=0;
}
bool setGamesEnabled(bool enabled){
 if(!campaign::interfaceSettings.set("games",enabled,persistInterfaceSetting)){
  interfaceNotice="Couldn't save this setting.";
  if(visible)publish();else RE::SendHUDMessage::ShowHUDMessage(interfaceNotice.c_str());
  return false;
 }
 interfaceNotice.clear();campaign::syncGameAvailability();
 if(!enabled){
  cancelPanelRequest();
  if(active){
   close(false);RE::SendHUDMessage::ShowHUDMessage("Tessera disabled. Your game is saved to resume later.");return true;
  }
  if(visible&&(screen!="album"||challengeContext)){
   campaign::closeShop();opponent={};opponentName.clear();challengeContext=false;
   campaign::tournamentID=0;campaign::tournamentRegistration=0;screen="album";albumSection.clear();
   campaign::prepare(nullptr,true);++epoch;++session;revision=0;updateMusic();
  }
 }
 if(visible)publish();else RE::SendHUDMessage::ShowHUDMessage(enabled?"Tessera enabled.":"Tessera disabled.");
 return true;
}
void logNpcSyncTiming(const ttcg::NpcSyncTiming& timing,std::string_view source){
 if(!spdlog::should_log(spdlog::level::debug))return;
 std::string phases,work;
 for(unsigned i=0;i<timing.phases.size();++i){
  if(i)phases+=", ";phases+=std::format("{} {:.3f}",ttcg::npcSyncPhaseNames[i],ttcg::NpcSyncTiming::ms(timing.phases[i]));
 }
 for(unsigned i=0;i<timing.work.size();++i){
  if(i)work+=", ";work+=std::format("{} {:.3f}/{}",ttcg::npcSyncWorkNames[i],ttcg::NpcSyncTiming::ms(timing.work[i]),timing.calls[i]);
 }
 log::debug("NPC sync profile [{}]: {:.3f} ms; phases: {} ({} opponents, {} clock reads)",source,ttcg::NpcSyncTiming::ms(timing.total),phases,ttcg::opponents.size(),timing.clockReads);
 log::debug("NPC sync work [{}]: {} (exclusive ms/calls)",source,work);
}
void backgroundUpdate(){
 if(worldReady()&&campaign::available()){
  // Detailed profiling is opt-in. Keep only coarse slow-update timing during
  // normal play; diagnostic sessions sample the first and every twelfth refresh.
  static std::atomic_uint profileCount{0};
  const bool diagnostics=spdlog::should_log(spdlog::level::debug);
  const bool profile=diagnostics&&profileCount.fetch_add(1,std::memory_order_relaxed)%12==0;
  ttcg::NpcSyncTiming syncTiming;
  const auto started=std::chrono::steady_clock::now();
  campaign::registerNearbyNpcs();const auto registered=std::chrono::steady_clock::now();
  campaign::syncProgression(profile?&syncTiming:nullptr);const auto refreshed=std::chrono::steady_clock::now();
  campaign::updateTournaments();const auto finished=std::chrono::steady_clock::now();
  auto ms=[](auto duration){return std::chrono::duration<double,std::milli>(duration).count();};
  if(ms(finished-started)>=16.0)log::warn("Slow background update: {:.1f} ms (residents {:.1f}, progression {:.1f}, tournaments {:.1f}; {} opponents)",ms(finished-started),ms(registered-started),ms(refreshed-registered),ms(finished-refreshed),ttcg::opponents.size());
  if(profile)logNpcSyncTiming(syncTiming,"background");
  // Successful summaries are diagnostic-only, including their accumulation
  // and mutex. These measure native work, not frame time or task-queue delay.
  if(diagnostics){
   static std::mutex timingsMutex;
   static std::array<double,4> totals{};
   static double peak=0;
   static unsigned samples=0;
   std::scoped_lock timingLock(timingsMutex);
   const std::array<double,4> elapsed{ms(finished-started),ms(registered-started),ms(refreshed-registered),ms(finished-refreshed)};
   for(unsigned i=0;i<totals.size();++i)totals[i]+=elapsed[i];peak=std::max(peak,elapsed[0]);
   if(++samples==12){
    log::debug("Background update summary: {} samples, average {:.2f} ms, peak {:.2f} ms (residents {:.2f}, progression {:.2f}, tournaments {:.2f}; {} opponents)",samples,totals[0]/samples,peak,totals[1]/samples,totals[2]/samples,totals[3]/samples,ttcg::opponents.size());
    totals={};peak=0;samples=0;
   }
  }
 }
}
void backgroundUpdatePapyrus(RE::StaticFunctionTag*){
 // Queue one refresh for the current save.
 static std::atomic_bool queued{false};
 if(queued.exchange(true))return;
 const auto generation=campaign::npcFactionTracking.generation();
 collectionIntegration::queue([generation](){
  struct Complete { ~Complete(){queued.store(false);} } complete;
  if(campaign::npcFactionTracking.current(generation))backgroundUpdate();
 });
}
namespace legacyPapyrus {
RE::Actor* nextMaster(RE::StaticFunctionTag*){return nullptr;}
void masterArrived(RE::StaticFunctionTag*,RE::Actor*){}
void masterResponse(RE::StaticFunctionTag*,RE::Actor*,bool){}
}
bool resumeMatch() {
 if(!campaign::interfaceSettings.gamesEnabled||!campaign::recoverSavedMatch())return false;
 const auto saved=campaign::savedMatch;auto actor=campaign::resolveID<RE::Actor>(saved.opponent);
 auto player=RE::PlayerCharacter::GetSingleton();
 const auto* event=ttcg::tournament(campaign::saved,saved.tournament);
 if(!player||player->IsDead()||player->IsInCombat()||!actor||!campaign::actorReadyForCards(actor)||
    (saved.tournament?(!event||campaign::tournamentVenue()!=event->hold):!eligible(actor)))return false;
 if(!api||!uiReady||!api->IsValid(view)||(!visible&&api->HasAnyActiveFocus()))return false;
 campaign::closeShop();
 campaign::tournamentID=saved.tournament;campaign::sessionRules=saved.table.rules;
 campaign::practice=saved.practice;campaign::competitiveMatch=saved.competitive;campaign::cultureGame=saved.culture;
 const auto* profile=campaign::profile(actor);
 campaign::cultureHeldHold=!saved.tournament&&!(profile&&(profile->personalRules||profile->traveller))?campaign::actorHold(actor):0;
 campaign::rivalDeck=saved.originals()[1];campaign::canStake=saved.competitive&&!saved.tournament;
 opponent=actor->GetHandle();opponentName=actor->GetName();match=saved.table;
 // Resume the settled board and advance a pending redeal.
 const bool redealt=match.redeal();
 active=true;visible=true;thinking=false;settling=match.finished()||redealt;
 lastResult={};playedRuleSounds=0;playedResultSound=false;playedOpeningSound=false;
 screen="lobby";challengeContext=true;albumSection.clear();campaign::notice.clear();
 ++epoch;++session;revision=0;checkpointMatch();ttcg::refreshCardArt();scanMusicFolder();api->Show(view);
 updateMusic();publish();runAI();return true;
}
void showPanel(RE::Actor* actor,const std::string& requested="lobby") {
 if(actor)campaign::registerNpcActor(actor);
 auto player=RE::PlayerCharacter::GetSingleton();
 if(!visible){auto pick=RE::CrosshairPickData::GetSingleton();auto ref=pick?pick->GetActiveTarget().get():RE::NiPointer<RE::TESObjectREFR>{};auto target=ref?ref->As<RE::Actor>():nullptr;enrollmentTarget=target?target->GetHandle():RE::ActorHandle{};}
 const std::string target=(requested=="tournaments"||requested=="registration")?"album":requested;
 if(target!="album"&&target!="lobby"&&target!="shop")return;
 if(!campaign::interfaceSettings.gamesEnabled&&requested!="album")return;
 if(visible||!player||player->IsDead()||player->IsInCombat()||(actor&&!eligible(actor))||(!actor&&target!="album")) return;
 if(!api||!uiReady||!api->IsValid(view)) { RE::SendHUDMessage::ShowHUDMessage(std::format("Tessera: {} is unavailable.",ttcg::ui::name).c_str()); return; }
 if(api->HasAnyActiveFocus()) { RE::SendHUDMessage::ShowHUDMessage("Close the other menu first."); return; }
 campaign::recoverSavedMatch();
 if(campaign::savedMatch.present&&target=="shop"){RE::SendHUDMessage::ShowHUDMessage("Finish your interrupted game before trading cards.");return;}
 if(campaign::savedMatch.present&&target=="lobby"){
  if(campaign::resumingWith(actor)&&resumeMatch())return;
  RE::SendHUDMessage::ShowHUDMessage(("Finish your game with "+campaign::savedMatchName()+" first.").c_str());return;
 }
 if(campaign::savedMatch.present&&campaign::savedMatch.tournament&&requested=="registration"){
  const auto* e=ttcg::tournament(campaign::saved,campaign::savedMatch.tournament);const auto* p=campaign::profile(actor);
  if(e&&p&&campaign::tournamentVenue()==e->hold&&std::string_view(p->name)==ttcg::tournamentVenues[e->hold-1].host&&resumeMatch())return;
 }
 if(target=="lobby"&&actor&&!campaign::profile(actor)&&!campaign::isChild(actor)&&!campaign::development.allowAnyNPC) { RE::SendHUDMessage::ShowHUDMessage("They don't play Tessera.");return; }
 albumSection=(requested=="tournaments"||requested=="registration")?"tournaments":"";campaign::tournamentID=0;campaign::tournamentRegistration=0;
 if(requested=="registration"&&actor){const auto* p=campaign::profile(actor);const auto venue=campaign::tournamentVenue();if(p&&venue&&std::string_view(p->name)==ttcg::tournamentVenues[venue-1].host)campaign::tournamentRegistration=venue;}
 campaign::syncProgression();
 if(target=="lobby"&&!campaign::canChallenge(actor)) {
   RE::SendHUDMessage::ShowHUDMessage(campaign::hasAlbum()?"They are unavailable for a game.":"Buy a Tessera Album from a general-goods merchant.");return;
 }
 if(target=="lobby")campaign::acceptChallenge(actor);
 campaign::updateTournaments();
 if(requested=="tournaments")ttcg::discoverAllTournaments(campaign::saved,campaign::gameHour());
 if(!campaign::prepare(target=="lobby"?actor:nullptr,true)) { RE::SendHUDMessage::ShowHUDMessage(campaign::notice.c_str()); return; }
 if(target=="shop"&&!campaign::openShop(actor)){RE::SendHUDMessage::ShowHUDMessage("Card trading is unavailable here.");return;}
 opponent=actor?actor->GetHandle():RE::ActorHandle{}; opponentName=actor?actor->GetName():"";
 screen=target;challengeContext=screen=="lobby";
 ++session; revision=0; active=false; thinking=false; lastResult={}; settling=false;
 match=ttcg::Match(1,campaign::sessionRules); visible=true;
 ttcg::refreshCardArt(); scanMusicFolder(); api->Show(view);
 if(!updateWorldFocus())return;
 updateMusic(); publish();
 log::info("TTCG {}: {}",screen,opponentName);
}
void logPanelFailure(RE::Actor* actor,const std::string& requested,std::uint64_t elapsed,const char* reason) {
 const auto scene=actor?actor->GetCurrentScene():nullptr;
 const auto package=actor?actor->GetCurrentPackage():nullptr;
 const auto player=RE::PlayerCharacter::GetSingleton();const auto playerScene=player?player->GetCurrentScene():nullptr;
 log::warn("Panel request {} for {} ({:08X}) failed after {} ms: {}; scene {:08X} (playing {}, stopping {}), quest {:08X}, package {:08X} (type {}), player scene {:08X}",
  requested,actor?actor->GetName():"unavailable NPC",actor?actor->GetFormID():0,elapsed,reason,
  scene?scene->GetFormID():0,scene?scene->isPlaying:false,scene?scene->isShuttingDown:0,
  scene&&scene->parentQuest?scene->parentQuest->GetFormID():0,package?package->GetFormID():0,package?int(package->packData.packType.underlying()):-1,playerScene?playerScene->GetFormID():0);
}
void finishPanelRequest(std::uint64_t token) {
 if(!panelOpenRequest.active(token))return;
 auto actor=pending.get();auto player=RE::PlayerCharacter::GetSingleton();auto ui=RE::UI::GetSingleton();
 const auto now=musicTime(),elapsed=panelOpenRequest.elapsed(now);
 auto reject=[&](const char* reason,const char* message){
  logPanelFailure(actor.get(),pendingScreen,elapsed,reason);cancelPanelRequest();
  if(message)RE::SendHUDMessage::ShowHUDMessage(message);
 };
 if(visible||!campaign::interfaceSettings.gamesEnabled){cancelPanelRequest();return;}
 if(!player||!ui||!campaign::available()||ui->IsMenuOpen(RE::MainMenu::MENU_NAME)||ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)){
  cancelPanelRequest();return;
 }
 if(!actor||actor.get()==player||player->IsDead()||player->IsInCombat()){
  reject("player or NPC unavailable","Tessera is unavailable right now.");return;
 }
 if(actor->GetParentCell()!=player->GetParentCell()||actor->GetPosition().GetDistance(player->GetPosition())>=600.f){
  reject("NPC moved out of reach","They are too far away. Speak to them again.");return;
 }
 const auto actorBlock=campaign::actorCardBlock(actor.get());
 if(actorBlock!=campaign::ActorCardBlock::None&&actorBlock!=campaign::ActorCardBlock::Scene){
  const char* reason=actorBlock==campaign::ActorCardBlock::Combat?"NPC in combat":
   actorBlock==campaign::ActorCardBlock::Hostile?"NPC hostile":
   actorBlock==campaign::ActorCardBlock::Dead?"NPC dead":
   actorBlock==campaign::ActorCardBlock::Disabled?"NPC disabled":
   actorBlock==campaign::ActorCardBlock::ChildDialogue?"child dialogue/quest gate":"ineligible actor";
  reject(reason,"They are unavailable for Tessera right now.");return;
 }
 if(!api||!uiReady||!api->IsValid(view)){
  reject("native UI unavailable","Tessera's interface is unavailable.");return;
 }
 std::string reason;
 if(ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)){
  const auto manager=RE::MenuTopicManager::GetSingleton();
  if(manager&&manager->speaker&&manager->speaker!=actor->GetHandle()){
   reject("another conversation started",nullptr);return;
  }
  reason="dialogue closing";
 }else if(actorBlock==campaign::ActorCardBlock::Scene)reason="NPC scene";
 else if(player->GetCurrentScene())reason="player scene";
 else if(ui->GameIsPaused()||api->HasAnyActiveFocus())reason="another menu still owns focus";
 if(reason!=panelWaitReason){
  panelWaitReason=reason;
  log::debug("Panel request {} for {}: {}",pendingScreen,actor->GetName(),reason.empty()?"ready; waiting for menu state to settle":reason);
 }
 const auto step=panelOpenRequest.poll(token,now,!reason.empty());
 if(step==ttcg::PanelOpenRequest::Step::Expired){
  reject(reason.empty()?"menu transition timed out":reason.c_str(),
   reason=="NPC scene"||reason=="player scene"?"They are busy. Try speaking to them again in a moment.":"Tessera couldn't open. Please try again.");return;
 }
 if(step!=ttcg::PanelOpenRequest::Step::Open)return;
 const auto requested=pendingScreen;cancelPanelRequest();
 showPanel(actor.get(),requested);
 collectionIntegration::publish();
 if(!visible)logPanelFailure(actor.get(),requested,elapsed,"opening rejected; see in-game message");
 else log::debug("Panel request {} for {} opened after {} ms",requested,actor->GetName(),elapsed);
}
void requestPanel(RE::Actor* actor,const std::string& requested="lobby") {
 if(!actor||visible||!campaign::interfaceSettings.gamesEnabled)return;
 pending=actor->GetHandle();pendingScreen=requested;panelWaitReason.clear();
 const auto token=panelOpenRequest.begin(musicTime());
 log::info("Panel requested: {} for {} ({:08X})",requested,actor->GetName(),actor->GetFormID());
 finishPanelRequest(token);
 if(!panelOpenRequest.current(token))return;
 auto queued=std::make_shared<std::atomic_bool>(false);
 std::thread([token,queued](){
  while(panelOpenRequest.current(token)){
   std::this_thread::sleep_for(std::chrono::milliseconds(ttcg::PanelOpenRequest::retryMs));
   if(!panelOpenRequest.current(token))break;
   if(queued->exchange(true))continue;
   // Publish the collection once the menu opens.
   SKSE::GetTaskInterface()->AddTask([token,queued](){queued->store(false);finishPanelRequest(token);});
  }
 }).detach();
}
void queuePanelRequest(RE::Actor* actor,std::string requested) {
 if(!actor)return;
 const auto handle=actor->GetHandle();const auto generation=collectionIntegration::generation.load();
 collectionIntegration::queue([handle,generation,requested=std::move(requested)](){
  if(generation!=collectionIntegration::generation.load())return;
  if(auto actor=handle.get())requestPanel(actor.get(),requested);
 });
}
void requestChallenge(RE::Actor* actor) {
 campaign::registerNpcActor(actor);
 if(!campaign::interfaceSettings.gamesEnabled||!eligible(actor)||visible)return;
 campaign::syncProgression();
 if(!campaign::hasAlbum()){RE::SendHUDMessage::ShowHUDMessage("Buy a Tessera Album from a general-goods merchant.");return;}
 const auto i=campaign::profileIndex(actor);
 if(!(campaign::resumingWith(actor)||campaign::isChild(actor)||(i>=0?campaign::unlocked(i):actor&&campaign::development.allowAnyNPC))){RE::SendHUDMessage::ShowHUDMessage("They are unavailable for a game.");return;}
 if(i>=0&&ttcg::opponents[i].dynamic){
  const int ready=campaign::npcAvailability(actor);if(!ready){RE::SendHUDMessage::ShowHUDMessage("They are unavailable for a game.");return;}
  auto info=campaign::npcResponse(actor,ready);
  if(info){if(info->objConditions.IsTrue(actor,RE::PlayerCharacter::GetSingleton()))actor->SetDialogueWithPlayer(true,true,info);else RE::SendHUDMessage::ShowHUDMessage("They are unavailable for a game.");return;}
  if(ready==1){requestPanel(actor);}else RE::SendHUDMessage::ShowHUDMessage(ready==3?"They don't have enough cards for a game.":"They don't want to play right now.");return;
 }
 const bool accepted=campaign::resumingWith(actor)||i<0||(ttcg::opponentHasCards(campaign::saved,i)&&(campaign::development.unlockAllPlayers||ttcg::willingToPlay(campaign::saved,i,campaign::gameHour())));
 const bool alternate=i>=0&&ttcg::opponents[i].alternateBase&&campaign::persistentID(actor->GetActorBase())==ttcg::opponents[i].alternateBase;
 const auto id=i<0?ttcg::encounter_childAccept:alternate?(accepted?ttcg::encounter_alternateAccept:ttcg::encounter_alternateDecline):accepted?ttcg::encounterRecords[i].accept:ttcg::encounterRecords[i].decline;
 auto info=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESTopicInfo>(id,campaign::plugin);
 if(info)actor->SetDialogueWithPlayer(true,true,info);
}
void finishAlbumOpen() {
 if(!pendingAlbum)return;
 auto ui=RE::UI::GetSingleton();if(!ui)return;
 if(ui->IsMenuOpen(RE::BookMenu::MENU_NAME)||ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME))return;
 pendingAlbum=false;
 if(!ui->GameIsPaused()&&!ui->IsMenuOpen(RE::MainMenu::MENU_NAME))showPanel(nullptr,"album");
}
void albumPapyrus(RE::StaticFunctionTag*) {
 collectionIntegration::queue([]() {
   auto player=RE::PlayerCharacter::GetSingleton();auto ui=RE::UI::GetSingleton();
   if(visible||pendingAlbum||!campaign::available()||!player||player->IsDead()||player->IsInCombat()||!ui||!uiReady||!api||!api->IsValid(view))return;
   if(!ui->IsMenuOpen(RE::BookMenu::MENU_NAME)||RE::BookMenu::GetTargetForm()!=campaign::albumForm)return;
   pendingAlbum=true;
   auto queue=RE::UIMessageQueue::GetSingleton();if(!queue){pendingAlbum=false;return;}
   if(auto book=ui->GetMenu<RE::BookMenu>()) {
     auto& data=book->GetRuntimeData();
     if(book->uiMovie)book->uiMovie->SetVisible(false);
     if(data.book)data.book->SetVisible(false);
     if(data.bookModel)data.bookModel->SetAppCulled(true);
     data.closeMenu=true;
   }
   queue->AddMessage(RE::BookMenu::MENU_NAME,RE::UI_MESSAGE_TYPE::kForceHide,nullptr);
   if(ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME))queue->AddMessage(RE::InventoryMenu::MENU_NAME,RE::UI_MESSAGE_TYPE::kHide,nullptr);
 });
}
void recordResult(const ttcg::Result& result) {
 lastResult=result; settling=true; playedRuleSounds=0;
 ++revision;
 if(match.finished()&&campaign::saved.contract.held&&!campaign::saved.contract.capture(match)){log::error("Invalid match provenance");close();return;}
 if(match.finished()) { campaign::saved.dealReveals={}; const auto s=match.score();auto actor=opponent.get();campaign::recordGame(actor.get(),s[0]==s[1]?-1:s[0]>s[1]?0:1); }
 checkpointMatch();
 playSound("UIMenuFocus");
}
void runAI() {
 if(!visible||!active||thinking||settling||worldFocus.switching()||match.finished()||match.turn!=1||!checkLiveWorld()) return;
 thinking=true; publish();
 const auto snapshot=match; auto actor=opponent.get();const auto* profile=campaign::profile(actor.get());const int skill=campaign::development.opponentSkill>=0?campaign::development.opponentSkill:profile?profile->skill:campaign::isChild(actor.get())?ttcg::childProfile(campaign::persistentID(actor->GetActorBase())).skill:3; const auto token=epoch.load(); const auto round=session; const auto stateRevision=revision;
 std::thread([snapshot,token,round,stateRevision,skill]() {
   const bool diagnostics=spdlog::should_log(spdlog::level::debug);
   const auto started=diagnostics?musicTime():0;
   auto move=ttcg::opponentMove(snapshot,skill,ttcg::matchDecisionSeed(snapshot));
   if(diagnostics)log::debug("AI decision: session {}, revision {}, placed {}, skill {}, hand {}, square {}, elapsed {} ms",round,stateRevision,snapshot.placed,skill,move.hand,move.square,musicTime()-started);
   // Presentation has acknowledged the previous move before this search starts.
   std::this_thread::sleep_for(std::chrono::milliseconds(300));
   if(epoch.load()!=token) return;
   collectionIntegration::queue([move,token,round,stateRevision]() {
     if(epoch.load()!=token||!visible||session!=round||revision!=stateRevision||match.turn!=1) return;
     if(!checkLiveWorld())return;
     thinking=false;
     auto result=match.play(move);
     if(!result.legal) { log::error("AI returned an illegal move"); close(); return; }
     recordResult(result); publish();
   });
 }).detach();
}
#include "NativeTournamentCommands.h"
bool validCommandSize(std::string_view text){return text.size()<=(text.starts_with("cardback ")?2048u:128u);}
void command(const char* raw) {
 if(!raw||!visible) return;
 std::string text(raw); if(!validCommandSize(text)) return;
 std::istringstream input(text); std::string verb,extra; std::uint64_t s=0,r=0;
 if(!(input>>verb>>s>>r)||s!=session||r!=revision) return;
 if(worldFocus.switching())return;
 if(!checkLiveWorld())return;
 if(verb=="interrupted"){
  std::string action;if(active||screen!="album"||!(input>>action)||(input>>extra)||!campaign::savedMatch.present)return;
  if(action=="resume"){if(!resumeMatch()){
   if(campaign::savedMatch.present)campaign::notice=campaign::savedMatch.tournament?"Return to the tournament inn to resume.":"Return to "+campaign::savedMatchName()+" to resume.";
   ++revision;publish();
  }return;}
  if(action=="forfeit"&&!campaign::savedMatch.table.finished()){
   campaign::resolveSavedMatch(true);campaign::prepare(nullptr,true);++session;revision=0;publish();return;
  }
  return;
 }
 // Checkpoint a match when toggling games off.
 if(verb=="settings"){
  const auto position=input.tellg();std::string key;int value=-1;
  if(!(input>>key>>value)||(input>>extra)||value<0||!bindingCapture.empty())return;
  if(key=="games"){if(value<=1)setGamesEnabled(value!=0);return;}
  if(key=="residents"||key=="vanilla-voices"){
   if(active||thinking||settling)return;
   interfaceNotice=campaign::interfaceSettings.set(key,value,persistInterfaceSetting)?"":"Couldn't save this setting.";
   campaign::registerNearbyNpcs();campaign::syncProgression();publish();return;
  }
  input.clear();input.seekg(position);
 }
 if(!campaign::interfaceSettings.gamesEnabled&&(verb=="start"||verb=="play"||verb=="prepare"||verb=="rule-lesson"||verb=="shop"))return;
 if(!ttcg::tournamentRewardAllowsCommand(campaign::saved,verb)){publish();return;}
 if(screen=="shop"&&verb!="shop"&&verb!="close"&&verb!="binding"&&verb!="development"&&verb!="cardback"&&verb!="settings")return;
 if(verb=="collection-recovery"){
   std::string action,confirmation;
   if(!(input>>action>>confirmation)||(input>>extra)||confirmation!="confirm"||
      (action!="clear"&&action!="remove-missing"))return;
   if(!campaign::available()||active||thinking||settling||screen!="album"||!bindingCapture.empty())return;
   if(action=="clear"&&!campaign::development.enabled)return;
   const auto blocked=ttcg::collectionRecoveryBlocked(campaign::saved,campaign::savedMatch.present);
   if(!blocked.empty()){campaign::notice=blocked;publish();return;}
   if(action=="clear"){
     if(!campaign::development.clearCollection(campaign::saved,[](const char* key,int value){
       const std::string name(key);const std::wstring wide(name.begin(),name.end());
       return WritePrivateProfileStringW(L"Development",wide.c_str(),std::to_wstring(value).c_str(),settingsPath.c_str())!=0;
     })){campaign::notice="Couldn't save the collection settings. Your collection has not been cleared.";publish();return;}
     developmentNotice.clear();
   }else if(!ttcg::removeUnavailableCards(campaign::saved))return;
   campaign::prepare(opponent.get().get());campaign::syncProgression();
   campaign::notice=action=="clear"?"Collection cleared. Collect new cards from packs, shops and games.":"Unavailable cards removed from your collection and decks.";
   log::info("Collection recovery: {}",action);
   match=ttcg::Match(1,campaign::sessionRules);++revision;publish();return;
 }
 if(verb=="settings") {
   std::string key;int value=-1;
   if(active||thinking||settling||screen=="lesson"||!bindingCapture.empty()||
      !(input>>key>>value)||(input>>extra))return;
   const auto option=std::find_if(pauseOptions.begin(),pauseOptions.end(),[&](const auto& entry){return entry.command==key;});
   if(key=="rank-finish-reset"&&value==0){
     interfaceNotice=campaign::interfaceSettings.resetRankFinish(persistInterfaceSetting)?"":"Couldn't save every setting.";
     publish();return;
   }
   if(key=="match-appearance-reset"&&value==0){
     interfaceNotice=campaign::interfaceSettings.resetMatchAppearance(persistInterfaceSetting)?"":"Couldn't save every setting.";
     publish();return;
   }
   if(ttcg::InterfaceSettings::find(key)){
     interfaceNotice=campaign::interfaceSettings.set(key,value,persistInterfaceSetting)?"":"Couldn't save this setting.";
     publish();return;
   }
   if(option==pauseOptions.end()||(value!=0&&value!=1))return;
   if(!WritePrivateProfileStringW(L"Interface",option->iniKey,value?L"1":L"0",settingsPath.c_str())) {
     interfaceNotice="Couldn't save this setting.";publish();return;
   }
   worldPauseSettings.*option->member=value!=0;interfaceNotice.clear();publish();return;
 }
 if(verb=="npc-add"){
  if(active||thinking||settling||campaign::saved.contract.pending()||(input>>extra))return;
  auto actor=enrollmentTarget.get();const bool added=actor&&eligible(actor.get())&&campaign::canEnrollNpc(actor.get())&&campaign::registerNpcActor(actor.get(),true);
  interfaceNotice=added?std::string(actor->GetName())+" now plays Tessera.":campaign::npcInspector(actor.get());
  campaign::syncProgression();publish();return;
 }
 if(verb=="shop"){
  if(screen!="shop"||active||thinking||settling)return;
  auto actor=opponent.get();if(!eligible(actor.get())||!campaign::shopAvailable(actor.get())){close();return;}
  if(campaign::shopCommand(actor.get(),input)){++revision;publish();}return;
 }
 if(verb=="tournament"){tournamentCommand(input);return;}
 if(verb=="cardback"){
   if(active||thinking||settling||screen=="lesson"||!bindingCapture.empty())return;
   if(!ttcg::cardBackCommand(input,cardBackNotice,[](std::string_view id){
     const auto value=ttcg::cardBackSetting(id);const std::wstring wide(value.begin(),value.end());
     return WritePrivateProfileStringW(L"Appearance",L"CardBack",wide.c_str(),settingsPath.c_str())!=0;
   }))return;
   publish();return;
 }
 if(verb=="development") {
   if(!campaign::development.enabled||!campaign::available()||active||thinking||settling||screen=="lesson"||
      !bindingCapture.empty()||campaign::savedMatch.present||campaign::saved.contract.pending()||campaign::saved.goldCredit)return;
   std::string action,key;int value=0;if(!(input>>action))return;
   bool resetTerms=false;
   if(action=="set"){
     if(!(input>>key>>value)||(input>>extra))return;
     auto proposed=campaign::development;if(!proposed.set(key,value))return;
     if(!campaign::development.setPersisted(key,value,[](std::string_view name,int number){
       const std::wstring wide(name.begin(),name.end());
       return WritePrivateProfileStringW(L"Development",wide.c_str(),std::to_wstring(number).c_str(),settingsPath.c_str())!=0;
     })){developmentNotice="Couldn't save this setting.";publish();return;}
     resetTerms=key=="Rules"||key=="TradeRule"||key=="Wager";developmentNotice.clear();
     log::info("Development setting: {} = {}",key,value);
   }else if(action=="cards"||action=="foils"){
     if(input>>extra)return;
     const auto added=campaign::development.addMissingCards(campaign::saved,action=="foils");if(added<0)return;
     developmentNotice=added?std::format("Added {} {} to your album.",added,action=="foils"?"foils":"cards"):
       action=="foils"?"Your album already has every foil.":"Your album already has every card.";
     campaign::ensureAlbum();log::info("Development: {}",developmentNotice);
   }else return;
   campaign::prepare(screen=="shop"?nullptr:opponent.get().get(),resetTerms);campaign::syncProgression();
   match=ttcg::Match(1,campaign::sessionRules);++revision;publish();return;
 }
 if(verb=="binding") {
   if(active||settling||thinking)return;
   std::string action,field;unsigned value=0;
   if(!(input>>action))return;
   if(action=="cancel") {if(input>>extra)return;bindingCapture.clear();bindingNotice.clear();publish();return;}
   if(!(input>>field))return;
   const auto setting=std::find_if(hotkeys.fields.begin(),hotkeys.fields.end(),[&](const auto& entry){return entry.name==field;});
   if(setting==hotkeys.fields.end())return;
   if(action=="begin") {if(input>>extra)return;bindingCapture=field;bindingNotice.clear();publish();return;}
   if(action=="set") {if(!(input>>value)||bindingCapture!=field)return;}
   else if(action=="unbind")value=0;
   else if(action=="reset")value=setting->defaultKey;
   else return;
   if(input>>extra)return;
   if(!ttcg::validHotkey(value)){bindingNotice="Choose a different key.";publish();return;}
   if(const auto conflict=hotkeys.conflict(field,value);!conflict.empty()){
     bindingNotice="Already used for "+std::string(conflict)+".";publish();return;
   }
   if(!WritePrivateProfileStringW(L"Input",setting->ini,std::to_wstring(value).c_str(),settingsPath.c_str())){bindingNotice="Couldn't save the key.";publish();return;}
   hotkeys.*setting->member=value;bindingCapture.clear();bindingNotice.clear();publish();return;
 }
 if(verb=="close") {
   if(input>>extra) return;
   if((active&&campaign::saved.contract.pending())||(campaign::tournamentID&&active&&!match.finished())) { publish(); return; }
   close(); return;
 }
 if(verb=="rule-lesson"){
   if((input>>extra)||active||thinking||settling||screen!="lobby"||!campaign::development.ruleLesson)return;
   constexpr unsigned types[]={0,ttcg::ThreeOpen,ttcg::Legion,ttcg::Decimation,ttcg::Affinity};
   ttcg::abandonRuleCulture(campaign::saved,campaign::gameHour());campaign::sessionRules=campaign::terms(opponent.get().get()).rules;lesson=ttcg::Lesson::workshop(types[campaign::development.ruleLesson]);playedRuleSounds=0;screen="lesson";++session;revision=0;publish();return;
 }
 if(verb=="lesson") {
   std::string action;if(!(input>>action)||active||settling||screen!="lesson")return;
   auto actor=opponent.get();if(!eligible(actor.get()))return;
   if(action=="place") {
     int h=-1,square=-1;if(!(input>>h>>square)||(input>>extra)||lesson.match.turn!=0||!lesson.advance(h,square))return;

     ++revision;publish();return;
   }
   if(input>>extra)return;
   if(action=="next") {if(lesson.match.turn!=1||!lesson.advance())return;++revision;publish();return;}
   if(action=="replay") {lesson=lesson.riftTour?ttcg::Lesson::riftLesson():lesson.affinityTour?ttcg::Lesson::affinityLesson():lesson.workshopRule?ttcg::Lesson::workshop(lesson.workshopRule):ttcg::Lesson(lesson.sameLesson,lesson.plusLesson);playedRuleSounds=0;++session;revision=0;publish();return;}
   if(action=="play") {
     if((lesson.sameLesson||lesson.workshopRule)&&!campaign::canChallenge(actor.get())){close();return;}
     if(!campaign::prepare(actor.get()))return;
     screen="lobby";challengeContext=true;match=ttcg::Match(1,campaign::sessionRules);
     ++session;revision=0;updateMusic();publish();return;
   }
   return;
 }
 if(verb=="culture") {
   std::string action;unsigned value=0;
   if(!(input>>action>>value)||(input>>extra)||active||thinking||settling||screen!="lobby")return;
   auto actor=opponent.get();const auto i=campaign::profileIndex(actor.get());
   if(!eligible(actor.get())||!campaign::canChallenge(actor.get())||!campaign::cultureEnabled(actor.get()))return;
   bool changed=false;
   if(action=="trial"&&value<=1)changed=ttcg::answerSpread(campaign::saved,i,campaign::gameHour(),value!=0);
   else if(action=="abolish")changed=ttcg::challengeAbolition(campaign::saved,i,campaign::gameHour(),value);
   if(!changed){++revision;publish();return;}
   const auto& attempt=campaign::saved.culture.pending;
   campaign::sessionRules=attempt.kind?attempt.rules:campaign::terms(actor.get()).rules;
   if(!campaign::prepare(actor.get())){++revision;publish();return;}
   match=ttcg::Match(1,campaign::sessionRules);++session;revision=0;publish();return;
 }
 if(verb=="terms") {
   std::string action;unsigned value=0;if(!(input>>action>>value)||(input>>extra)||active||settling||screen!="lobby")return;
   auto actor=opponent.get();if(!eligible(actor.get())||!campaign::canChallenge(actor.get()))return;
   if(action=="rule"&&campaign::isChild(actor.get())&&campaign::ruleRequest&&!campaign::requestAnswered&&(value==0||value==campaign::ruleRequest)){campaign::sessionRules=campaign::terms(actor.get()).rules|value;campaign::requestAnswered=true;}
   else return;
   if(!campaign::prepare(actor.get())){++revision;publish();return;}
   match=ttcg::Match(1,campaign::sessionRules);++session;revision=0;publish();return;
 }
 if(verb=="browse") {
   std::string target;
   if(!(input>>target)||(input>>extra)||active||campaign::saved.contract.pending()||(target!="album"&&target!="lobby")) return;
   auto actor=opponent.get();
   if(target!="album"&&!campaign::tournamentID&&!eligible(actor.get())) return;
   if(!campaign::prepare(actor.get())) { publish(); return; }
   if(target=="lobby"&&!campaign::canChallenge(actor.get())) return;
   match=ttcg::Match(1,campaign::sessionRules);
   screen=target;if(target=="lobby")challengeContext=true; ++session; revision=0; updateMusic(); publish(); return;
 }
 if(verb=="open-pack") {
   if((input>>extra)||active||campaign::savedMatch.present||screen!="album") return;
   const auto seed=static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
   campaign::notice=campaign::openPack(seed)?"":"Pack unavailable.";
   ++revision; publish(); return;
 }
 if(verb=="forfeit") {
   if((input>>extra)||!active||match.finished())return;
   campaign::savedMatch={};releaseOpponent();
   if(campaign::tournamentID){ttcg::withdrawTournament(campaign::saved,campaign::tournamentID);tournamentBoard();return;}
   if(!campaign::saved.contract.held){close();return;}
   auto& contract=campaign::saved.contract;
   auto actor=opponent.get();campaign::recordGame(actor.get(),1,false);
   contract.forfeit();campaign::finish(1);
   if(!contract.pending()) { RE::SendHUDMessage::ShowHUDMessage(campaign::receipt.c_str()); close(); } else publish();
   return;
 }
 if(verb=="claim"||verb=="claim-many") {
   unsigned selection=0;auto& contract=campaign::saved.contract;
   if(!(input>>selection)||(input>>extra)||settling||!active||!match.finished()||!contract.choicePending())return;
   if(verb=="claim"){if(contract.rule()!=ttcg::TradeOne||selection>4)return;selection=1u<<selection;}
   if((selection&~31u)||std::popcount(selection)!=static_cast<int>(contract.required(0)))return;
   campaign::finish(0,selection);publish();return;
 }
 if(verb=="payout") {
   auto& contract=campaign::saved.contract;
   if((input>>extra)||settling||!active||!match.finished()||!contract.paying) return;
   campaign::recover(); publish(); return;
 }
 if(verb=="prepare") {
   if(input>>extra)return;
   log::info("Rematch requested: session {}, revision {}, settling {}, thinking {}, pending {}",session,revision,settling,thinking,campaign::saved.contract.pending());
   if(settling||thinking||(active&&!match.finished())||campaign::saved.contract.pending()){
    campaign::notice="The previous game is still finishing.";publish();return;
   }
   auto actor=opponent.get(); if(!eligible(actor.get())) { close(); return; }
   releaseOpponent();campaign::savedMatch={};
   if(!campaign::prepare(actor.get(),true)) { log::warn("Rematch preparation failed: {}",campaign::notice);publish(); return; }
   ++epoch; ++session; revision=0; active=false; lastResult={}; match=ttcg::Match(1,campaign::sessionRules);
   log::info("Rematch prepared: session {}, opponent {}",session,opponentName);
   updateMusic(); publish(); return;
 }
 if(verb=="deck-manage") {
   if(active||settling||thinking||!campaign::available()||campaign::savedMatch.present||campaign::saved.contract.pending()||campaign::saved.goldCredit||(screen!="album"&&screen!="lobby"))return;
   std::string action,name;unsigned id=0;if(!(input>>action>>id))return;
   if(action=="new"||action=="copy"||action=="rename")std::getline(input>>std::ws,name);
   else if(input>>extra)return;
   if(ttcg::manageDeck(campaign::saved,action,id,name)){++session;revision=0;publish();}return;
 }
 if(verb=="deck") {
   if(active||settling||campaign::savedMatch.present||campaign::saved.contract.pending()||campaign::saved.goldCredit||(screen!="album"&&screen!="lobby")) return;
   ttcg::Hand deck{};
   for(auto& id:deck) { int index=-1; if(!(input>>index)||index< -1||index>=static_cast<int>(ttcg::cards.size()*2)) return; id=index<0?0:ttcg::displayCardID(index); }
   if(input>>extra) return;
   auto stock=campaign::inventory(RE::PlayerCharacter::GetSingleton());
   if(ttcg::validDeck(deck,stock)) { campaign::saved.deck=deck; ttcg::saveActiveDeck(campaign::saved);publish(); }
   return;
 }
 if(verb=="opening") {
   if((input>>extra)||!active||!settling||match.placed!=0||playedOpeningSound) return;
   playedOpeningSound=true;
   const RE::FormID localID=match.turn==0?0x86A:0x869;
   auto descriptor=RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSSoundDescriptorForm>(localID,"Tessera TCG.esp");
   auto audio=RE::BSAudioManager::GetSingleton();
   const bool started=descriptor&&descriptor->soundDescriptor&&audio&&audio->Play(descriptor);
   if(started)log::debug("Opening sound {:06X}: playback submitted",localID);
   else log::warn("Opening sound {:06X}: playback failed",localID);
   return;
 }
 if(verb=="result") {
   if((input>>extra)||!active||!settling||!match.finished()||playedResultSound) return;
   playedResultSound=true;
   const auto score=match.score();
   const RE::FormID localID=score[0]==score[1]?0x815:score[0]>score[1]?0x817:0x816;
   auto descriptor=RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSSoundDescriptorForm>(localID,"Tessera TCG.esp");
   auto audio=RE::BSAudioManager::GetSingleton();
   const bool started=descriptor&&descriptor->soundDescriptor&&audio&&audio->Play(descriptor);
   if(started)log::debug("Result sound {:06X}: playback submitted",localID);
   else log::warn("Result sound {:06X}: playback failed",localID);
   return;
 }
 if(verb=="rule") {
   std::string rule; if(!(input>>rule)||(input>>extra)||(!settling&&screen!="lesson")) return;
   const unsigned bit=rule=="same"||rule=="samewall"?1u:rule=="plus"?2u:rule=="combo"?8u:rule=="legion"?ttcg::Legion:rule=="decimation"?ttcg::Decimation:0u;
   if(!bit||(playedRuleSounds&bit)) return;
   const bool triggered=std::any_of((screen=="lesson"?lesson.last:lastResult).stages.begin(),(screen=="lesson"?lesson.last:lastResult).stages.end(),[bit](auto& stage){ return (stage.rule&bit)!=0; });
   if(!triggered) return;
   playedRuleSounds|=bit;
   playRuleSound(bit);
   return;
 }
 if(verb=="music") {
   int enabled=-1; if(!(input>>enabled)||(input>>extra)||(enabled!=0&&enabled!=1)) return;
   musicEnabled=enabled!=0;
   if(musicEnabled&&!musicPlaying) scanMusicFolder();
   if(!WritePrivateProfileStringW(L"Audio",L"Enabled",musicEnabled?L"1":L"0",settingsPath.c_str())) log::warn("Could not save music preference");
   updateMusic(); publish(); return;
 }
 if(verb=="settled") {
   if((input>>extra)||!settling) return;
   log::debug("Presentation acknowledged: session {}, revision {}, placed {}, turn {}",session,revision,match.placed,match.turn);
   settling=false;
   if(match.redeal()){
     ++revision;lastResult={};settling=true;playedOpeningSound=false;playedRuleSounds=0;
     checkpointMatch();publish();return;
   }
   auto& contract=campaign::saved.contract;
   if(match.finished()&&contract.held&&!contract.choicePending()) {
     const int winner=contract.outcome;
     campaign::finish(winner);
   }
   publish(); runAI(); return;
 }
 if(verb=="start") {
   unsigned rules=0; int tradeRule=-1,wager=-1;
   if(!(input>>rules>>tradeRule>>wager)||!ttcg::validRules(rules)||(tradeRule<0||tradeRule>4)||(wager<0||wager>ttcg::maximumWager||wager%5)||thinking||settling||active||screen!="lobby"||campaign::savedMatch.present||campaign::saved.contract.pending()||campaign::saved.goldCredit) return;
   ttcg::Hand deck{};
   for(auto& id:deck) { int index=-1; if(!(input>>index)||index<0||index>=static_cast<int>(ttcg::cards.size()*2)) return; id=ttcg::displayCardID(index); }
   if(input>>extra) return;
   auto actor=opponent.get(); if(!campaign::tournamentID&&!eligible(actor.get())) { close(); return; }
   if(!campaign::canChallenge(actor.get())||(!campaign::tournamentID&&!campaign::canStake&&!campaign::practice))return;
   const int expected=!campaign::practice&&campaign::canStake?campaign::terms(actor.get()).trade:0;
   const int fixed=campaign::fixedWager();
   if((campaign::cultureEnabled(actor.get())&&ttcg::spreadOffer(campaign::saved,campaign::profileIndex(actor.get()),campaign::gameHour()))||rules!=campaign::sessionRules||tradeRule!=expected||wager>campaign::maxWager(actor.get())||(fixed>=0&&wager!=fixed)||(campaign::ruleRequest&&!campaign::requestAnswered))return;
   auto stock=campaign::inventory(RE::PlayerCharacter::GetSingleton());
   if(ttcg::playableCount(stock)<5) { campaign::notice="You need five cards."; publish(); return; }
   if(!ttcg::validHand(deck,stock)) { campaign::notice="Choose five cards you own."; publish(); return; }
   if(campaign::saved.culture.pending.kind&&(!campaign::cultureEnabled(actor.get())||!ttcg::canStartCultureMatch(campaign::saved,campaign::profileIndex(actor.get()),rules)))return;
   if(campaign::tournamentID){
     const auto* e=ttcg::tournament(campaign::saved,campaign::tournamentID);
     if(!e||campaign::tournamentVenue()!=e->hold||!ttcg::startTournamentRound(campaign::saved,campaign::tournamentID,campaign::gameHour())){campaign::notice="This tournament round is unavailable.";++revision;publish();return;}
   }
   campaign::Bank bank(campaign::persistentID(actor.get()));
   if(tradeRule||wager) {
     if(!campaign::canStake||!ttcg::validHand(campaign::rivalDeck,campaign::inventory(actor.get()))) { campaign::notice="They don't have enough cards for a game."; publish(); return; }
     if(!campaign::saved.contract.reserve(bank,{deck,campaign::rivalDeck},static_cast<unsigned>(tradeRule),wager,campaign::persistentID(actor.get()))) {
       log::warn("Match stakes unavailable: {} ({:08X}), actor resolved {}, ledger {:08X}, trade {}, wager {}, gold {}/{}, cards valid {}/{}, transfer pending {}",
         actor->GetName(),actor->GetFormID(),bank.actors[1]!=nullptr,bank.rival,tradeRule,wager,bank.count(0,0),bank.count(1,0),
         ttcg::validHand(deck,campaign::inventory(RE::PlayerCharacter::GetSingleton())),ttcg::validHand(campaign::rivalDeck,campaign::inventory(actor.get())),campaign::saved.contract.pending());
       campaign::notice="Cards or gold unavailable."; publish(); return;
     }
   } else campaign::saved.contract={};
   campaign::cultureGame=campaign::cultureEnabled(actor.get())&&(tradeRule||wager);
   if(campaign::cultureGame)ttcg::startCultureMatch(campaign::saved,campaign::profileIndex(actor.get()),rules);
   campaign::competitiveMatch=!campaign::practice;
   campaign::saved.deck=deck;ttcg::saveActiveDeck(campaign::saved); campaign::notice.clear(); campaign::receipt.clear();
   ++epoch; ++session; revision=0;
   const auto* event=ttcg::tournament(campaign::saved,campaign::tournamentID);
   const auto seed=event?ttcg::tournamentHash(event->seed^event->matchNode^event->replays^((event->wins[event->matchNode-8][0]+event->wins[event->matchNode-8][1])*2654435761u)):static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
   match=ttcg::Match(ttcg::indices(deck),ttcg::indices(campaign::rivalDeck),seed,rules); active=true;
   for(int p=0;p<2;++p)for(int h=0;h<5;++h)match.foilHands[p][h]=ttcg::isFoil(p==0?deck[h]:campaign::rivalDeck[h]);
   ttcg::retainDealVisibility(campaign::saved,match);
   lastResult={}; settling=true; playedResultSound=false; playedOpeningSound=false;
   checkpointMatch();
   log::info("Match {} seed {} rules {} trade {} wager {}",session,seed,rules,tradeRule,wager);
   updateMusic(); publish(); runAI(); return;
 }

 if(verb=="play") {
   int hand=-1,square=-1,group=-1;
   if(!(input>>hand>>square)||!active||thinking||settling||match.turn!=0) return;
   input>>std::ws;if(input.peek()!=std::char_traits<char>::eof()){if(!(input>>group)||(input>>extra))return;}
   auto result=match.play({hand,square,group}); if(!result.legal) return;
   recordResult(result); publish();
 }
}
int npcAvailabilityPapyrus(RE::StaticFunctionTag*,RE::Actor* actor){
 if(!actor)return -1;std::scoped_lock lock(campaign::npcCacheMutex);const auto it=campaign::npcAvailabilityCache.find(actor->GetFormID());return it==campaign::npcAvailabilityCache.end()?-1:it->second;
}
bool npcRegisteredPapyrus(RE::StaticFunctionTag* tag,RE::Actor* actor){return npcAvailabilityPapyrus(tag,actor)>=0;}
bool npcGrantedPapyrus(RE::StaticFunctionTag*,RE::Actor* actor,RE::BSFixedString grant){
 if(!actor||grant.empty())return false;std::scoped_lock lock(campaign::npcCacheMutex);const auto it=campaign::npcIdentityCache.find(actor->GetFormID());
 return it!=campaign::npcIdentityCache.end()&&campaign::npcGrantedCache.contains("script:"+std::string(grant.c_str())+":"+it->second);
}
void npcAddPapyrus(RE::StaticFunctionTag*,RE::Actor* actor){if(!actor)return;const auto handle=actor->GetHandle();collectionIntegration::queue([handle](){auto actor=handle.get();if(actor){const bool added=campaign::registerNpcActor(actor.get(),true);campaign::syncProgression();log::info("NPC enrollment {}: {}",actor->GetName(),added);}});}
void npcRefreshPapyrus(RE::StaticFunctionTag*,RE::Actor* actor){if(!actor)return;const auto handle=actor->GetHandle();collectionIntegration::queue([handle](){auto actor=handle.get();if(actor){campaign::registerNpcActor(actor.get());campaign::syncProgression();}});}
void npcEnabledPapyrus(RE::StaticFunctionTag*,RE::Actor* actor,bool enabled){if(!actor)return;const auto handle=actor->GetHandle();collectionIntegration::queue([handle,enabled](){auto actor=handle.get();if(!actor)return;campaign::registerNpcActor(actor.get());const auto base=campaign::persistentID(actor->GetActorBase());if(auto it=campaign::saved.residents.profiles.find(base);it!=campaign::saved.residents.profiles.end())it->second.enabled=enabled;campaign::syncProgression();});}
void npcGivePapyrus(RE::StaticFunctionTag*,RE::Actor* actor,RE::BSFixedString card,RE::BSFixedString grant,int quantity,bool foil){
 if(!actor||card.empty()||grant.empty())return;const auto handle=actor->GetHandle();const std::string key=card.c_str(),token=grant.c_str();
 collectionIntegration::queue([handle,key,token,quantity,foil](){auto actor=handle.get();if(!actor||!ttcg::validExpansionKey(key)||!ttcg::validExpansionKey(token)||quantity<1||quantity>99||(campaign::profileIndex(actor.get())<0&&!campaign::registerNpcActor(actor.get(),true))){log::warn("NPC card grant rejected: {}",token);return;}
  campaign::seedOpponent(actor.get());const auto base=campaign::persistentID(actor->GetActorBase());
  const bool given=ttcg::grantNpcCards(campaign::saved,campaign::ledgerKey(actor.get()),"script:"+token+":"+ttcg::worldKey(base),{{key,unsigned(quantity),foil}});
  campaign::syncProgression();log::info("NPC card grant {}: {}",token,given?"delivered":"already delivered or unavailable");
 });
}
void beginPapyrus(RE::StaticFunctionTag*,RE::Actor* actor) {
 queuePanelRequest(actor,"lobby");
}
void receiveCommand(const char* raw) {
 if(!raw) return;
 std::string text(raw);
 if(!validCommandSize(text)) return;
 // Native render callbacks run on the render thread. Game state and audio belong
 // to the game task queue; validate session/revision when the task executes.
 collectionIntegration::queue([text=std::move(text)]() { command(text.c_str()); });
}
RE::TESObjectBOOK* nextTournamentInvitationPapyrus(RE::StaticFunctionTag*){
 const auto request=campaign::tournamentLetterReady.exchange(0);if(!request)return nullptr;
 campaign::tournamentLetterSentAt=campaign::tournamentCourierClock();campaign::tournamentLetterInFlight=request;
 log::info("Tournament {} invitation handed to courier script",unsigned(request>>32));
 return RE::TESForm::LookupByID<RE::TESObjectBOOK>(unsigned(request));
}
void tournamentInvitationPapyrus(RE::StaticFunctionTag*,RE::TESObjectBOOK* letter,bool delivered){
 if(!letter)return;const auto form=letter->GetFormID();const auto request=campaign::tournamentLetterInFlight.load();
 collectionIntegration::queue([form,delivered,request](){
  if(!delivered&&request&&unsigned(request)==form)campaign::acknowledgeTournamentCourier(request);
  campaign::updateTournaments();
 });
}
void tournamentEntryPapyrus(RE::StaticFunctionTag*,RE::Actor* actor){queuePanelRequest(actor,"registration");}
void tournamentPapyrus(RE::StaticFunctionTag*,RE::Actor* actor){
 queuePanelRequest(actor,"tournaments");
}
void shopPapyrus(RE::StaticFunctionTag*,RE::Actor* actor){
 queuePanelRequest(actor,"shop");
}
bool expansionLoadedPapyrus(RE::StaticFunctionTag*,RE::BSFixedString key){return ttcg::expansionCardID(key.c_str())!=0;}
void expansionGivePapyrus(RE::StaticFunctionTag*,RE::BSFixedString key,std::int32_t quantity,bool foil){
 const std::string identity(key.c_str());
 collectionIntegration::queue([identity,quantity,foil](){
  const bool granted=campaign::available()&&RE::PlayerCharacter::GetSingleton()&&ttcg::grantExpansionCard(campaign::saved,identity,quantity,foil);
  if(granted){log::info("Expansion reward: {} x{}, foil {}",identity,quantity,foil);++revision;publish();}
  else log::warn("Expansion reward rejected: {} x{}, foil {} (unavailable card, duplicate unique, or invalid quantity/state)",identity,quantity,foil);
 });
}
void buyAlbumPapyrus(RE::StaticFunctionTag*,RE::Actor* actor) {
 if(!actor)return;
 const auto handle=actor->GetHandle();
 collectionIntegration::queue([handle](){
   auto merchant=handle.get();if(!merchant)return;
   const bool bought=campaign::buyAlbum(merchant.get());
   const char* message=bought?"Your Tessera Album includes eight starter cards.":
     campaign::hasAlbum()?"You already own a Tessera Album.":
     campaign::physicalCount(RE::PlayerCharacter::GetSingleton(),campaign::goldForm)<ttcg::albumPrice?"You need 100 gold to buy a Tessera Album.":
     "The album purchase could not be completed.";
   RE::SendHUDMessage::ShowHUDMessage(message);
   if(spdlog::should_log(spdlog::level::debug))log::debug("Album purchase callback: {} ({:08X}), eligible {}, success {}",merchant->GetName(),merchant->GetFormID(),vendors::albumEligible(merchant.get()),bought);
 });
}
void logAlbumDialogue() {
 // These engine queries exist only for diagnosis, not dialogue availability.
 if(!spdlog::should_log(spdlog::level::debug))return;
 auto manager=RE::MenuTopicManager::GetSingleton();
 auto ref=manager?manager->speaker.get():RE::NiPointer<RE::TESObjectREFR>{};
 auto actor=ref?ref->As<RE::Actor>():nullptr;if(!actor)return;
 auto data=RE::TESDataHandler::GetSingleton();
 auto info=data->LookupForm<RE::TESTopicInfo>(0x832,campaign::plugin);
 auto quest=data->LookupForm<RE::TESQuest>(0x800,campaign::plugin);
 auto player=RE::PlayerCharacter::GetSingleton();
 log::debug("Album dialogue: {} ({:08X}), eligible {}, acquired {}, global {}, quest running {}, INFO conditions {}, gold {}",
   actor->GetName(),actor->GetFormID(),vendors::albumEligible(actor),campaign::hasAlbum(),
   campaign::progression?campaign::progression->value:-1,quest&&quest->IsRunning(),
   info&&info->objConditions.IsTrue(actor,player),campaign::physicalCount(player,campaign::goldForm));
}
void interruptWorld() {
 const auto token=unpausedWorldEpoch.load();if(!token)return;
 collectionIntegration::queue([token]() {
   if(unpausedWorldEpoch.load()!=token||epoch.load()+1!=token||!visible||!worldFocus.unpaused())return;
   log::info("Closing unpaused Tessera for a world interruption");interruptPanel();
 });
}
bool watchedActor(RE::TESObjectREFR* actor){return actor&&(actor->GetFormID()==0x14||actor->GetFormID()==watchedOpponent.load());}
// Merge requests while a roster refresh is queued or running. SKSE can drain
// its task queue from different engine threads; claiming a refresh is atomic.
std::atomic_bool npcRefreshQueued{false};
void queueNpcRefresh(){
 if(npcRefreshQueued.exchange(true))return;
 collectionIntegration::queue([](){
  if(campaign::available()){
   static std::atomic_uint profileCount{0};
   const bool profile=spdlog::should_log(spdlog::level::debug)&&profileCount.fetch_add(1,std::memory_order_relaxed)%12==0;
   ttcg::NpcSyncTiming syncTiming;
   const auto started=std::chrono::steady_clock::now();
   campaign::syncProgression(profile?&syncTiming:nullptr);
   const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
   if(elapsed>=16.0)log::warn("Slow NPC load refresh: {:.1f} ms ({} opponents)",elapsed,ttcg::opponents.size());
   if(profile)logNpcSyncTiming(syncTiming,"object load");
  }
  npcRefreshQueued=false;
 });
}
class Events final: public RE::BSTEventSink<RE::InputEvent*>, public RE::BSTEventSink<RE::MenuOpenCloseEvent>, public RE::BSTEventSink<RE::TESContainerChangedEvent>, public RE::BSTEventSink<RE::TESCombatEvent>, public RE::BSTEventSink<RE::TESHitEvent>, public RE::BSTEventSink<RE::TESDeathEvent>, public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
public:
 RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event,RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override {
  if(event&&event->loaded){const auto id=event->formID;const auto generation=campaign::npcFactionTracking.generation();collectionIntegration::queue([id,generation](){
   if(!campaign::available()||!campaign::npcFactionTracking.current(generation))return;
   auto actor=RE::TESForm::LookupByID<RE::Actor>(id);
   if(actor){
    campaign::registerNpcActor(actor);
    const bool tagged=campaign::observeNpcFactions(actor,generation);
    if(campaign::profileIndex(actor)>=0||tagged)queueNpcRefresh();
   }
  });}return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* event,RE::BSTEventSource<RE::TESCombatEvent>*) override {
   if(event&&event->newState!=RE::ACTOR_COMBAT_STATE::kNone&&
      (watchedActor(event->actor.get())||watchedActor(event->targetActor.get())))interruptWorld();
   return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event,RE::BSTEventSource<RE::TESHitEvent>*) override {
   if(event&&watchedActor(event->target.get()))interruptWorld();
   return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* event,RE::BSTEventSource<RE::TESDeathEvent>*) override {
   if(event&&watchedActor(event->actorDying.get()))interruptWorld();
   if(event&&event->actorDying){const auto id=campaign::persistentID(event->actorDying.get());
    collectionIntegration::queue([id](){if(campaign::savedMatch.present&&campaign::savedMatch.opponent==id&&!active)campaign::recoverSavedMatch();});
   }
   return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
   // Inventory stacks need not have an ObjectReference. The engine event gives
   // us the base form directly, including letters moved by RemoveAllItems.
   if(event&&event->itemCount>0&&
      ((event->newContainer==0x14&&(event->oldContainer==0x39FB9||event->oldContainer==0x39FB7))||event->newContainer==0x39FB9||event->newContainer==0x39FB7)){
     const auto base=event->baseObj;
     // A newly delivered copy confirms this invitation request.
     const auto request=!event->oldContainer&&event->newContainer!=0x14?campaign::tournamentLetterInFlight.load():0;
     collectionIntegration::queue([base,request](){
       if(!campaign::available())return;
       const auto letter=campaign::tournamentLetterID(RE::TESForm::LookupByID<RE::TESObjectBOOK>(base));if(!letter)return;
       // Confirm the matching request when its letter arrives.
       if(request&&unsigned(request)==base)campaign::acknowledgeTournamentCourier(request);
       campaign::updateTournaments();
     });
   }
   if(event&&campaign::albumForm&&event->baseObj==campaign::albumForm->GetFormID()&&event->newContainer==0x14&&event->itemCount>0){
     collectionIntegration::queue([](){
       if(campaign::claimAlbum())RE::SendHUDMessage::ShowHUDMessage("Your Tessera Album includes eight starter cards.");
     });
   }
   return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
   if(!events) return RE::BSEventNotifyControl::kContinue;
   const auto modifiers=heldHotkeyModifiers();
   for(auto event=*events;event;event=event->next) {
     auto button=event->AsButtonEvent(); if(!button||!button->IsDown()) continue;
     const auto code=button->GetIDCode();
     if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&ttcg::hotkeyMatches(collectionKey,code,modifiers)&&!visible) {
       collectionIntegration::queue([]() {
         if(visible) { if(api) api->Escape(view); return; }
         auto ui=RE::UI::GetSingleton(); if(!ui||ui->GameIsPaused()||ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) return;
         if(campaign::interfaceSettings.gamesEnabled&&!campaign::hasAlbum()){RE::SendHUDMessage::ShowHUDMessage("Buy a Tessera Album from a general-goods merchant.");return;}
         showPanel(nullptr,"album");
       });
     }
     if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&campaign::interfaceSettings.gamesEnabled&&ttcg::hotkeyMatches(challengeKey,code,modifiers)&&!visible) {
       collectionIntegration::queue([]() {
         if(visible) { if(api) api->Escape(view); return; }
         auto ui=RE::UI::GetSingleton(); if(!ui||ui->GameIsPaused()||ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) return;
         if(!campaign::interfaceSettings.gamesEnabled)return;
         auto pick=RE::CrosshairPickData::GetSingleton();
         auto ref=pick?pick->GetActiveTarget().get():RE::NiPointer<RE::TESObjectREFR>{};
         auto actor=ref?ref->As<RE::Actor>():nullptr;
         if(!eligible(actor)) { RE::SendHUDMessage::ShowHUDMessage("Face a nearby NPC to challenge them."); return; }
         requestChallenge(actor);
       });
     }
     if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&ttcg::hotkeyMatches(hotkeys.toggleGames,code,modifiers)&&!visible){
       collectionIntegration::queue([](){
         const auto ui=RE::UI::GetSingleton();
         if(visible||!ui||ui->GameIsPaused()||ui->IsMenuOpen(RE::MainMenu::MENU_NAME))return;
         setGamesEnabled(!campaign::interfaceSettings.gamesEnabled);
       });
     }

   }
   return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
   if(!event) return RE::BSEventNotifyControl::kContinue;
   if(event->opening&&event->menuName==RE::DialogueMenu::MENU_NAME){
     interruptWorld();
     collectionIntegration::queue([](){campaign::registerNearbyNpcs();campaign::syncProgression();campaign::claimAlbum();campaign::updateTournaments();logAlbumDialogue();});
   }
   if(event->opening&&event->menuName==RE::BookMenu::MENU_NAME) albumPapyrus(nullptr);
   if(!event->opening&&(event->menuName==RE::BookMenu::MENU_NAME||event->menuName==RE::InventoryMenu::MENU_NAME)) {
     collectionIntegration::queue([](){finishAlbumOpen();});
   }
   if(event->menuName==RE::DialogueMenu::MENU_NAME&&!event->opening) {
     const auto token=panelOpenRequest.token();
     SKSE::GetTaskInterface()->AddTask([token]() { finishPanelRequest(token); });
   }
   if(event->opening&&(event->menuName==RE::MainMenu::MENU_NAME||event->menuName==RE::LoadingMenu::MENU_NAME)) {
     log::debug("Menu {}: cleanup begin (visible {}, music owned {})",event->menuName.c_str(),visible,musicPlaying);
     if(event->menuName==RE::MainMenu::MENU_NAME)collectionIntegration::reset();
     close(false);
     log::debug("Menu cleanup complete");
   }
   // Release/reclaim queues a FocusMenu hide/show while the view keeps ownership.
   // Only an actual focus loss interrupts Tessera and preserves its table.
   if(!event->opening&&event->menuName==ttcg::ui::focusMenu&&visible&&!worldFocus.switching()&&(!api||!api->HasFocus(view))) interruptPanel();
   return RE::BSEventNotifyControl::kContinue;
 }
};
Events events;
void message(SKSE::MessagingInterface::Message* msg) {
 if(msg->type==SKSE::MessagingInterface::kInputLoaded) {
   api=ttcg::ui::Acquire();
   log::info("{} API acquired at InputLoaded: {}",ttcg::ui::name,api!=nullptr);
 }
 if(msg->type==SKSE::MessagingInterface::kDataLoaded) {
   const auto data=RE::TESDataHandler::GetSingleton();
   const auto plugin=data?data->LookupModByName(campaign::plugin):nullptr;
   const bool pluginLoaded=plugin&&plugin->compileIndex!=0xFF;
   log::info("Required plugin {} loaded: {} ({} regular, {} light plugins)",campaign::plugin,pluginLoaded,data?data->GetLoadedModCount():0,data?data->GetLoadedLightModCount():0);
   if(!pluginLoaded)log::error("TTCG.dll requires an enabled Tessera TCG.esp from the matching Main package; check the active load order");
   const auto expansions=ttcg::loadCardExpansions();ttcg::expansionLoadErrors=expansions.errors;
   log::info("Card expansions: {} sets, {} cards loaded",expansions.sets,expansions.cards);
   for(const auto& error:expansions.errors)log::error("Card expansion: {}",error);
   ttcg::opponentLoadErrors=ttcg::loadOpponentIntegrations();
   log::info("Opponent integrations: {} profiles, {} grants",ttcg::opponentDefinitions.size(),ttcg::npcGrantDefinitions.size());
   for(const auto& error:ttcg::opponentLoadErrors)log::error("Opponent integration: {}",error);
   campaign::logNpcDependencies();
   ttcg::refreshCardArt();
   for(std::size_t i=ttcg::baseCards.size();i<ttcg::cards.size();++i)if(ttcg::cardImages[i].art!=ttcg::cards[i].art)log::warn("Card expansion {}: artwork unavailable; using card back ({})",ttcg::cards[i].id,ttcg::cards[i].art);
   campaign::development=ttcg::DevelopmentSettings::load([](const char* key) {
     const std::string name(key);const std::wstring wide(name.begin(),name.end());wchar_t value[128]{};
     GetPrivateProfileStringW(L"Development",wide.c_str(),L"",value,128,settingsPath.c_str());
     std::string result;for(const auto* c=value;*c;++c)result+=*c<=127?static_cast<char>(*c):'?';return result;
   });
   if(campaign::development.enabled)log::info("Development mode enabled");
   log::debug("Development: enabled {}, grant {}, foils {}, show players {}, unlock players {}, reveal {}, any NPC {}, rules {}, trade {}, wager {}, skill {}, lesson {}, record results {}, regular tournaments {}, invitational tournaments {}, masters tournaments {}, courier tournament hold {}",
     campaign::development.enabled,campaign::development.addAllCards,campaign::development.addAllFoils,campaign::development.showAllPlayers,
     campaign::development.unlockAllPlayers,campaign::development.revealAllCards,campaign::development.allowAnyNPC,campaign::development.rules,campaign::development.tradeRule,campaign::development.wager,campaign::development.opponentSkill,campaign::development.ruleLesson,campaign::development.recordResults,campaign::development.regularTournaments,campaign::development.invitationalTournaments,campaign::development.mastersTournaments,campaign::development.tournamentHold);
   campaign::interfaceSettings.load([](const char* section,const char* key,int fallback){
     const std::string s(section),k(key);const std::wstring ws(s.begin(),s.end()),wk(k.begin(),k.end());
     return static_cast<int>(GetPrivateProfileIntW(ws.c_str(),wk.c_str(),fallback,settingsPath.c_str()));
   });
   campaign::initForms();
   for(const auto& field:hotkeys.fields)hotkeys.*field.member=GetPrivateProfileIntW(L"Input",field.ini,field.defaultKey,settingsPath.c_str());
   hotkeys.normalize();
   for(const auto& option:pauseOptions)worldPauseSettings.*option.member=GetPrivateProfileIntW(L"Interface",option.iniKey,1,settingsPath.c_str())!=0;
   wchar_t back[4096]{};
   GetPrivateProfileStringW(L"Appearance",L"CardBack",L"mosaic",back,4096,settingsPath.c_str());
   ttcg::cardBackPreference=ttcg::cardBackFromSetting(ttcg::artUTF8(std::filesystem::path(back)));
   musicDescriptor=RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSSoundDescriptorForm>(0x814,"Tessera TCG.esp");
   musicEnabled=GetPrivateProfileIntW(L"Audio",L"Enabled",1,settingsPath.c_str())!=0;
   musicPlaying=false;
   scanMusicFolder();
   log::info("Direct music descriptor available: {}, enabled {}",musicDescriptor!=nullptr,musicEnabled);
   RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESContainerChangedEvent>(&events);
   RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESCombatEvent>(&events);
   RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&events);
   RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESDeathEvent>(&events);
   RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESObjectLoadedEvent>(&events);
   RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&events);
   RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
   if(!api) { log::error("{} API not found; core remains available",ttcg::ui::name); return; }
   api->initialize(receiveCommand,interruptPanel);view=1;uiReady=true;
   log::info("TTCG interface ready ({})",ttcg::ui::name);

 } else if(msg->type==SKSE::MessagingInterface::kPreLoadGame||msg->type==SKSE::MessagingInterface::kNewGame||msg->type==SKSE::MessagingInterface::kPostLoadGame) {
   if(msg->type!=SKSE::MessagingInterface::kPostLoadGame)collectionIntegration::reset();
   const char* name=msg->type==SKSE::MessagingInterface::kPreLoadGame?"PreLoadGame":msg->type==SKSE::MessagingInterface::kNewGame?"NewGame":"PostLoadGame";
   log::debug("{}: cleanup begin (visible {}, music owned {})",name,visible,musicPlaying);
   // Checkpoint the active character before loading another save.
   if(msg->type!=SKSE::MessagingInterface::kPreLoadGame)active=false;
   close(false);campaign::syncGameAvailability();
   if(msg->type!=SKSE::MessagingInterface::kPreLoadGame&&
      (msg->type==SKSE::MessagingInterface::kNewGame||msg->data!=nullptr)) {
    // SKSE passes the load result as (void*)result, not a pointer to a bool.
    const auto generation=collectionIntegration::generation.load();
    collectionIntegration::queue([generation](){
     if(generation!=collectionIntegration::generation.load())return;
     campaign::npcFactionTracking.allowDiscovery();releaseSavedOpponent();campaign::recover();collectionIntegration::activate();
    });
   }
   log::debug("{}: cleanup complete",name);
 }
}
}
extern "C" __declspec(dllexport) const TesseraCollectionAPI* Tessera_GetCollectionAPI(std::uint32_t version) noexcept {
 return version==TESSERA_COLLECTION_API_VERSION?&ttcg::collectionAPI::api:nullptr;
}
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version=[]() noexcept {
 SKSE::PluginVersionData data{};
 data.PluginName("TTCG"); data.AuthorName("TTCG"); data.PluginVersion(REL::Version(TTCG_VERSION_MAJOR,TTCG_VERSION_MINOR,TTCG_VERSION_PATCH,0));
 // CommonLib selects engine layouts at runtime.
 data.UsesAddressLibrary(); data.UsesNoStructs();
 return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface* skse,SKSE::PluginInfo* info) {
 info->infoVersion=SKSE::PluginInfo::kVersion;
 info->name="TTCG"; info->version=SKSEPlugin_Version.pluginVersion;
 return !skse->IsEditor()&&(skse->RuntimeVersion()==SKSE::RUNTIME_VR_1_4_15||skse->RuntimeVersion()>=SKSE::RUNTIME_SSE_1_5_97);
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
 const bool diagnostics=GetPrivateProfileIntW(L"Diagnostics",L"Enabled",0,settingsPath.c_str())!=0;
 SKSE::InitInfo initInfo;
 // CommonLib creates the log and flushes at this level, preserving the last
 // match/recovery message if Skyrim exits unexpectedly.
 initInfo.logLevel=diagnostics?spdlog::level::debug:spdlog::level::info;
 SKSE::Init(skse,initInfo);
 log::info("Diagnostics: {}",diagnostics?"enabled":"disabled");
 SKSE::GetPapyrusInterface()->Register([](RE::BSScript::IVirtualMachine* vm) {
   exchangeIntegration::bind(vm);
   vm->RegisterFunction("GetAPIVersion","Tessera_Collection",collectionIntegration::apiVersion);
   vm->RegisterFunction("IsReady","Tessera_Collection",collectionIntegration::isReady);
   vm->RegisterFunction("GetRevision","Tessera_Collection",collectionIntegration::revision);
   vm->RegisterFunction("GetCardKeys","Tessera_Collection",collectionIntegration::cardKeys);
   vm->RegisterFunction("IsCardLoaded","Tessera_Collection",collectionIntegration::cardLoaded);
   vm->RegisterFunction("IsCardPending","Tessera_Collection",collectionIntegration::cardPending);
   vm->RegisterFunction("GetCardName","Tessera_Collection",collectionIntegration::text<&TesseraCollectionCard::name>);
   vm->RegisterFunction("GetCardSet","Tessera_Collection",collectionIntegration::text<&TesseraCollectionCard::setID>);
   vm->RegisterFunction("GetCardTier","Tessera_Collection",collectionIntegration::tier);
   vm->RegisterFunction("GetCardRarity","Tessera_Collection",collectionIntegration::text<&TesseraCollectionCard::rarity>);
   vm->RegisterFunction("GetCardGroups","Tessera_Collection",collectionIntegration::strings<&TesseraCollectionCard::groupsJSON>);
   vm->RegisterFunction("GetCardAffinities","Tessera_Collection",collectionIntegration::strings<&TesseraCollectionCard::affinitiesJSON>);
   vm->RegisterFunction("GetCardArt","Tessera_Collection",collectionIntegration::text<&TesseraCollectionCard::art>);
   vm->RegisterFunction("GetCardThumbnail","Tessera_Collection",collectionIntegration::text<&TesseraCollectionCard::thumbnail>);
   vm->RegisterFunction("GetOwnedCount","Tessera_Collection",collectionIntegration::ownedCount);
   vm->RegisterFunction("IsDiscovered","Tessera_Collection",collectionIntegration::discovered);
   vm->RegisterFunction("GetCardTotal","Tessera_Collection",collectionIntegration::cardTotal);
   vm->RegisterFunction("GetOwnedCardTotal","Tessera_Collection",collectionIntegration::total<collectionIntegration::Total::Owned>);
   vm->RegisterFunction("GetOwnedCopyTotal","Tessera_Collection",collectionIntegration::total<collectionIntegration::Total::Copies>);
   vm->RegisterFunction("GetDiscoveredCardTotal","Tessera_Collection",collectionIntegration::discoveredTotal);
   vm->RegisterFunction("IsCardLoaded","TTCG_CardExpansion",expansionLoadedPapyrus);
   vm->RegisterFunction("GiveCard","TTCG_CardExpansion",expansionGivePapyrus);
   vm->RegisterFunction("GetAvailability","TTCG_Opponents",npcAvailabilityPapyrus);
   vm->RegisterFunction("IsRegistered","TTCG_Opponents",npcRegisteredPapyrus);
   vm->RegisterFunction("AddPlayer","TTCG_Opponents",npcAddPapyrus);
   vm->RegisterFunction("SetEnabled","TTCG_Opponents",npcEnabledPapyrus);
   vm->RegisterFunction("Refresh","TTCG_Opponents",npcRefreshPapyrus);
   vm->RegisterFunction("GiveCard","TTCG_Opponents",npcGivePapyrus);
   vm->RegisterFunction("HasReceivedGrant","TTCG_Opponents",npcGrantedPapyrus);
   vm->RegisterFunction("BeginMatch","TTCG_Native",beginPapyrus);
   vm->RegisterFunction("BackgroundUpdate","TTCG_Native",backgroundUpdatePapyrus);
   vm->RegisterFunction("NextMaster","TTCG_Native",legacyPapyrus::nextMaster);
   vm->RegisterFunction("MasterArrived","TTCG_Native",legacyPapyrus::masterArrived);
   vm->RegisterFunction("MasterResponse","TTCG_Native",legacyPapyrus::masterResponse);
   vm->RegisterFunction("OpenTournaments","TTCG_Native",tournamentPapyrus);
   vm->RegisterFunction("RegisterTournament","TTCG_Native",tournamentEntryPapyrus);
   vm->RegisterFunction("NextTournamentInvitation","TTCG_Native",nextTournamentInvitationPapyrus);
   vm->RegisterFunction("TournamentInvitation","TTCG_Native",tournamentInvitationPapyrus);
   vm->RegisterFunction("OpenCardShop","TTCG_Native",shopPapyrus);
   vm->RegisterFunction("BuyAlbum","TTCG_Native",buyAlbumPapyrus);
   vm->RegisterFunction("OpenAlbum","TTCG_Native",albumPapyrus);
   return true;
 });
 auto serial=SKSE::GetSerializationInterface();
 serial->SetUniqueID(0x54544347); serial->SetSaveCallback([](SKSE::SerializationInterface* serial){checkpointMatch();campaign::save(serial);}); serial->SetLoadCallback([](SKSE::SerializationInterface* serial){collectionIntegration::reset();campaign::load(serial);}); serial->SetRevertCallback([](SKSE::SerializationInterface* serial){collectionIntegration::reset();campaign::revert(serial);});
 SKSE::GetMessagingInterface()->RegisterListener(message);
 log::info("Tessera {} loaded ({} / CommonLibSSE-NG {} / {} / Skyrim {})",TTCG_VERSION,ttcg::ui::name,TTCG_COMMONLIB_VERSION,TTCG_COMMONLIB_COMMIT,REL::Module::get().version().string());
 log::info("CommonLib container fix: {}",TTCG_COMMONLIB_HASHMAP_PATCH);
 if constexpr(sizeof(TTCG_BUILD_LABEL)>1)log::info("Tessera test build: {}",TTCG_BUILD_LABEL);
 return true;
}
