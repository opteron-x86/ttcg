#pragma once
// State and functions shared by the TTCG.dll sources. Everything here was
// previously private to Plugin.cpp; see the per-file comments for ownership.
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

namespace tessera_plugin {
namespace log = SKSE::log;

// Defined in PluginState.cpp.
extern ttcg::ui::Host* api;
extern ttcg::ui::View view;
extern bool uiReady, visible, active, thinking, settling;
extern std::uint64_t session, revision;
extern std::atomic<std::uint64_t> epoch;
extern RE::ActorHandle opponent, pending, enrollmentTarget;
extern ttcg::PanelOpenRequest panelOpenRequest;
extern std::string panelWaitReason;
extern std::string albumSection;
extern std::string opponentName, screen, pendingScreen;
extern ttcg::HotkeyBindings hotkeys;
extern unsigned& collectionKey;
extern unsigned& challengeKey;
extern std::string bindingCapture, bindingNotice;
extern std::string developmentNotice;
extern std::string cardBackNotice;
extern ttcg::WorldPauseSettings worldPauseSettings;
extern std::string interfaceNotice;
extern ttcg::WorldFocus worldFocus;
extern ttcg::PresentationWait presentationWait;
extern bool panelMonitoring;
extern std::atomic<std::uint64_t> unpausedWorldEpoch, worldMonitorGeneration;
extern std::atomic<RE::FormID> watchedOpponent;
extern RE::ActorHandle waitingOpponent;
extern RE::TESPackage* waitingPackage;
extern bool pendingAlbum, challengeContext;
extern ttcg::Match match;
extern ttcg::Result lastResult;
extern ttcg::Lesson lesson;
extern unsigned playedRuleSounds;
extern bool playedResultSound, playedOpeningSound;
extern RE::BGSSoundDescriptorForm* musicDescriptor;
extern std::vector<std::filesystem::path> musicFiles;
extern ttcg::FolderPlaylist folderPlaylist;
extern std::atomic<std::uint64_t> musicGeneration;
extern bool musicEnabled, musicPlaying;
extern ttcg::SuspendedMusic<RE::BSIMusicType> suspendedMusic;
extern const std::wstring settingsPath;

struct PauseOption {
    std::string_view command, jsonKey;
    const wchar_t* iniKey;
    bool ttcg::WorldPauseSettings::* member;
};
constexpr std::array pauseOptions{
    PauseOption{"pause-album", "pauseWorldInAlbum", L"PauseWorldInAlbum", &ttcg::WorldPauseSettings::album},
    PauseOption{"pause-shop", "pauseWorldInShops", L"PauseWorldInShops", &ttcg::WorldPauseSettings::shops},
    PauseOption{"pause-setup", "pauseWorldInMatchSetup", L"PauseWorldInMatchSetup", &ttcg::WorldPauseSettings::setup},
    PauseOption{"pause-match", "pauseWorldDuringMatches", L"PauseWorldDuringMatches",
                &ttcg::WorldPauseSettings::matches}};

void close(bool unwind = true);

// PluginState.cpp
void cancelPanelRequest();
unsigned heldHotkeyModifiers();

// Music.cpp
void playSound(const char* editorID);
void playRuleSound(unsigned bit);
std::uint64_t musicTime();
void scanMusicFolder();
void stopMusic();
void monitorMusic();
void updateMusic();

// World.cpp
void checkpointMatch();
void releaseOpponent();
void waitOpponent();
void releaseSavedOpponent();
void interruptPanel();
bool checkLiveWorld();
void monitorWorld();
bool updateWorldFocus();

// Panel.cpp
void publish();
bool eligible(RE::Actor* actor);
bool worldReady();
bool resumeMatch();
void showPanel(RE::Actor* actor, const std::string& requested = "lobby");
void logPanelFailure(RE::Actor* actor, const std::string& requested, std::uint64_t elapsed, const char* reason);
void finishPanelRequest(std::uint64_t token);
void requestPanel(RE::Actor* actor, const std::string& requested = "lobby");
void queuePanelRequest(RE::Actor* actor, std::string requested);
void requestChallenge(RE::Actor* actor);
void finishAlbumOpen();

// MatchFlow.cpp
void recordResult(const ttcg::Result& result);
void runAI();

// Commands.cpp
bool persistInterfaceSetting(const char* section, const char* key, int value);
bool setGamesEnabled(bool enabled);
bool validCommandSize(std::string_view text);
void command(const char* raw);

// Papyrus.cpp
void logNpcSyncTiming(const ttcg::NpcSyncTiming& timing, std::string_view source);
void backgroundUpdate();
void backgroundUpdatePapyrus(RE::StaticFunctionTag*);
void albumPapyrus(RE::StaticFunctionTag*);
int npcAvailabilityPapyrus(RE::StaticFunctionTag*, RE::Actor* actor);
bool npcRegisteredPapyrus(RE::StaticFunctionTag* tag, RE::Actor* actor);
bool npcGrantedPapyrus(RE::StaticFunctionTag*, RE::Actor* actor, RE::BSFixedString grant);
void npcAddPapyrus(RE::StaticFunctionTag*, RE::Actor* actor);
void npcRefreshPapyrus(RE::StaticFunctionTag*, RE::Actor* actor);
void npcEnabledPapyrus(RE::StaticFunctionTag*, RE::Actor* actor, bool enabled);
void npcGivePapyrus(RE::StaticFunctionTag*, RE::Actor* actor, RE::BSFixedString card, RE::BSFixedString grant,
                    int quantity, bool foil);
void beginPapyrus(RE::StaticFunctionTag*, RE::Actor* actor);
void receiveCommand(const char* raw);
RE::TESObjectBOOK* nextTournamentInvitationPapyrus(RE::StaticFunctionTag*);
void tournamentInvitationPapyrus(RE::StaticFunctionTag*, RE::TESObjectBOOK* letter, bool delivered);
void tournamentEntryPapyrus(RE::StaticFunctionTag*, RE::Actor* actor);
void tournamentPapyrus(RE::StaticFunctionTag*, RE::Actor* actor);
void shopPapyrus(RE::StaticFunctionTag*, RE::Actor* actor);
bool expansionLoadedPapyrus(RE::StaticFunctionTag*, RE::BSFixedString key);
void expansionGivePapyrus(RE::StaticFunctionTag*, RE::BSFixedString key, std::int32_t quantity, bool foil);
void buyAlbumPapyrus(RE::StaticFunctionTag*, RE::Actor* actor);

// Events.cpp
void logAlbumDialogue();
void interruptWorld();
bool watchedActor(RE::TESObjectREFR* actor);
void queueNpcRefresh();
void message(SKSE::MessagingInterface::Message* msg);

namespace legacyPapyrus {
RE::Actor* nextMaster(RE::StaticFunctionTag*);
void masterArrived(RE::StaticFunctionTag*, RE::Actor*);
void masterResponse(RE::StaticFunctionTag*, RE::Actor*, bool);
}
} // namespace tessera_plugin
