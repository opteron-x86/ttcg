// Sound effects and the folder music playlist.
#include "PluginState.h"

namespace tessera_plugin {
void playSound(const char* editorID) {
    log::debug("Audio request: {} (session {}, revision {})", editorID, session, revision);
    RE::PlaySound(editorID);
}
void playRuleSound(unsigned bit) {
    const RE::FormID localID = bit == 1                  ? 0x813
                               : bit == 2                ? 0x811
                               : bit == ttcg::Legion     ? 0x86B
                               : bit == ttcg::Decimation ? 0x86C
                                                         : 0x812;
    auto descriptor =
        RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSSoundDescriptorForm>(localID, "Tessera TCG.esp");
    auto audio = RE::BSAudioManager::GetSingleton();
    if (!descriptor || !descriptor->soundDescriptor || !audio) {
        log::error("Rule sound unavailable: {:06X}", localID);
        return;
    }
    const bool started = audio->Play(descriptor);
    if (started)
        log::debug("Rule sound {:08X}: playback submitted", descriptor->GetFormID());
    else
        log::warn("Rule sound {:08X}: playback failed", descriptor->GetFormID());
}
void publish();
std::uint64_t musicTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
void scanMusicFolder() {
    auto scan = ttcg::scanMusic(std::filesystem::path(L"Data\\Music\\TTCG"));
    musicFiles = std::move(scan.files);
    log::debug("Music folder: {} tracks, {} duplicate copies, {} rejected files", musicFiles.size(), scan.duplicates,
               scan.rejected.size());
    for (const auto& path : scan.rejected) log::warn("Invalid XWM: {}", path.string());
}
struct MusicAudio {
    RE::BSSoundHandle handle;
    std::string path;
    bool play(const std::filesystem::path& file) {
        auto audio = RE::BSAudioManager::GetSingleton();
        auto definition =
            musicDescriptor ? skyrim_cast<RE::BGSStandardSoundDef*>(musicDescriptor->soundDescriptor) : nullptr;
        auto defaults = RE::BGSDefaultObjectManager::GetSingleton();
        auto category =
            defaults ? defaults->GetObject<RE::BGSSoundCategory>(RE::DEFAULT_OBJECTS::kMusicSoundCategory) : nullptr;
        if (!audio || !definition || !category || !ttcg::validXwm(file)) return false;
        path = file.lexically_relative(std::filesystem::path(L"Data")).string();
        std::replace(path.begin(), path.end(), '/', '\\');
        RE::BSResource::ID id{};
        id.GenerateFromPath(path.c_str());
        // This dedicated descriptor is private to the one-at-a-time playlist.
        // Resource IDs are Data-relative, including Music\\TTCG\\.
        definition->soundFiles.clear();
        definition->soundFiles.push_back(id);
        definition->category = category;
        definition->soundCharacteristics.staticAttenuation = 0;
        handle = RE::BSSoundHandle{};
        const bool built = audio->GetSoundHandle(handle, musicDescriptor);
        const bool submitted = built && handle.Play();
        if (submitted)
            log::debug("Direct music: {} (built {}, submitted {}, handle {})", path, built, submitted, handle.soundID);
        else
            log::warn("Music playback failed: {} (built {})", path, built);
        return submitted;
    }
    bool playing() { return handle.IsValid() && handle.IsPlaying(); }
    void stop() {
        if (handle.IsValid()) handle.Stop();
        handle = RE::BSSoundHandle{};
    }
    void failed() { log::warn("Music did not start within 3 seconds: {}", path); }
    void suspend() {
        log::debug("Music playing: {}", path);
        auto manager = RE::BSMusicManager::GetSingleton();
        if (!manager) return;
        const bool paused = suspendedMusic.suspend(
            manager->current, static_cast<RE::BSIMusicType*>(nullptr),
            [](auto* music) { return music->typeStatus == RE::BSIMusicType::MUSIC_STATUS::kPlaying; },
            [](auto* music) {
                music->DoPause();
                return music->typeStatus == RE::BSIMusicType::MUSIC_STATUS::kPaused;
            });
        if (paused) log::debug("Location music paused");
    }
    void restore() {
        auto manager = RE::BSMusicManager::GetSingleton();
        const bool restored = suspendedMusic.restore(
            [manager](auto* music) { return manager && manager->current == music; },
            [](auto* music) { return music->typeStatus == RE::BSIMusicType::MUSIC_STATUS::kPaused; },
            [](auto* music) { music->DoPlay(); });
        if (restored) log::debug("Location music resumed");
    }

} musicAudio;
void stopMusic() {
    ++musicGeneration;
    folderPlaylist.stop(musicAudio);
    musicPlaying = false;
}
void monitorMusic() {
    const auto generation = ++musicGeneration;
    auto pendingTick = std::make_shared<std::atomic<bool>>(false);
    std::thread([generation, pendingTick]() {
        while (musicGeneration.load() == generation) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            if (musicGeneration.load() != generation) break;
            if (pendingTick->exchange(true)) continue;
            collectionIntegration::queue([generation, pendingTick]() {
                pendingTick->store(false);
                if (musicGeneration.load() != generation) return;
                folderPlaylist.tick(musicAudio, musicTime());
                musicPlaying = folderPlaylist.active();
                if (!musicPlaying) {
                    ++musicGeneration;
                    log::warn("Playlist stopped: no playable tracks");
                    publish();
                }
            });
        }
    }).detach();
}
void updateMusic() {
    const bool wanted =
        musicEnabled && visible && (screen == "lobby" || challengeContext) && musicDescriptor && !musicFiles.empty();
    if (wanted == musicPlaying) return;
    if (!wanted) {
        stopMusic();
        return;
    }
    musicPlaying = folderPlaylist.start(musicFiles, musicAudio, musicTime());
    if (musicPlaying)
        monitorMusic();
    else
        log::error("Could not start any music from Data/Music/TTCG");
}
} // namespace tessera_plugin
