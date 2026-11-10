// Interface publishing, panel open/close requests and match resumption.
#include "PluginState.h"

namespace tessera_plugin {
void publish() {
    if (!api || !uiReady || !visible) return;
    if (!updateWorldFocus()) return;
    campaign::updateRegionalRules();
    auto data = ttcg::snapshotJson(match, session, revision, opponentName, active, thinking, lastResult, settling,
                                   musicEnabled, (musicDescriptor && !musicFiles.empty()));
    auto actor = opponent.get();
    const bool conceal = ttcg::concealRivalHand(match, active);
    data.pop_back();
    data += ",\"collection\":" + campaign::json(screen == "shop" ? nullptr : actor.get(), screen == "lobby", conceal) +
            ",\"screen\":" + ttcg::quote(screen) +
            ",\"revealCards\":" + (campaign::development.revealAllCards ? "true" : "false") + "}";
    data.pop_back();
    data += ",\"albumSection\":" + ttcg::quote(albumSection) + "}";
    data.pop_back();
    data += ",\"matchContext\":" + std::string(challengeContext ? "true" : "false") + "}";
    if (screen == "shop") {
        data.pop_back();
        data += ",\"shop\":" + campaign::shopJson(actor.get()) + "}";
    }
    if (screen == "lesson") {
        data.pop_back();
        data += ",\"lesson\":" + lesson.json() + "}";
    }
    data.pop_back();
    data += std::format(
        ",\"settings\":{{\"collection\":{},\"challenge\":{},\"capturing\":{},\"notice\":{},\"cardBackNotice\":{},\"interfaceNotice\":{},\"development\":{}",
        collectionKey, challengeKey, ttcg::quote(bindingCapture), ttcg::quote(bindingNotice),
        ttcg::quote(cardBackNotice), ttcg::quote(interfaceNotice),
        ttcg::developmentJson(campaign::development, developmentNotice));
    data += ",\"toggleGames\":" + std::to_string(hotkeys.toggleGames);
    data += ",\"heldModifiers\":" + std::to_string(heldHotkeyModifiers());
    for (const auto& f : ttcg::InterfaceSettings::fields) {
        const int value = campaign::interfaceSettings.get(f);
        data += "," + ttcg::quote(f.json) + ":" +
                (f.boolean() ? std::string(value ? "true" : "false") : std::to_string(value));
    }
    auto enroll = enrollmentTarget.get();
    const auto candidate = enroll.get();
    const bool canEnroll = campaign::canEnrollNpc(candidate);
    data += ",\"residentCandidate\":" + ttcg::quote(campaign::npcInspector(candidate)) +
            ",\"canAddResident\":" + (canEnroll ? "true" : "false");
    for (const auto& option : pauseOptions)
        data += "," + ttcg::quote(option.jsonKey) + ":" + (worldPauseSettings.*option.member ? "true" : "false");
    data += "}}";
    const auto& saved = campaign::savedMatch;
    data.pop_back();
    data += ",\"interruptedMatch\":";
    if (saved.present && !active) {
        const auto* e = ttcg::tournament(campaign::saved, saved.tournament);
        data += "{\"opponent\":" + ttcg::quote(campaign::savedMatchName()) +
                ",\"tournament\":" + std::to_string(saved.tournament) +
                ",\"finished\":" + (saved.table.finished() ? "true" : "false") +
                ",\"venue\":" + ttcg::quote(e ? ttcg::tournamentVenues[e->hold - 1].name : "") + ",\"canResume\":" +
                (campaign::interfaceSettings.gamesEnabled && e && campaign::tournamentVenue() == e->hold ? "true"
                                                                                                         : "false") +
                "}";
    } else
        data += "null";
    data += "}";
    api->State(view, data);
    albumSection.clear();
}
void close(bool unwind) {
    if (!unwind) checkpointMatch();
    campaign::updateRegionalRules();
    campaign::cultureHeldHold = 0;
    unpausedWorldEpoch.store(0);
    watchedOpponent.store(0);
    ++worldMonitorGeneration;
    panelMonitoring = false;
    presentationWait.reset();
    releaseOpponent();
    campaign::closeShop();
    if (unwind && active) campaign::savedMatch = {};
    if (!campaign::savedMatch.present) ttcg::forfeitTournamentFoils(campaign::saved);
    if (unwind && campaign::tournamentID && active && !match.finished())
        ttcg::withdrawTournament(campaign::saved, campaign::tournamentID);
    campaign::tournamentID = 0;
    campaign::tournamentRegistration = 0;
    albumSection.clear();
    pendingAlbum = false;
    challengeContext = false;
    bindingCapture.clear();
    bindingNotice.clear();
    developmentNotice.clear();
    cardBackNotice.clear();
    interfaceNotice.clear();
    if (unwind && !campaign::savedMatch.present) {
        ttcg::abandonRuleCulture(campaign::saved, campaign::gameHour());
        campaign::recover();
    }
    campaign::cultureGame = false;
    stopMusic();
    ++epoch;
    ++session;
    revision = 0;
    cancelPanelRequest();
    opponent = {};
    visible = false;
    active = false;
    thinking = false;
    settling = false;
    screen = "lobby";
    pendingScreen = "lobby";
    worldFocus.reset();
    if (api && api->IsValid(view)) {
        if (uiReady) api->Reset(view);
        if (api->HasFocus(view)) api->Unfocus(view);
        api->Hide(view);
    }
    collectionIntegration::publish();
}
bool eligible(RE::Actor* actor) {
    auto player = RE::PlayerCharacter::GetSingleton();
    return actor && player && actor != player && !player->IsDead() && !player->IsInCombat() &&
           campaign::actorReadyForCards(actor) && actor->GetParentCell() == player->GetParentCell() &&
           actor->GetPosition().GetDistance(player->GetPosition()) < 600.0F;
}
bool worldReady() {
    auto player = RE::PlayerCharacter::GetSingleton();
    auto ui = RE::UI::GetSingleton();
    return player && ui && api && uiReady && !visible && !player->IsDead() && !player->IsInCombat() &&
           !player->IsSneaking() && !player->GetCurrentScene() && !ui->GameIsPaused() &&
           !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME) &&
           !api->HasAnyActiveFocus();
}
void showPanel(RE::Actor* actor, const std::string& requested);
bool resumeMatch() {
    if (!campaign::interfaceSettings.gamesEnabled || !campaign::recoverSavedMatch()) return false;
    const auto saved = campaign::savedMatch;
    auto actor = campaign::resolveID<RE::Actor>(saved.opponent);
    auto player = RE::PlayerCharacter::GetSingleton();
    const auto* event = ttcg::tournament(campaign::saved, saved.tournament);
    if (!player || player->IsDead() || player->IsInCombat() || !actor || !campaign::actorReadyForCards(actor) ||
        (saved.tournament ? (!event || campaign::tournamentVenue() != event->hold) : !eligible(actor)))
        return false;
    if (!api || !uiReady || !api->IsValid(view) || (!visible && api->HasAnyActiveFocus())) return false;
    campaign::closeShop();
    campaign::tournamentID = saved.tournament;
    campaign::sessionRules = saved.table.rules;
    campaign::practice = saved.practice;
    campaign::competitiveMatch = saved.competitive;
    campaign::cultureGame = saved.culture;
    const auto* profile = campaign::profile(actor);
    campaign::cultureHeldHold = !saved.tournament && !(profile && (profile->personalRules || profile->traveller))
                                    ? campaign::actorHold(actor)
                                    : 0;
    campaign::rivalDeck = saved.originals()[1];
    campaign::canStake = saved.competitive && !saved.tournament;
    opponent = actor->GetHandle();
    opponentName = actor->GetName();
    match = saved.table;
    // Resume the settled board and advance a pending redeal.
    const bool redealt = match.redeal();
    active = true;
    visible = true;
    thinking = false;
    settling = match.finished() || redealt;
    lastResult = {};
    playedRuleSounds = 0;
    playedResultSound = false;
    playedOpeningSound = false;
    screen = "lobby";
    challengeContext = true;
    albumSection.clear();
    campaign::notice.clear();
    ++epoch;
    ++session;
    revision = 0;
    checkpointMatch();
    ttcg::refreshCardArt();
    scanMusicFolder();
    api->Show(view);
    updateMusic();
    publish();
    runAI();
    return true;
}
void showPanel(RE::Actor* actor, const std::string& requested) {
    if (actor) campaign::registerNpcActor(actor);
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!visible) {
        auto pick = RE::CrosshairPickData::GetSingleton();
        auto ref = pick ? pick->GetActiveTarget().get() : RE::NiPointer<RE::TESObjectREFR>{};
        auto target = ref ? ref->As<RE::Actor>() : nullptr;
        enrollmentTarget = target ? target->GetHandle() : RE::ActorHandle{};
    }
    const std::string target = (requested == "tournaments" || requested == "registration") ? "album" : requested;
    if (target != "album" && target != "lobby" && target != "shop") return;
    if (!campaign::interfaceSettings.gamesEnabled && requested != "album") return;
    if (visible || !player || player->IsDead() || player->IsInCombat() || (actor && !eligible(actor)) ||
        (!actor && target != "album"))
        return;
    if (!api || !uiReady || !api->IsValid(view)) {
        RE::SendHUDMessage::ShowHUDMessage(std::format("Tessera: {} is unavailable.", ttcg::ui::name).c_str());
        return;
    }
    if (api->HasAnyActiveFocus()) {
        RE::SendHUDMessage::ShowHUDMessage("Close the other menu first.");
        return;
    }
    campaign::recoverSavedMatch();
    if (campaign::savedMatch.present && target == "shop") {
        RE::SendHUDMessage::ShowHUDMessage("Finish your interrupted game before trading cards.");
        return;
    }
    if (campaign::savedMatch.present && target == "lobby") {
        if (campaign::resumingWith(actor) && resumeMatch()) return;
        RE::SendHUDMessage::ShowHUDMessage(("Finish your game with " + campaign::savedMatchName() + " first.").c_str());
        return;
    }
    if (campaign::savedMatch.present && campaign::savedMatch.tournament && requested == "registration") {
        const auto* e = ttcg::tournament(campaign::saved, campaign::savedMatch.tournament);
        const auto* p = campaign::profile(actor);
        if (e && p && campaign::tournamentVenue() == e->hold &&
            std::string_view(p->name) == ttcg::tournamentVenues[e->hold - 1].host && resumeMatch())
            return;
    }
    if (target == "lobby" && actor && !campaign::profile(actor) && !campaign::isChild(actor) &&
        !campaign::development.allowAnyNPC) {
        RE::SendHUDMessage::ShowHUDMessage("They don't play Tessera.");
        return;
    }
    albumSection = (requested == "tournaments" || requested == "registration") ? "tournaments" : "";
    campaign::tournamentID = 0;
    campaign::tournamentRegistration = 0;
    if (requested == "registration" && actor) {
        const auto* p = campaign::profile(actor);
        const auto venue = campaign::tournamentVenue();
        if (p && venue && std::string_view(p->name) == ttcg::tournamentVenues[venue - 1].host)
            campaign::tournamentRegistration = venue;
    }
    campaign::syncProgression();
    if (target == "lobby" && !campaign::canChallenge(actor)) {
        RE::SendHUDMessage::ShowHUDMessage(campaign::hasAlbum() ? "They are unavailable for a game."
                                                                : "Buy a Tessera Album from a general-goods merchant.");
        return;
    }
    if (target == "lobby") campaign::acceptChallenge(actor);
    campaign::updateTournaments();
    if (requested == "tournaments") ttcg::discoverAllTournaments(campaign::saved, campaign::gameHour());
    if (!campaign::prepare(target == "lobby" ? actor : nullptr, true)) {
        RE::SendHUDMessage::ShowHUDMessage(campaign::notice.c_str());
        return;
    }
    if (target == "shop" && !campaign::openShop(actor)) {
        RE::SendHUDMessage::ShowHUDMessage("Card trading is unavailable here.");
        return;
    }
    opponent = actor ? actor->GetHandle() : RE::ActorHandle{};
    opponentName = actor ? actor->GetName() : "";
    screen = target;
    challengeContext = screen == "lobby";
    ++session;
    revision = 0;
    active = false;
    thinking = false;
    lastResult = {};
    settling = false;
    match = ttcg::Match(1, campaign::sessionRules);
    visible = true;
    ttcg::refreshCardArt();
    scanMusicFolder();
    api->Show(view);
    if (!updateWorldFocus()) return;
    updateMusic();
    publish();
    log::info("TTCG {}: {}", screen, opponentName);
}
void logPanelFailure(RE::Actor* actor, const std::string& requested, std::uint64_t elapsed, const char* reason) {
    const auto scene = actor ? actor->GetCurrentScene() : nullptr;
    const auto package = actor ? actor->GetCurrentPackage() : nullptr;
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto playerScene = player ? player->GetCurrentScene() : nullptr;
    log::warn(
        "Panel request {} for {} ({:08X}) failed after {} ms: {}; scene {:08X} (playing {}, stopping {}), quest {:08X}, package {:08X} (type {}), player scene {:08X}",
        requested, actor ? actor->GetName() : "unavailable NPC", actor ? actor->GetFormID() : 0, elapsed, reason,
        scene ? scene->GetFormID() : 0, scene ? scene->isPlaying : false, scene ? scene->isShuttingDown : 0,
        scene && scene->parentQuest ? scene->parentQuest->GetFormID() : 0, package ? package->GetFormID() : 0,
        package ? int(package->packData.packType.underlying()) : -1, playerScene ? playerScene->GetFormID() : 0);
}
void finishPanelRequest(std::uint64_t token) {
    if (!panelOpenRequest.active(token)) return;
    auto actor = pending.get();
    auto player = RE::PlayerCharacter::GetSingleton();
    auto ui = RE::UI::GetSingleton();
    const auto now = musicTime(), elapsed = panelOpenRequest.elapsed(now);
    auto reject = [&](const char* reason, const char* message) {
        logPanelFailure(actor.get(), pendingScreen, elapsed, reason);
        cancelPanelRequest();
        if (message) RE::SendHUDMessage::ShowHUDMessage(message);
    };
    if (visible || !campaign::interfaceSettings.gamesEnabled) {
        cancelPanelRequest();
        return;
    }
    if (!player || !ui || !campaign::available() || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) ||
        ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
        cancelPanelRequest();
        return;
    }
    if (!actor || actor.get() == player || player->IsDead() || player->IsInCombat()) {
        reject("player or NPC unavailable", "Tessera is unavailable right now.");
        return;
    }
    if (actor->GetParentCell() != player->GetParentCell() ||
        actor->GetPosition().GetDistance(player->GetPosition()) >= 600.f) {
        reject("NPC moved out of reach", "They are too far away. Speak to them again.");
        return;
    }
    const auto actorBlock = campaign::actorCardBlock(actor.get());
    if (actorBlock != campaign::ActorCardBlock::None && actorBlock != campaign::ActorCardBlock::Scene) {
        const char* reason = actorBlock == campaign::ActorCardBlock::Combat          ? "NPC in combat"
                             : actorBlock == campaign::ActorCardBlock::Hostile       ? "NPC hostile"
                             : actorBlock == campaign::ActorCardBlock::Dead          ? "NPC dead"
                             : actorBlock == campaign::ActorCardBlock::Disabled      ? "NPC disabled"
                             : actorBlock == campaign::ActorCardBlock::ChildDialogue ? "child dialogue/quest gate"
                                                                                     : "ineligible actor";
        reject(reason, "They are unavailable for Tessera right now.");
        return;
    }
    if (!api || !uiReady || !api->IsValid(view)) {
        reject("native UI unavailable", "Tessera's interface is unavailable.");
        return;
    }
    std::string reason;
    if (ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
        const auto manager = RE::MenuTopicManager::GetSingleton();
        if (manager && manager->speaker && manager->speaker != actor->GetHandle()) {
            reject("another conversation started", nullptr);
            return;
        }
        reason = "dialogue closing";
    } else if (actorBlock == campaign::ActorCardBlock::Scene)
        reason = "NPC scene";
    else if (player->GetCurrentScene())
        reason = "player scene";
    else if (ui->GameIsPaused() || api->HasAnyActiveFocus())
        reason = "another menu still owns focus";
    if (reason != panelWaitReason) {
        panelWaitReason = reason;
        log::debug("Panel request {} for {}: {}", pendingScreen, actor->GetName(),
                   reason.empty() ? "ready; waiting for menu state to settle" : reason);
    }
    const auto step = panelOpenRequest.poll(token, now, !reason.empty());
    if (step == ttcg::PanelOpenRequest::Step::Expired) {
        reject(reason.empty() ? "menu transition timed out" : reason.c_str(),
               reason == "NPC scene" || reason == "player scene"
                   ? "They are busy. Try speaking to them again in a moment."
                   : "Tessera couldn't open. Please try again.");
        return;
    }
    if (step != ttcg::PanelOpenRequest::Step::Open) return;
    const auto requested = pendingScreen;
    cancelPanelRequest();
    showPanel(actor.get(), requested);
    collectionIntegration::publish();
    if (!visible)
        logPanelFailure(actor.get(), requested, elapsed, "opening rejected; see in-game message");
    else
        log::debug("Panel request {} for {} opened after {} ms", requested, actor->GetName(), elapsed);
}
void requestPanel(RE::Actor* actor, const std::string& requested) {
    if (!actor || visible || !campaign::interfaceSettings.gamesEnabled) return;
    pending = actor->GetHandle();
    pendingScreen = requested;
    panelWaitReason.clear();
    const auto token = panelOpenRequest.begin(musicTime());
    log::info("Panel requested: {} for {} ({:08X})", requested, actor->GetName(), actor->GetFormID());
    finishPanelRequest(token);
    if (!panelOpenRequest.current(token)) return;
    auto queued = std::make_shared<std::atomic_bool>(false);
    std::thread([token, queued]() {
        while (panelOpenRequest.current(token)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ttcg::PanelOpenRequest::retryMs));
            if (!panelOpenRequest.current(token)) break;
            if (queued->exchange(true)) continue;
            // Publish the collection once the menu opens.
            SKSE::GetTaskInterface()->AddTask([token, queued]() {
                queued->store(false);
                finishPanelRequest(token);
            });
        }
    }).detach();
}
void queuePanelRequest(RE::Actor* actor, std::string requested) {
    if (!actor) return;
    const auto handle = actor->GetHandle();
    const auto generation = collectionIntegration::generation.load();
    collectionIntegration::queue([handle, generation, requested = std::move(requested)]() {
        if (generation != collectionIntegration::generation.load()) return;
        if (auto actor = handle.get()) requestPanel(actor.get(), requested);
    });
}
void requestChallenge(RE::Actor* actor) {
    campaign::registerNpcActor(actor);
    if (!campaign::interfaceSettings.gamesEnabled || !eligible(actor) || visible) return;
    campaign::syncProgression();
    if (!campaign::hasAlbum()) {
        RE::SendHUDMessage::ShowHUDMessage("Buy a Tessera Album from a general-goods merchant.");
        return;
    }
    const auto i = campaign::profileIndex(actor);
    if (!(campaign::resumingWith(actor) || campaign::isChild(actor) ||
          (i >= 0 ? campaign::unlocked(i) : actor && campaign::development.allowAnyNPC))) {
        RE::SendHUDMessage::ShowHUDMessage("They are unavailable for a game.");
        return;
    }
    if (i >= 0 && ttcg::opponents[i].dynamic) {
        const int ready = campaign::npcAvailability(actor);
        if (!ready) {
            RE::SendHUDMessage::ShowHUDMessage("They are unavailable for a game.");
            return;
        }
        auto info = campaign::npcResponse(actor, ready);
        if (info) {
            if (info->objConditions.IsTrue(actor, RE::PlayerCharacter::GetSingleton()))
                actor->SetDialogueWithPlayer(true, true, info);
            else
                RE::SendHUDMessage::ShowHUDMessage("They are unavailable for a game.");
            return;
        }
        if (ready == 1) {
            requestPanel(actor);
        } else
            RE::SendHUDMessage::ShowHUDMessage(ready == 3 ? "They don't have enough cards for a game."
                                                          : "They don't want to play right now.");
        return;
    }
    const bool accepted =
        campaign::resumingWith(actor) || i < 0 ||
        (ttcg::opponentHasCards(campaign::saved, i) &&
         (campaign::development.unlockAllPlayers || ttcg::willingToPlay(campaign::saved, i, campaign::gameHour())));
    const bool alternate = i >= 0 && ttcg::opponents[i].alternateBase &&
                           campaign::persistentID(actor->GetActorBase()) == ttcg::opponents[i].alternateBase;
    const auto id = i < 0       ? ttcg::encounter_childAccept
                    : alternate ? (accepted ? ttcg::encounter_alternateAccept : ttcg::encounter_alternateDecline)
                    : accepted  ? ttcg::encounterRecords[i].accept
                                : ttcg::encounterRecords[i].decline;
    auto info = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESTopicInfo>(id, campaign::plugin);
    if (info) actor->SetDialogueWithPlayer(true, true, info);
}
void finishAlbumOpen() {
    if (!pendingAlbum) return;
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    if (ui->IsMenuOpen(RE::BookMenu::MENU_NAME) || ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) return;
    pendingAlbum = false;
    if (!ui->GameIsPaused() && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) showPanel(nullptr, "album");
}
} // namespace tessera_plugin
