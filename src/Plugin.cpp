#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <chrono>
#include <format>
#include <filesystem>
#include <sstream>
#include <thread>
#include "BuildInfo.h"
#include "UIHost.h"
#include "WorldFocus.h"
#include "Match.h"
#include "Presentation.h"
#include "ArtTransfer.h"
#include "Lesson.h"
#include "MusicPlayback.h"
#include "DropInMusic.h"
#ifdef GetObject
#undef GetObject
#endif
#include "NativeCollection.h"
#include "NativeShop.h"
#include "DevelopmentPresentation.h"
#include "EncounterRecords.h"

namespace {
namespace log=SKSE::log;
ttcg::ui::Host* api{};
ttcg::ui::View view{};
bool domReady=false, visible=false, active=false, thinking=false, settling=false;
std::uint64_t session=0, revision=0;
std::atomic<std::uint64_t> epoch{0};
RE::ActorHandle opponent, pending, approachingMaster;
std::atomic<RE::FormID> masterCandidate{0};
bool masterPresented=false;
std::chrono::steady_clock::time_point approachStarted;
void cancelMasterApproach();
std::string albumSection;
std::string opponentName,screen="lobby",pendingScreen="lobby";
ttcg::HotkeyBindings hotkeys;
auto& collectionKey=hotkeys.collection;
auto& challengeKey=hotkeys.challenge;
std::string bindingCapture,bindingNotice;
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
 log::info("Audio request: {} (session {}, revision {})",editorID,session,revision);
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
 log::info("Rule sound {:08X}: playback {}",descriptor->GetFormID(),started?"submitted":"failed");
}
void publish();
std::uint64_t musicTime() {
 return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void scanMusicFolder() {
 auto scan=ttcg::scanMusic(std::filesystem::path(L"Data\\Music\\TTCG"));
 musicFiles=std::move(scan.files);
 log::info("Music folder: {} tracks, {} duplicate copies, {} rejected files",musicFiles.size(),scan.duplicates,scan.rejected.size());
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
   log::info("Direct music: {} (built {}, submitted {}, handle {})",path,built,submitted,handle.soundID);
   return submitted;
 }
 bool playing() { return handle.IsValid()&&handle.IsPlaying(); }
 void stop() { if(handle.IsValid()) handle.Stop(); handle=RE::BSSoundHandle{}; }
 void failed() { log::warn("Music did not start within 3 seconds: {}",path); }
 void suspend() {
   log::info("Music playing: {}",path);
   auto manager=RE::BSMusicManager::GetSingleton();
   if(!manager) return;
   const bool paused=suspendedMusic.suspend(manager->current,static_cast<RE::BSIMusicType*>(nullptr),
     [](auto* music) { return music->typeStatus==RE::BSIMusicType::MUSIC_STATUS::kPlaying; },
     [](auto* music) { music->DoPause(); return music->typeStatus==RE::BSIMusicType::MUSIC_STATUS::kPaused; });
   if(paused) log::info("Location music paused");
 }
 void restore() {
   auto manager=RE::BSMusicManager::GetSingleton();
   const bool restored=suspendedMusic.restore([manager](auto* music) { return manager&&manager->current==music; },
     [](auto* music) { return music->typeStatus==RE::BSIMusicType::MUSIC_STATUS::kPaused; },
     [](auto* music) { music->DoPlay(); });
   if(restored) log::info("Location music resumed");
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
     SKSE::GetTaskInterface()->AddTask([generation,pendingTick]() {
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
   SKSE::GetTaskInterface()->AddTask([generation,pendingTick](){
    pendingTick->store(false);if(worldMonitorGeneration.load()==generation)checkLiveWorld();
   });
  }
 }).detach();
}
bool updateWorldFocus() {
 const auto result=worldFocus.apply(*api,view,ttcg::shouldPauseWorld(worldPauseSettings,screen,active));
 const auto token=result==ttcg::FocusUpdate::Ready&&worldFocus.unpaused()?epoch.load()+1:0;
 if(unpausedWorldEpoch.exchange(token)!=token){++worldMonitorGeneration;if(token)monitorWorld();}
 watchedOpponent.store(token&&(active||challengeContext||screen=="shop")&&opponent.get()?opponent.get()->GetFormID():0);
 if(result==ttcg::FocusUpdate::Lost){interruptPanel();return false;}
 if(result==ttcg::FocusUpdate::Releasing) {
   const auto token=epoch.load();
   // Meridian queued its release first. Cross that UI-task boundary before
   // returning to the game thread to acquire the new mode and publish state.
   SKSE::GetTaskInterface()->AddUITask([token]() {
     SKSE::GetTaskInterface()->AddTask([token]() {
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
 if(!api||!domReady||!visible) return;
 if(!updateWorldFocus())return;
 auto data=ttcg::snapshotJson(match,session,revision,opponentName,active,thinking,lastResult,settling,musicEnabled,(musicDescriptor&&!musicFiles.empty()));
 auto actor=opponent.get();
 const bool conceal=ttcg::concealRivalHand(match,active);
 data.pop_back(); data+=",\"collection\":"+campaign::json(screen=="shop"?nullptr:actor.get(),screen=="lobby",conceal)+",\"screen\":"+ttcg::quote(screen)+",\"revealCards\":"+(campaign::development.revealAllCards?"true":"false")+"}";
 data.pop_back();data+=",\"albumSection\":"+ttcg::quote(albumSection)+"}";
 data.pop_back();data+=",\"matchContext\":"+std::string(challengeContext?"true":"false")+"}";
 if(screen=="shop"){data.pop_back();data+=",\"shop\":"+campaign::shopJson(actor.get())+"}";}
 if(screen=="lesson"){data.pop_back();data+=",\"lesson\":"+lesson.json()+"}";}
 data.pop_back();data+=std::format(",\"settings\":{{\"collection\":{},\"challenge\":{},\"capturing\":{},\"notice\":{},\"cardBackNotice\":{},\"interfaceNotice\":{},\"development\":{}",collectionKey,challengeKey,ttcg::quote(bindingCapture),ttcg::quote(bindingNotice),ttcg::quote(cardBackNotice),ttcg::quote(interfaceNotice),ttcg::developmentJson(campaign::development,developmentNotice));
 data+=std::format(",\"toggleGames\":{},\"gamesEnabled\":{},\"rankLayout\":{},\"showCardNames\":{}",hotkeys.toggleGames,campaign::interfaceSettings.gamesEnabled?"true":"false",campaign::interfaceSettings.rankLayout,campaign::interfaceSettings.showCardNames?"true":"false");
 for(const auto& option:pauseOptions)data+=","+ttcg::quote(option.jsonKey)+":"+(worldPauseSettings.*option.member?"true":"false");
 data+="}}";
 const auto& saved=campaign::savedMatch;
 data.pop_back();data+=",\"interruptedMatch\":";
 if(saved.present&&!active){const auto* e=ttcg::tournament(campaign::saved,saved.tournament);
  data+="{\"opponent\":"+ttcg::quote(campaign::savedMatchName())+",\"tournament\":"+std::to_string(saved.tournament)+",\"finished\":"+(saved.table.finished()?"true":"false")+",\"venue\":"+ttcg::quote(e?ttcg::tournamentVenues[e->hold-1].name:"")+",\"canResume\":"+(campaign::interfaceSettings.gamesEnabled&&e&&campaign::tournamentVenue()==e->hold?"true":"false")+"}";
 }else data+="null";
 data+="}";
 api->InteropCall(view,"ttcgState",data.c_str());albumSection.clear();
}
void close(bool unwind) {
 if(!unwind)checkpointMatch();
 unpausedWorldEpoch.store(0);watchedOpponent.store(0);++worldMonitorGeneration;releaseOpponent();
 campaign::closeShop();
 if(unwind&&active)campaign::savedMatch={};
 if(!campaign::savedMatch.present)ttcg::forfeitTournamentFoils(campaign::saved);
 if(unwind&&campaign::tournamentID&&active&&!match.finished())ttcg::withdrawTournament(campaign::saved,campaign::tournamentID);
 campaign::tournamentID=0;campaign::tournamentRegistration=0;albumSection.clear();
 cancelMasterApproach();
 pendingAlbum=false;challengeContext=false;bindingCapture.clear();bindingNotice.clear();developmentNotice.clear();cardBackNotice.clear();interfaceNotice.clear();
 if(unwind&&!campaign::savedMatch.present){ttcg::abandonRuleCulture(campaign::saved,campaign::gameHour());campaign::recover();}
 campaign::cultureGame=false;
 stopMusic();
 ++epoch; ++session; revision=0; pending={}; opponent={};
 visible=false; active=false; thinking=false; settling=false; screen="lobby"; pendingScreen="lobby";worldFocus.reset();
 if(api&&api->IsValid(view)) { if(domReady) api->InteropCall(view,"ttcgReset",""); if(api->HasFocus(view)) api->Unfocus(view); api->Hide(view); }
}
bool eligible(RE::Actor* actor) {
 auto player=RE::PlayerCharacter::GetSingleton();
 return actor&&player&&actor!=player&&!player->IsDead()&&!player->IsInCombat()&&campaign::actorReadyForCards(actor)
   &&actor->GetParentCell()==player->GetParentCell()&&actor->GetPosition().GetDistance(player->GetPosition())<600.0F;
}
void cancelMasterApproach(){
 masterCandidate.store(0);masterPresented=false;
 if(auto actor=approachingMaster.get())actor->EvaluatePackage();
 approachingMaster={};
 if(auto data=RE::TESDataHandler::GetSingleton())if(auto g=data->LookupForm<RE::TESGlobal>(ttcg::encounter_masterGlobal,campaign::plugin))g->value=0;
}
bool worldReady(){
 auto player=RE::PlayerCharacter::GetSingleton();auto ui=RE::UI::GetSingleton();
 return player&&ui&&api&&domReady&&!visible&&!player->IsDead()&&!player->IsInCombat()&&!player->IsSneaking()&&!player->GetCurrentScene()&&!ui->GameIsPaused()&&!ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)&&!ui->IsMenuOpen(RE::MainMenu::MENU_NAME)&&!api->HasAnyActiveFocus();
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
  cancelMasterApproach();pending={};
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
void scanMasterApproach(){
 if(worldReady()&&campaign::available()){
  campaign::syncProgression();campaign::updateTournaments();

 }
 if(!campaign::interfaceSettings.gamesEnabled){cancelMasterApproach();return;}
 if(approachingMaster){if(masterPresented)return;if(!worldReady()||std::chrono::steady_clock::now()-approachStarted>std::chrono::seconds(30))cancelMasterApproach();return;}
 if(!worldReady()||!campaign::available()||campaign::savedMatch.present||campaign::saved.contract.pending())return;
 campaign::syncProgression();auto player=RE::PlayerCharacter::GetSingleton();const auto hour=campaign::gameHour(),hold=campaign::actorHold(player);
 for(std::size_t i=0;i<ttcg::opponents.size();++i){const auto& p=ttcg::opponents[i];
  if(p.hold!=hold||!ttcg::canMasterApproach(campaign::saved,int(i),hour)||ttcg::invitationHash(p.base^(hour/6))%3!=0)continue;
  auto actor=campaign::resolveID<RE::Actor>(p.reference);
  if(!actor||!actor->Is3DLoaded()||actor->IsDead()||actor->IsDisabled()||actor->IsInCombat()||actor->IsHostileToActor(player)||actor->GetCurrentScene()||actor->GetParentCell()!=player->GetParentCell()||actor->GetPosition().GetDistance(player->GetPosition())>1200)continue;
  if(!ttcg::reserveMasterApproach(campaign::saved,int(i),hour))continue;
  approachingMaster=actor->GetHandle();approachStarted=std::chrono::steady_clock::now();masterCandidate.store(actor->GetFormID());return;
 }
}
RE::Actor* nextMasterPapyrus(RE::StaticFunctionTag*){
 const auto id=masterCandidate.exchange(0);auto actor=id?RE::TESForm::LookupByID<RE::Actor>(id):nullptr;
 SKSE::GetTaskInterface()->AddTask([](){scanMasterApproach();});return actor;
}
void masterArrivedPapyrus(RE::StaticFunctionTag*,RE::Actor* actor){
 if(!actor)return;auto handle=actor->GetHandle();
 SKSE::GetTaskInterface()->AddTask([handle](){
  auto actor=handle.get();if(!actor||approachingMaster!=handle)return;
  const auto i=campaign::profileIndex(actor.get());
  if(i<0||!worldReady()||!eligible(actor.get())||!campaign::canChallenge(actor.get())){cancelMasterApproach();return;}
  auto data=RE::TESDataHandler::GetSingleton();auto info=data->LookupForm<RE::TESTopicInfo>(ttcg::encounterRecords[i].master,campaign::plugin);auto gate=data->LookupForm<RE::TESGlobal>(ttcg::encounter_masterGlobal,campaign::plugin);
  if(!info||!gate){cancelMasterApproach();return;}gate->value=float(i+1);
  masterPresented=true;if(!actor->SetDialogueWithPlayer(true,true,info))cancelMasterApproach();
 });
}
bool resumeMatch() {
 if(!campaign::interfaceSettings.gamesEnabled||!campaign::recoverSavedMatch())return false;
 const auto saved=campaign::savedMatch;auto actor=campaign::resolveID<RE::Actor>(saved.opponent);
 auto player=RE::PlayerCharacter::GetSingleton();
 const auto* event=ttcg::tournament(campaign::saved,saved.tournament);
 if(!player||player->IsDead()||player->IsInCombat()||!actor||!campaign::actorReadyForCards(actor)||
    (saved.tournament?(!event||campaign::tournamentVenue()!=event->hold):!eligible(actor)))return false;
 if(!api||!domReady||!api->IsValid(view)||(!visible&&api->HasAnyActiveFocus()))return false;
 cancelMasterApproach();campaign::closeShop();
 campaign::tournamentID=saved.tournament;campaign::sessionRules=saved.table.rules;
 campaign::practice=saved.practice;campaign::competitiveMatch=saved.competitive;campaign::cultureGame=saved.culture;
 campaign::rivalDeck=saved.originals()[1];campaign::canStake=saved.competitive&&!saved.tournament;
 opponent=actor->GetHandle();opponentName=actor->GetName();match=saved.table;
 // Captures are atomic. Resume the completed position, without replaying an
 // old capture or choosing another Chaos card. Only a pending redeal advances.
 const bool redealt=match.redeal();
 active=true;visible=true;thinking=false;settling=match.finished()||redealt;
 lastResult={};playedRuleSounds=0;playedResultSound=false;playedOpeningSound=false;
 screen="lobby";challengeContext=true;albumSection.clear();campaign::notice.clear();
 ++epoch;++session;revision=0;checkpointMatch();ttcg::refreshCardArt();scanMusicFolder();api->Show(view);
 updateMusic();publish();runAI();return true;
}
void showPanel(RE::Actor* actor,const std::string& requested="lobby") {
 auto player=RE::PlayerCharacter::GetSingleton();
 const std::string target=(requested=="tournaments"||requested=="registration")?"album":requested;
 if(target!="album"&&target!="lobby"&&target!="shop")return;
 if(!campaign::interfaceSettings.gamesEnabled&&requested!="album")return;
 if(visible||!player||player->IsDead()||player->IsInCombat()||(actor&&!eligible(actor))||(!actor&&target!="album")) return;
 if(!api||!domReady||!api->IsValid(view)) { RE::SendHUDMessage::ShowHUDMessage(std::format("Tessera: {} is unavailable.",ttcg::ui::name).c_str()); return; }
 cancelMasterApproach();
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
void requestPanel(RE::Actor* actor,const std::string& requested="lobby") {
 if(!campaign::interfaceSettings.gamesEnabled||!eligible(actor)||visible) return;
 if(RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) { pending=actor->GetHandle(); pendingScreen=requested; }
 else showPanel(actor,requested);
}
void requestChallenge(RE::Actor* actor) {
 if(!campaign::interfaceSettings.gamesEnabled||!eligible(actor)||visible)return;
 campaign::syncProgression();
 if(!campaign::hasAlbum()){RE::SendHUDMessage::ShowHUDMessage("Buy a Tessera Album from a general-goods merchant.");return;}
 const auto i=campaign::profileIndex(actor);
 if(!(campaign::resumingWith(actor)||campaign::isChild(actor)||(i>=0?campaign::unlocked(i):actor&&campaign::development.allowAnyNPC))){RE::SendHUDMessage::ShowHUDMessage("They are unavailable for a game.");return;}
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
 SKSE::GetTaskInterface()->AddTask([]() {
   auto player=RE::PlayerCharacter::GetSingleton();auto ui=RE::UI::GetSingleton();
   if(visible||pendingAlbum||!campaign::available()||!player||player->IsDead()||player->IsInCombat()||!ui||!domReady||!api||!api->IsValid(view))return;
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
   auto move=ttcg::opponentMove(snapshot,skill,ttcg::matchDecisionSeed(snapshot));
   // Presentation has acknowledged the previous move before this search starts.
   std::this_thread::sleep_for(std::chrono::milliseconds(300));
   if(epoch.load()!=token) return;
   SKSE::GetTaskInterface()->AddTask([move,token,round,stateRevision]() {
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
 // The toggle remains available during play; it checkpoints instead of forfeiting.
 if(verb=="settings"){
  const auto position=input.tellg();std::string key;int value=-1;
  if(!(input>>key>>value)||(input>>extra)||(value!=0&&value!=1)||!bindingCapture.empty())return;
  if(key=="games"){setGamesEnabled(value!=0);return;}
  input.clear();input.seekg(position);
 }
 if(!campaign::interfaceSettings.gamesEnabled&&(verb=="start"||verb=="play"||verb=="prepare"||verb=="rule-lesson"||verb=="shop"))return;
 if(!ttcg::tournamentRewardAllowsCommand(campaign::saved,verb)){publish();return;}
 if(screen=="shop"&&verb!="shop"&&verb!="close"&&verb!="binding"&&verb!="development"&&verb!="cardback"&&verb!="settings")return;
 if(verb=="settings") {
   std::string key;int value=-1;
   if(active||thinking||settling||screen=="lesson"||!bindingCapture.empty()||
      !(input>>key>>value)||(input>>extra)||(value!=0&&value!=1))return;
   const auto option=std::find_if(pauseOptions.begin(),pauseOptions.end(),[&](const auto& entry){return entry.command==key;});
   if(key=="card-names"||key=="card-ranks"){
     interfaceNotice=campaign::interfaceSettings.set(key,value,persistInterfaceSetting)?"":"Couldn't save this setting.";
     publish();return;
   }
   if(option==pauseOptions.end())return;
   if(!WritePrivateProfileStringW(L"Interface",option->iniKey,value?L"1":L"0",settingsPath.c_str())) {
     interfaceNotice="Couldn't save this setting.";publish();return;
   }
   worldPauseSettings.*option->member=value!=0;interfaceNotice.clear();publish();return;
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
   if(!campaign::saved.contract.held)return;
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
   if((input>>extra)||settling||thinking||(active&&!match.finished())||campaign::saved.contract.pending()) return;
   auto actor=opponent.get(); if(!eligible(actor.get())) { close(); return; }
   releaseOpponent();campaign::savedMatch={};
   if(!campaign::prepare(actor.get(),true)) { publish(); return; }
   ++epoch; ++session; revision=0; active=false; lastResult={}; match=ttcg::Match(1,campaign::sessionRules);
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
   log::info("Opening sound {:06X}: playback {}",localID,started?"submitted":"failed");
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
   log::info("Result sound {:06X}: playback {}",localID,started?"submitted":"failed");
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
     if(!campaign::saved.contract.reserve(bank,{deck,campaign::rivalDeck},static_cast<unsigned>(tradeRule),wager,campaign::persistentID(actor.get()))) { campaign::notice="Cards or gold unavailable."; publish(); return; }
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
void masterResponsePapyrus(RE::StaticFunctionTag*,RE::Actor* actor,bool accepted){
 if(!actor)return;auto handle=actor->GetHandle();
 SKSE::GetTaskInterface()->AddTask([handle,accepted](){
  auto actor=handle.get();if(handle==approachingMaster)cancelMasterApproach();
  const auto* profile=campaign::profile(actor.get());if(accepted&&actor&&profile&&profile->level==5&&campaign::canChallenge(actor.get())){campaign::acceptChallenge(actor.get());requestPanel(actor.get());}
 });
}
void beginPapyrus(RE::StaticFunctionTag*,RE::Actor* actor) {
 if(!actor) return;
 auto handle=actor->GetHandle();
 SKSE::GetTaskInterface()->AddTask([handle]() { auto target=handle.get(); if(target&&campaign::canChallenge(target.get())){campaign::acceptChallenge(target.get());requestPanel(target.get());} });
}
void receiveCommand(const char* raw) {
 if(!raw) return;
 std::string text(raw);
 if(!validCommandSize(text)) return;
 // Browser callbacks can run on their UI thread. Game state and audio belong
 // to the game task queue; validate session/revision when the task executes.
 SKSE::GetTaskInterface()->AddTask([text=std::move(text)]() { command(text.c_str()); });
}
void receiveArtRequest(const char* raw){
 if(!raw)return;
 const auto request=ttcg::parseArtRequest(raw);if(!request)return;
 SKSE::GetTaskInterface()->AddTask([request=*request](){
  if(!api||!domReady||!api->IsValid(view))return;
  const auto image=ttcg::readCardArt(request.path);
  static bool reported=false;static unsigned failures=0;
  if(!image.data.empty()&&!reported){reported=true;log::info("Artwork fallback active: reading MO2 virtual Data files through TTCG");}
  if(!image.error.empty()&&failures++<5)log::warn("Artwork fallback: {} ({})",request.path,image.error);
  const auto response="{\"id\":"+std::to_string(request.id)+",\"data\":"+ttcg::quote(image.data)+"}";
  api->InteropCall(view,"ttcgArtResult",response.c_str());
 });
}
RE::TESObjectBOOK* nextTournamentInvitationPapyrus(RE::StaticFunctionTag*){
 const auto request=campaign::tournamentLetterReady.exchange(0);if(!request)return nullptr;
 campaign::tournamentLetterSentAt=campaign::tournamentCourierClock();campaign::tournamentLetterInFlight=request;
 log::info("Tournament {} invitation handed to courier script",unsigned(request>>32));
 return RE::TESForm::LookupByID<RE::TESObjectBOOK>(unsigned(request));
}
void tournamentInvitationPapyrus(RE::StaticFunctionTag*,RE::TESObjectBOOK* letter,bool delivered){
 if(!letter)return;const auto form=letter->GetFormID();const auto request=campaign::tournamentLetterInFlight.load();
 SKSE::GetTaskInterface()->AddTask([form,delivered,request](){
  if(!delivered&&request&&unsigned(request)==form)campaign::acknowledgeTournamentCourier(request);
  campaign::updateTournaments();
 });
}
void tournamentEntryPapyrus(RE::StaticFunctionTag*,RE::Actor* actor){if(actor){const auto handle=actor->GetHandle();SKSE::GetTaskInterface()->AddTask([handle](){if(auto actor=handle.get())requestPanel(actor.get(),"registration");});}}
void tournamentPapyrus(RE::StaticFunctionTag*,RE::Actor* actor){
 if(!actor)return;auto handle=actor->GetHandle();SKSE::GetTaskInterface()->AddTask([handle](){if(auto host=handle.get())requestPanel(host.get(),"tournaments");});
}
void shopPapyrus(RE::StaticFunctionTag*,RE::Actor* actor){
 if(!actor)return;const auto handle=actor->GetHandle();
 SKSE::GetTaskInterface()->AddTask([handle](){if(auto actor=handle.get())requestPanel(actor.get(),"shop");});
}
void buyAlbumPapyrus(RE::StaticFunctionTag*,RE::Actor* actor) {
 if(!actor)return;
 const auto handle=actor->GetHandle();
 SKSE::GetTaskInterface()->AddTask([handle](){
   auto merchant=handle.get();if(!merchant)return;
   const bool bought=campaign::buyAlbum(merchant.get());
   const char* message=bought?"Your Tessera Album includes eight starter cards.":
     campaign::hasAlbum()?"You already own a Tessera Album.":
     campaign::physicalCount(RE::PlayerCharacter::GetSingleton(),campaign::goldForm)<ttcg::albumPrice?"You need 100 gold to buy a Tessera Album.":
     "The album purchase could not be completed.";
   RE::SendHUDMessage::ShowHUDMessage(message);
   log::info("Album purchase callback: {} ({:08X}), eligible {}, success {}",merchant->GetName(),merchant->GetFormID(),vendors::albumEligible(merchant.get()),bought);
 });
}
void logAlbumDialogue() {
 auto manager=RE::MenuTopicManager::GetSingleton();
 auto ref=manager?manager->speaker.get():RE::NiPointer<RE::TESObjectREFR>{};
 auto actor=ref?ref->As<RE::Actor>():nullptr;if(!actor)return;
 auto data=RE::TESDataHandler::GetSingleton();
 auto info=data->LookupForm<RE::TESTopicInfo>(0x832,campaign::plugin);
 auto quest=data->LookupForm<RE::TESQuest>(0x800,campaign::plugin);
 auto player=RE::PlayerCharacter::GetSingleton();
 log::info("Album dialogue: {} ({:08X}), eligible {}, acquired {}, global {}, quest running {}, INFO conditions {}, gold {}",
   actor->GetName(),actor->GetFormID(),vendors::albumEligible(actor),campaign::hasAlbum(),
   campaign::progression?campaign::progression->value:-1,quest&&quest->IsRunning(),
   info&&info->objConditions.IsTrue(actor,player),campaign::physicalCount(player,campaign::goldForm));
}
void interruptWorld() {
 const auto token=unpausedWorldEpoch.load();if(!token)return;
 SKSE::GetTaskInterface()->AddTask([token]() {
   if(unpausedWorldEpoch.load()!=token||epoch.load()+1!=token||!visible||!worldFocus.unpaused())return;
   log::info("Closing unpaused Tessera for a world interruption");interruptPanel();
 });
}
bool watchedActor(RE::TESObjectREFR* actor){return actor&&(actor->GetFormID()==0x14||actor->GetFormID()==watchedOpponent.load());}
class Events final: public RE::BSTEventSink<RE::InputEvent*>, public RE::BSTEventSink<RE::MenuOpenCloseEvent>, public RE::BSTEventSink<RE::TESContainerChangedEvent>, public RE::BSTEventSink<RE::TESCombatEvent>, public RE::BSTEventSink<RE::TESHitEvent>, public RE::BSTEventSink<RE::TESDeathEvent> {
public:
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
    SKSE::GetTaskInterface()->AddTask([id](){if(campaign::savedMatch.present&&campaign::savedMatch.opponent==id&&!active)campaign::recoverSavedMatch();});
   }
   return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
   // Inventory stacks need not have an ObjectReference. The engine event gives
   // us the base form directly, including letters moved by RemoveAllItems.
   if(event&&event->itemCount>0&&
      ((event->newContainer==0x14&&(event->oldContainer==0x39FB9||event->oldContainer==0x39FB7))||event->newContainer==0x39FB9||event->newContainer==0x39FB7)){
     const auto base=event->baseObj;
     // Only a newly added courier copy proves this dispatch. Receiving an
     // older copy of the same BOOK must not acknowledge a new pending request.
     const auto request=!event->oldContainer&&event->newContainer!=0x14?campaign::tournamentLetterInFlight.load():0;
     SKSE::GetTaskInterface()->AddTask([base,request](){
       if(!campaign::available())return;
       const auto letter=campaign::tournamentLetterID(RE::TESForm::LookupByID<RE::TESObjectBOOK>(base));if(!letter)return;
       // The observed addition confirms the exact request even if Papyrus's
       // acknowledgement fails or delivery beats it. A late event cannot
       // accidentally acknowledge the next invitation sharing this BOOK.
       if(request&&unsigned(request)==base)campaign::acknowledgeTournamentCourier(request);
       campaign::updateTournaments();
     });
   }
   if(event&&campaign::albumForm&&event->baseObj==campaign::albumForm->GetFormID()&&event->newContainer==0x14&&event->itemCount>0){
     SKSE::GetTaskInterface()->AddTask([](){
       if(campaign::claimAlbum())RE::SendHUDMessage::ShowHUDMessage("Your Tessera Album includes eight starter cards.");
     });
   }
   return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
   if(!events) return RE::BSEventNotifyControl::kContinue;
   for(auto event=*events;event;event=event->next) {
     auto button=event->AsButtonEvent(); if(!button||!button->IsDown()) continue;
     const auto code=button->GetIDCode();
     if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&collectionKey&&code==collectionKey&&!visible) {
       SKSE::GetTaskInterface()->AddTask([]() {
         if(visible) { if(api) api->InteropCall(view,"ttcgEscape","native"); return; }
         auto ui=RE::UI::GetSingleton(); if(!ui||ui->GameIsPaused()||ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) return;
         if(campaign::interfaceSettings.gamesEnabled&&!campaign::hasAlbum()){RE::SendHUDMessage::ShowHUDMessage("Buy a Tessera Album from a general-goods merchant.");return;}
         showPanel(nullptr,"album");
       });
     }
     if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&campaign::interfaceSettings.gamesEnabled&&challengeKey&&code==challengeKey&&!visible) {
       SKSE::GetTaskInterface()->AddTask([]() {
         if(visible) { if(api) api->InteropCall(view,"ttcgEscape","native"); return; }
         auto ui=RE::UI::GetSingleton(); if(!ui||ui->GameIsPaused()||ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) return;
         if(!campaign::interfaceSettings.gamesEnabled)return;
         auto pick=RE::CrosshairPickData::GetSingleton();
         auto ref=pick?pick->GetActiveTarget().get():RE::NiPointer<RE::TESObjectREFR>{};
         auto actor=ref?ref->As<RE::Actor>():nullptr;
         if(!eligible(actor)) { RE::SendHUDMessage::ShowHUDMessage("Face a nearby NPC to challenge them."); return; }
         requestChallenge(actor);
       });
     }
     if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&hotkeys.toggleGames&&code==hotkeys.toggleGames&&!visible){
       SKSE::GetTaskInterface()->AddTask([](){
         const auto ui=RE::UI::GetSingleton();
         if(visible||!ui||ui->GameIsPaused()||ui->IsMenuOpen(RE::MainMenu::MENU_NAME))return;
         setGamesEnabled(!campaign::interfaceSettings.gamesEnabled);
       });
     }
     if(visible&&api&&!api->HandlesController()&&button->GetDevice()==RE::INPUT_DEVICE::kGamepad) {
       const char* key=code==1?"up":code==2?"down":code==4?"left":code==8?"right":code==4096?"confirm":code==8192?"cancel":code==256?"previous":code==512?"next":nullptr;
       if(key&&api) api->InteropCall(view,"ttcgKey",key);
     }
   }
   return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
   if(!event) return RE::BSEventNotifyControl::kContinue;
   if(event->opening&&event->menuName==RE::DialogueMenu::MENU_NAME){
     interruptWorld();
     SKSE::GetTaskInterface()->AddTask([](){campaign::claimAlbum();campaign::updateTournaments();logAlbumDialogue();});
   }
   if(event->opening&&event->menuName==RE::BookMenu::MENU_NAME) albumPapyrus(nullptr);
   if(!event->opening&&(event->menuName==RE::BookMenu::MENU_NAME||event->menuName==RE::InventoryMenu::MENU_NAME)) {
     SKSE::GetTaskInterface()->AddTask([](){finishAlbumOpen();});
   }
   if(event->menuName==RE::DialogueMenu::MENU_NAME&&!event->opening) {
     cancelMasterApproach();
     const auto handle=pending; const auto requested=pendingScreen; pending={};
     SKSE::GetTaskInterface()->AddTask([handle,requested]() { auto target=handle.get(); if(target) showPanel(target.get(),requested); });
   }
   if(event->opening&&(event->menuName==RE::MainMenu::MENU_NAME||event->menuName==RE::LoadingMenu::MENU_NAME)) {
     log::info("Menu {}: cleanup begin (visible {}, music owned {})",event->menuName.c_str(),visible,musicPlaying);
     close(false);
     log::info("Menu cleanup complete");
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
   campaign::development=ttcg::DevelopmentSettings::load([](const char* key) {
     const std::string name(key);const std::wstring wide(name.begin(),name.end());wchar_t value[128]{};
     GetPrivateProfileStringW(L"Development",wide.c_str(),L"",value,128,settingsPath.c_str());
     std::string result;for(const auto* c=value;*c;++c)result+=*c<=127?static_cast<char>(*c):'?';return result;
   });
   log::info("Development: enabled {}, grant {}, foils {}, show players {}, unlock players {}, reveal {}, any NPC {}, rules {}, trade {}, wager {}, skill {}, lesson {}, record results {}, regular tournaments {}, invitational tournaments {}, masters tournaments {}, courier tournament hold {}",
     campaign::development.enabled,campaign::development.addAllCards,campaign::development.addAllFoils,campaign::development.showAllPlayers,
     campaign::development.unlockAllPlayers,campaign::development.revealAllCards,campaign::development.allowAnyNPC,campaign::development.rules,campaign::development.tradeRule,campaign::development.wager,campaign::development.opponentSkill,campaign::development.ruleLesson,campaign::development.recordResults,campaign::development.regularTournaments,campaign::development.invitationalTournaments,campaign::development.mastersTournaments,campaign::development.tournamentHold);
   campaign::interfaceSettings.gamesEnabled=GetPrivateProfileIntW(L"Gameplay",L"Enabled",1,settingsPath.c_str())!=0;
   campaign::interfaceSettings.showCardNames=GetPrivateProfileIntW(L"Appearance",L"ShowCardNames",1,settingsPath.c_str())!=0;
   campaign::interfaceSettings.rankLayout=GetPrivateProfileIntW(L"Appearance",L"RankLayout",0,settingsPath.c_str())==1?1:0;
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
   RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&events);
   RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
   if(!api) { log::error("{} API not found; core remains available",ttcg::ui::name); return; }
   view=api->CreateView([](ttcg::ui::View ready) {
     // CEF DOM-ready is not a Skyrim task. Keep native state on the game thread.
     SKSE::GetTaskInterface()->AddTask([ready]() {
       if(!api||view!=ready||!api->IsValid(ready)) return;
       if(!api->RegisterJSListener(ready,"ttcgCommand",receiveCommand)) { log::error("Could not register TTCG bridge"); return; }
       if(!api->RegisterJSListener(ready,"ttcgReadArt",receiveArtRequest))log::warn("Could not register TTCG artwork fallback");
       domReady=true;
       api->EnableController(ready);
       if(visible) publish(); else api->Hide(ready);
       log::info("TTCG view ready ({})",ttcg::ui::name);
     });
   });
   if(!view) { log::error("{} could not create TTCG view",ttcg::ui::name); return; }
   api->Hide(view);
 } else if(msg->type==SKSE::MessagingInterface::kPreLoadGame||msg->type==SKSE::MessagingInterface::kNewGame||msg->type==SKSE::MessagingInterface::kPostLoadGame) {
   const char* name=msg->type==SKSE::MessagingInterface::kPreLoadGame?"PreLoadGame":msg->type==SKSE::MessagingInterface::kNewGame?"NewGame":"PostLoadGame";
   log::info("{}: cleanup begin (visible {}, music owned {})",name,visible,musicPlaying);
   // Only PreLoad still belongs to the old save. Do not write its table into
   // a newly loaded character after the serialization callbacks have run.
   if(msg->type!=SKSE::MessagingInterface::kPreLoadGame)active=false;
   close(false);campaign::syncGameAvailability();
   if(msg->type==SKSE::MessagingInterface::kNewGame) SKSE::GetTaskInterface()->AddTask([](){ campaign::recover(); });
   if(msg->type==SKSE::MessagingInterface::kPostLoadGame) SKSE::GetTaskInterface()->AddTask([](){ releaseSavedOpponent();campaign::recover(); });
   log::info("{}: cleanup complete",name);
 }
}
}
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version=[]() noexcept {
 SKSE::PluginVersionData data{};
 data.PluginName("TTCG"); data.AuthorName("TTCG"); data.PluginVersion(REL::Version(TTCG_VERSION_MAJOR,TTCG_VERSION_MINOR,TTCG_VERSION_PATCH,0));
 // CommonLib selects engine layouts at runtime; this DLL has no fixed-layout overrides.
 data.UsesAddressLibrary(); data.UsesNoStructs();
 return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface* skse,SKSE::PluginInfo* info) {
 info->infoVersion=SKSE::PluginInfo::kVersion;
 info->name="TTCG"; info->version=SKSEPlugin_Version.pluginVersion;
 return !skse->IsEditor()&&(skse->RuntimeVersion()==SKSE::RUNTIME_VR_1_4_15||skse->RuntimeVersion()>=SKSE::RUNTIME_SSE_1_5_97);
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
 auto path=SKSE::log::log_directory();
 if(path) {
   *path/="TTCG.log";
   auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true);
   auto logger=std::make_shared<spdlog::logger>("TTCG",sink);
   spdlog::set_default_logger(logger); spdlog::flush_on(spdlog::level::info);
 }
 SKSE::Init(skse);
 SKSE::GetPapyrusInterface()->Register([](RE::BSScript::IVirtualMachine* vm) {
   vm->RegisterFunction("BeginMatch","TTCG_Native",beginPapyrus);
   vm->RegisterFunction("NextMaster","TTCG_Native",nextMasterPapyrus);
   vm->RegisterFunction("MasterArrived","TTCG_Native",masterArrivedPapyrus);
   vm->RegisterFunction("MasterResponse","TTCG_Native",masterResponsePapyrus);
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
 serial->SetUniqueID(0x54544347); serial->SetSaveCallback([](SKSE::SerializationInterface* serial){checkpointMatch();campaign::save(serial);}); serial->SetLoadCallback([](SKSE::SerializationInterface* serial){campaign::load(serial);SKSE::GetTaskInterface()->AddTask([](){cancelMasterApproach();});}); serial->SetRevertCallback(campaign::revert);
 SKSE::GetMessagingInterface()->RegisterListener(message);
 log::info("Tessera {} loaded ({} / CommonLibSSE-NG {} / {} / Skyrim {})",TTCG_VERSION,ttcg::ui::name,TTCG_COMMONLIB_VERSION,TTCG_COMMONLIB_COMMIT,REL::Module::get().version().string()); return true;
}
