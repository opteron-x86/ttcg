// Shared interface state and small helpers that touch it.
#include "PluginState.h"

namespace tessera_plugin {
ttcg::ui::Host* api{};
ttcg::ui::View view{};
bool uiReady = false, visible = false, active = false, thinking = false, settling = false;
std::uint64_t session = 0, revision = 0;
std::atomic<std::uint64_t> epoch{0};
RE::ActorHandle opponent, pending, enrollmentTarget;
ttcg::PanelOpenRequest panelOpenRequest;
std::string panelWaitReason;
void cancelPanelRequest() {
    panelOpenRequest.cancel();
    pending = {};
    panelWaitReason.clear();
}
std::string albumSection;
std::string opponentName, screen = "lobby", pendingScreen = "lobby";
ttcg::HotkeyBindings hotkeys;
auto& collectionKey = hotkeys.collection;
auto& challengeKey = hotkeys.challenge;
std::string bindingCapture, bindingNotice;
unsigned heldHotkeyModifiers() {
    const auto manager = RE::BSInputDeviceManager::GetSingleton();
    const auto keyboard = manager ? manager->GetKeyboard() : nullptr;
    if (!keyboard) return 0;
    const auto& keys = keyboard->GetRuntimeData().curState;
    return ttcg::heldHotkeyModifiers([&](unsigned key) { return key < sizeof(keys) && (keys[key] & 0x80) != 0; });
}
std::string developmentNotice;
std::string cardBackNotice;
ttcg::WorldPauseSettings worldPauseSettings;
std::string interfaceNotice;
ttcg::WorldFocus worldFocus;
ttcg::PresentationWait presentationWait;
bool panelMonitoring = false;
std::atomic<std::uint64_t> unpausedWorldEpoch{0}, worldMonitorGeneration{0};
std::atomic<RE::FormID> watchedOpponent{0};
RE::ActorHandle waitingOpponent;
RE::TESPackage* waitingPackage{};
bool pendingAlbum = false, challengeContext = false;
ttcg::Match match;
ttcg::Result lastResult;
ttcg::Lesson lesson;
unsigned playedRuleSounds = 0;
bool playedResultSound = false, playedOpeningSound = false;
RE::BGSSoundDescriptorForm* musicDescriptor{};
std::vector<std::filesystem::path> musicFiles;
ttcg::FolderPlaylist folderPlaylist;
std::atomic<std::uint64_t> musicGeneration{0};
bool musicEnabled = true, musicPlaying = false;
ttcg::SuspendedMusic<RE::BSIMusicType> suspendedMusic;
const auto settingsPath = std::filesystem::absolute(L"Data\\SKSE\\Plugins\\TTCG.ini").wstring();
} // namespace tessera_plugin
