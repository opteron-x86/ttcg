// Engine event sinks and SKSE lifecycle messages.
#include "PluginState.h"

namespace tessera_plugin {
void logAlbumDialogue() {
    // These engine queries exist only for diagnosis, not dialogue availability.
    if (!spdlog::should_log(spdlog::level::debug)) return;
    auto manager = RE::MenuTopicManager::GetSingleton();
    auto ref = manager ? manager->speaker.get() : RE::NiPointer<RE::TESObjectREFR>{};
    auto actor = ref ? ref->As<RE::Actor>() : nullptr;
    if (!actor) return;
    auto data = RE::TESDataHandler::GetSingleton();
    auto info = data->LookupForm<RE::TESTopicInfo>(0x832, campaign::plugin);
    auto quest = data->LookupForm<RE::TESQuest>(0x800, campaign::plugin);
    auto player = RE::PlayerCharacter::GetSingleton();
    log::debug(
        "Album dialogue: {} ({:08X}), eligible {}, acquired {}, global {}, quest running {}, INFO conditions {}, gold {}",
        actor->GetName(), actor->GetFormID(), vendors::albumEligible(actor), campaign::hasAlbum(),
        campaign::progression ? campaign::progression->value : -1, quest && quest->IsRunning(),
        info && info->objConditions.IsTrue(actor, player), campaign::physicalCount(player, campaign::goldForm));
}
void interruptWorld() {
    const auto token = unpausedWorldEpoch.load();
    if (!token) return;
    collectionIntegration::queue([token]() {
        if (unpausedWorldEpoch.load() != token || epoch.load() + 1 != token || !visible || !worldFocus.unpaused())
            return;
        log::info("Closing unpaused Tessera for a world interruption");
        interruptPanel();
    });
}
bool watchedActor(RE::TESObjectREFR* actor) {
    return actor && (actor->GetFormID() == 0x14 || actor->GetFormID() == watchedOpponent.load());
}
// Merge requests while a roster refresh is queued or running. SKSE can drain
// its task queue from different engine threads; claiming a refresh is atomic.
std::atomic_bool npcRefreshQueued{false};
void queueNpcRefresh() {
    if (npcRefreshQueued.exchange(true)) return;
    collectionIntegration::queue([]() {
        if (campaign::available()) {
            static std::atomic_uint profileCount{0};
            const bool profile = spdlog::should_log(spdlog::level::debug) &&
                                 profileCount.fetch_add(1, std::memory_order_relaxed) % 12 == 0;
            ttcg::NpcSyncTiming syncTiming;
            const auto started = std::chrono::steady_clock::now();
            campaign::syncProgression(profile ? &syncTiming : nullptr);
            const auto elapsed =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            if (elapsed >= 16.0)
                log::warn("Slow NPC load refresh: {:.1f} ms ({} opponents)", elapsed, ttcg::opponents.size());
            if (profile) logNpcSyncTiming(syncTiming, "object load");
        }
        npcRefreshQueued = false;
    });
}
class Events final : public RE::BSTEventSink<RE::InputEvent*>,
                     public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
                     public RE::BSTEventSink<RE::TESContainerChangedEvent>,
                     public RE::BSTEventSink<RE::TESCombatEvent>,
                     public RE::BSTEventSink<RE::TESHitEvent>,
                     public RE::BSTEventSink<RE::TESDeathEvent>,
                     public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event,
                                          RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override {
        if (event && event->loaded) {
            const auto id = event->formID;
            const auto generation = campaign::npcFactionTracking.generation();
            collectionIntegration::queue([id, generation]() {
                if (!campaign::available() || !campaign::npcFactionTracking.current(generation)) return;
                auto actor = RE::TESForm::LookupByID<RE::Actor>(id);
                if (actor) {
                    campaign::registerNpcActor(actor);
                    const bool tagged = campaign::observeNpcFactions(actor, generation);
                    if (campaign::profileIndex(actor) >= 0 || tagged) queueNpcRefresh();
                }
            });
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* event,
                                          RE::BSTEventSource<RE::TESCombatEvent>*) override {
        if (event && event->newState != RE::ACTOR_COMBAT_STATE::kNone &&
            (watchedActor(event->actor.get()) || watchedActor(event->targetActor.get())))
            interruptWorld();
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event, RE::BSTEventSource<RE::TESHitEvent>*) override {
        if (event && watchedActor(event->target.get())) interruptWorld();
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* event,
                                          RE::BSTEventSource<RE::TESDeathEvent>*) override {
        if (event && watchedActor(event->actorDying.get())) interruptWorld();
        if (event && event->actorDying) {
            const auto id = campaign::persistentID(event->actorDying.get());
            collectionIntegration::queue([id]() {
                if (campaign::savedMatch.present && campaign::savedMatch.opponent == id && !active)
                    campaign::recoverSavedMatch();
            });
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,
                                          RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
        // Inventory stacks need not have an ObjectReference. The engine event gives
        // us the base form directly, including letters moved by RemoveAllItems.
        if (event && event->itemCount > 0 &&
            ((event->newContainer == 0x14 && (event->oldContainer == 0x39FB9 || event->oldContainer == 0x39FB7)) ||
             event->newContainer == 0x39FB9 || event->newContainer == 0x39FB7)) {
            const auto base = event->baseObj;
            // A newly delivered copy confirms this invitation request.
            const auto request =
                !event->oldContainer && event->newContainer != 0x14 ? campaign::tournamentLetterInFlight.load() : 0;
            collectionIntegration::queue([base, request]() {
                if (!campaign::available()) return;
                const auto letter = campaign::tournamentLetterID(RE::TESForm::LookupByID<RE::TESObjectBOOK>(base));
                if (!letter) return;
                // Confirm the matching request when its letter arrives.
                if (request && unsigned(request) == base) campaign::acknowledgeTournamentCourier(request);
                campaign::updateTournaments();
            });
        }
        if (event && campaign::albumForm && event->baseObj == campaign::albumForm->GetFormID() &&
            event->newContainer == 0x14 && event->itemCount > 0) {
            collectionIntegration::queue([]() {
                if (campaign::claimAlbum())
                    RE::SendHUDMessage::ShowHUDMessage("Your Tessera Album includes eight starter cards.");
            });
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,
                                          RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!events) return RE::BSEventNotifyControl::kContinue;
        const auto modifiers = heldHotkeyModifiers();
        for (auto event = *events; event; event = event->next) {
            auto button = event->AsButtonEvent();
            if (!button || !button->IsDown()) continue;
            const auto code = button->GetIDCode();
            if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                ttcg::hotkeyMatches(collectionKey, code, modifiers) && !visible) {
                collectionIntegration::queue([]() {
                    if (visible) {
                        if (api) api->Escape(view);
                        return;
                    }
                    auto ui = RE::UI::GetSingleton();
                    if (!ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) return;
                    if (campaign::interfaceSettings.gamesEnabled && !campaign::hasAlbum()) {
                        RE::SendHUDMessage::ShowHUDMessage("Buy a Tessera Album from a general-goods merchant.");
                        return;
                    }
                    showPanel(nullptr, "album");
                });
            }
            if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard && campaign::interfaceSettings.gamesEnabled &&
                ttcg::hotkeyMatches(challengeKey, code, modifiers) && !visible) {
                collectionIntegration::queue([]() {
                    if (visible) {
                        if (api) api->Escape(view);
                        return;
                    }
                    auto ui = RE::UI::GetSingleton();
                    if (!ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) return;
                    if (!campaign::interfaceSettings.gamesEnabled) return;
                    auto pick = RE::CrosshairPickData::GetSingleton();
                    auto ref = pick ? pick->GetActiveTarget().get() : RE::NiPointer<RE::TESObjectREFR>{};
                    auto actor = ref ? ref->As<RE::Actor>() : nullptr;
                    if (!eligible(actor)) {
                        RE::SendHUDMessage::ShowHUDMessage("Face a nearby NPC to challenge them.");
                        return;
                    }
                    requestChallenge(actor);
                });
            }
            if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                ttcg::hotkeyMatches(hotkeys.toggleGames, code, modifiers) && !visible) {
                collectionIntegration::queue([]() {
                    const auto ui = RE::UI::GetSingleton();
                    if (visible || !ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) return;
                    setGamesEnabled(!campaign::interfaceSettings.gamesEnabled);
                });
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (event->opening && event->menuName == RE::DialogueMenu::MENU_NAME) {
            interruptWorld();
            collectionIntegration::queue([]() {
                campaign::registerNearbyNpcs();
                campaign::syncProgression();
                campaign::claimAlbum();
                campaign::updateTournaments();
                logAlbumDialogue();
            });
        }
        if (event->opening && event->menuName == RE::BookMenu::MENU_NAME) albumPapyrus(nullptr);
        if (!event->opening &&
            (event->menuName == RE::BookMenu::MENU_NAME || event->menuName == RE::InventoryMenu::MENU_NAME)) {
            collectionIntegration::queue([]() { finishAlbumOpen(); });
        }
        if (event->menuName == RE::DialogueMenu::MENU_NAME && !event->opening) {
            const auto token = panelOpenRequest.token();
            SKSE::GetTaskInterface()->AddTask([token]() { finishPanelRequest(token); });
        }
        if (event->opening &&
            (event->menuName == RE::MainMenu::MENU_NAME || event->menuName == RE::LoadingMenu::MENU_NAME)) {
            log::debug("Menu {}: cleanup begin (visible {}, music owned {})", event->menuName.c_str(), visible,
                       musicPlaying);
            if (event->menuName == RE::MainMenu::MENU_NAME) collectionIntegration::reset();
            close(false);
            log::debug("Menu cleanup complete");
        }
        // Release/reclaim queues a FocusMenu hide/show while the view keeps ownership.
        // Only an actual focus loss interrupts Tessera and preserves its table.
        if (!event->opening && event->menuName == ttcg::ui::focusMenu && visible && !worldFocus.switching() &&
            (!api || !api->HasFocus(view)))
            interruptPanel();
        return RE::BSEventNotifyControl::kContinue;
    }
};
Events events;
void message(SKSE::MessagingInterface::Message* msg) {
    if (msg->type == SKSE::MessagingInterface::kInputLoaded) {
        api = ttcg::ui::Acquire();
        log::info("{} API acquired at InputLoaded: {}", ttcg::ui::name, api != nullptr);
    }
    if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
        const auto data = RE::TESDataHandler::GetSingleton();
        const auto plugin = data ? data->LookupModByName(campaign::plugin) : nullptr;
        const bool pluginLoaded = plugin && plugin->compileIndex != 0xFF;
        log::info("Required plugin {} loaded: {} ({} regular, {} light plugins)", campaign::plugin, pluginLoaded,
                  data ? data->GetLoadedModCount() : 0, data ? data->GetLoadedLightModCount() : 0);
        if (!pluginLoaded)
            log::error(
                "TTCG.dll requires an enabled Tessera TCG.esp from the matching Main package; check the active load order");
        const auto expansions = ttcg::loadCardExpansions();
        ttcg::expansionLoadErrors = expansions.errors;
        log::info("Card expansions: {} sets, {} cards loaded", expansions.sets, expansions.cards);
        for (const auto& error : expansions.errors) log::error("Card expansion: {}", error);
        ttcg::opponentLoadErrors = ttcg::loadOpponentIntegrations();
        log::info("Opponent integrations: {} profiles, {} grants", ttcg::opponentDefinitions.size(),
                  ttcg::npcGrantDefinitions.size());
        for (const auto& error : ttcg::opponentLoadErrors) log::error("Opponent integration: {}", error);
        campaign::logNpcDependencies();
        ttcg::refreshCardArt();
        for (std::size_t i = ttcg::baseCards.size(); i < ttcg::cards.size(); ++i)
            if (ttcg::cardImages[i].art != ttcg::cards[i].art)
                log::warn("Card expansion {}: artwork unavailable; using card back ({})", ttcg::cards[i].id,
                          ttcg::cards[i].art);
        campaign::development = ttcg::DevelopmentSettings::load([](const char* key) {
            const std::string name(key);
            const std::wstring wide(name.begin(), name.end());
            wchar_t value[128]{};
            GetPrivateProfileStringW(L"Development", wide.c_str(), L"", value, 128, settingsPath.c_str());
            std::string result;
            for (const auto* c = value; *c; ++c) result += *c <= 127 ? static_cast<char>(*c) : '?';
            return result;
        });
        if (campaign::development.enabled) log::info("Development mode enabled");
        log::debug(
            "Development: enabled {}, grant {}, foils {}, show players {}, unlock players {}, reveal {}, any NPC {}, rules {}, trade {}, wager {}, skill {}, lesson {}, record results {}, regular tournaments {}, invitational tournaments {}, masters tournaments {}, courier tournament hold {}",
            campaign::development.enabled, campaign::development.addAllCards, campaign::development.addAllFoils,
            campaign::development.showAllPlayers, campaign::development.unlockAllPlayers,
            campaign::development.revealAllCards, campaign::development.allowAnyNPC, campaign::development.rules,
            campaign::development.tradeRule, campaign::development.wager, campaign::development.opponentSkill,
            campaign::development.ruleLesson, campaign::development.recordResults,
            campaign::development.regularTournaments, campaign::development.invitationalTournaments,
            campaign::development.mastersTournaments, campaign::development.tournamentHold);
        campaign::interfaceSettings.load([](const char* section, const char* key, int fallback) {
            const std::string s(section), k(key);
            const std::wstring ws(s.begin(), s.end()), wk(k.begin(), k.end());
            return static_cast<int>(GetPrivateProfileIntW(ws.c_str(), wk.c_str(), fallback, settingsPath.c_str()));
        });
        campaign::initForms(GetPrivateProfileIntW(L"Diagnostics", L"VendorPackInjection", 1, settingsPath.c_str()) !=
                            0);
        for (const auto& field : hotkeys.fields)
            hotkeys.*field.member = GetPrivateProfileIntW(L"Input", field.ini, field.defaultKey, settingsPath.c_str());
        hotkeys.normalize();
        for (const auto& option : pauseOptions)
            worldPauseSettings.*option.member =
                GetPrivateProfileIntW(L"Interface", option.iniKey, 1, settingsPath.c_str()) != 0;
        wchar_t back[4096]{};
        GetPrivateProfileStringW(L"Appearance", L"CardBack", L"mosaic", back, 4096, settingsPath.c_str());
        ttcg::cardBackPreference = ttcg::cardBackFromSetting(ttcg::artUTF8(std::filesystem::path(back)));
        musicDescriptor =
            RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSSoundDescriptorForm>(0x814, "Tessera TCG.esp");
        musicEnabled = GetPrivateProfileIntW(L"Audio", L"Enabled", 1, settingsPath.c_str()) != 0;
        musicPlaying = false;
        scanMusicFolder();
        log::info("Direct music descriptor available: {}, enabled {}", musicDescriptor != nullptr, musicEnabled);
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESContainerChangedEvent>(&events);
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESCombatEvent>(&events);
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&events);
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESDeathEvent>(&events);
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESObjectLoadedEvent>(&events);
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&events);
        RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
        if (!api) {
            log::error("{} API not found; core remains available", ttcg::ui::name);
            return;
        }
        api->initialize(receiveCommand, interruptPanel);
        view = 1;
        uiReady = true;
        log::info("TTCG interface ready ({})", ttcg::ui::name);

    } else if (msg->type == SKSE::MessagingInterface::kPreLoadGame || msg->type == SKSE::MessagingInterface::kNewGame ||
               msg->type == SKSE::MessagingInterface::kPostLoadGame) {
        if (msg->type != SKSE::MessagingInterface::kPostLoadGame) collectionIntegration::reset();
        const char* name = msg->type == SKSE::MessagingInterface::kPreLoadGame ? "PreLoadGame"
                           : msg->type == SKSE::MessagingInterface::kNewGame   ? "NewGame"
                                                                               : "PostLoadGame";
        log::debug("{}: cleanup begin (visible {}, music owned {})", name, visible, musicPlaying);
        // Checkpoint the active character before loading another save.
        if (msg->type != SKSE::MessagingInterface::kPreLoadGame) active = false;
        close(false);
        campaign::syncGameAvailability();
        if (msg->type != SKSE::MessagingInterface::kPreLoadGame &&
            (msg->type == SKSE::MessagingInterface::kNewGame || msg->data != nullptr)) {
            // SKSE passes the load result as (void*)result, not a pointer to a bool.
            const auto generation = collectionIntegration::generation.load();
            collectionIntegration::queue([generation]() {
                if (generation != collectionIntegration::generation.load()) return;
                campaign::npcFactionTracking.allowDiscovery();
                releaseSavedOpponent();
                campaign::recover();
                collectionIntegration::activate();
            });
        }
        log::debug("{}: cleanup complete", name);
    }
}
} // namespace tessera_plugin
