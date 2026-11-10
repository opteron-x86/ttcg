// Opponent waiting, live-world checks and world focus.
#include "PluginState.h"

namespace tessera_plugin {
void runAI();
void checkpointMatch() {
    if (!active) return;
    auto actor = opponent.get();
    if (!actor) return;
    auto& saved = campaign::savedMatch;
    saved.present = true;
    saved.opponent = campaign::persistentID(actor.get());
    saved.ledger = campaign::ledgerKey(actor.get());
    saved.tournament = campaign::tournamentID;
    saved.table = match;
    saved.practice = campaign::practice;
    saved.competitive = campaign::competitiveMatch;
    saved.culture = campaign::cultureGame;
    saved.waiting = bool(waitingPackage);
    saved.waitPackage = waitingPackage ? campaign::persistentID(waitingPackage) : 0;
}
void releaseOpponent() {
    auto actor = waitingOpponent.get();
    auto package = waitingPackage;
    waitingOpponent = {};
    waitingPackage = nullptr;
    if (package) {
        campaign::savedMatch.waiting = false;
        campaign::savedMatch.waitPackage = 0;
    }
    if (actor && package && actor->GetCurrentPackage() == package) {
        actor->EndInterruptPackage(false);
        actor->EvaluatePackage();
    }
}
void waitOpponent() {
    if (waitingPackage || !active || !worldFocus.unpaused()) return;
    auto actor = opponent.get();
    auto player = RE::PlayerCharacter::GetSingleton();
    // Tournament opponents need not be physically present. Leave remote actors' AI alone.
    if (!actor || !player || actor->IsDead() || actor->IsInCombat() || actor->GetCurrentScene() ||
        actor->GetParentCell() != player->GetParentCell() ||
        actor->GetPosition().GetDistance(player->GetPosition()) >= 600.f)
        return;
    auto previous = actor->GetCurrentPackage();
    if (previous && previous->packData.packType == RE::PACKAGE_TYPE::kDoNothing) return;
    actor->InitiateDoNothingPackage();
    auto package = actor->GetCurrentPackage();
    if (package && package != previous && package->packData.packType == RE::PACKAGE_TYPE::kDoNothing) {
        waitingOpponent = actor->GetHandle();
        waitingPackage = package;
        checkpointMatch();
    }
}
void releaseSavedOpponent() {
    auto& saved = campaign::savedMatch;
    if (saved.present && saved.waiting)
        if (auto actor = campaign::resolveID<RE::Actor>(saved.opponent)) {
            auto package = actor->GetCurrentPackage();
            if (package && package->packData.packType == RE::PACKAGE_TYPE::kDoNothing &&
                campaign::persistentID(package) == saved.waitPackage) {
                actor->EndInterruptPackage(false);
                actor->EvaluatePackage();
            }
        }
    saved.waiting = false;
    saved.waitPackage = 0;
}
void interruptPanel() {
    if (!visible) return;
    const bool interrupted = active;
    checkpointMatch();
    close(!interrupted);
    if (interrupted) RE::SendHUDMessage::ShowHUDMessage("Tessera game interrupted. You can resume it later.");
}
bool checkLiveWorld() {
    if (!visible || !worldFocus.unpaused()) return true;
    auto player = RE::PlayerCharacter::GetSingleton();
    auto actor = opponent.get();
    if (!player || player->IsDead() || player->IsInCombat()) {
        interruptPanel();
        return false;
    }
    if (active) {
        if (campaign::tournamentID && !match.finished()) {
            const auto* event = ttcg::tournament(campaign::saved, campaign::tournamentID);
            if (!event || !ttcg::tournamentOpen(*event, campaign::gameHour())) {
                releaseOpponent();
                campaign::resolveSavedMatch(false);
                active = false;
                close(false);
                RE::SendHUDMessage::ShowHUDMessage("The tournament has ended.");
                return false;
            }
        }
        if (!actor || actor->IsDead() || actor->IsDisabled()) {
            // A completed table already owns its result. Preserve its prize screen;
            // ordinary recovery can settle any remaining stakes when the album opens.
            if (match.finished()) {
                interruptPanel();
                return false;
            }
            releaseOpponent();
            campaign::resolveSavedMatch(false);
            active = false;
            close(false);
            RE::SendHUDMessage::ShowHUDMessage("Your Tessera game has been canceled.");
            return false;
        }
        if (actor->IsInCombat() || actor->IsHostileToActor(player) || actor->GetCurrentScene() ||
            (!campaign::tournamentID && (actor->GetParentCell() != player->GetParentCell() ||
                                         actor->GetPosition().GetDistance(player->GetPosition()) >= 900.f))) {
            interruptPanel();
            return false;
        }
    } else if (screen == "shop" || challengeContext) {
        // Setup and shops have no live table to save. Close if their NPC leaves or
        // becomes unavailable, including while browsing an album from match setup.
        if (!campaign::actorReadyForCards(actor.get()) ||
            (!campaign::tournamentID && (actor->GetParentCell() != player->GetParentCell() ||
                                         actor->GetPosition().GetDistance(player->GetPosition()) >= 900.f))) {
            interruptPanel();
            return false;
        }
    }
    return true;
}
void monitorWorld() {
    const auto generation = ++worldMonitorGeneration;
    auto pendingTick = std::make_shared<std::atomic<bool>>(false);
    std::thread([generation, pendingTick]() {
        while (worldMonitorGeneration.load() == generation) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            if (worldMonitorGeneration.load() != generation) break;
            if (pendingTick->exchange(true)) continue;
            collectionIntegration::queue([generation, pendingTick]() {
                pendingTick->store(false);
                if (worldMonitorGeneration.load() != generation || !visible || !checkLiveWorld()) return;
                if (presentationWait.expired(session, revision,
                                             active ? (settling   ? 1u
                                                       : thinking ? 2u
                                                                  : 0u)
                                                    : 0u,
                                             musicTime())) {
                    log::error(
                        "Match stalled: session {}, revision {}, placed {}, turn {}, settling {}, thinking {}; preserving table and releasing UI",
                        session, revision, match.placed, match.turn, settling, thinking);
                    interruptPanel();
                }
            });
        }
    }).detach();
}
bool updateWorldFocus() {
    const auto result = worldFocus.apply(*api, view, ttcg::shouldPauseWorld(worldPauseSettings, screen, active));
    const auto token = result == ttcg::FocusUpdate::Ready && worldFocus.unpaused() ? epoch.load() + 1 : 0;
    unpausedWorldEpoch.store(token);
    // Paused matches also need recovery if presentation or AI stops responding.
    if (!panelMonitoring) {
        panelMonitoring = true;
        presentationWait.reset();
        monitorWorld();
    }
    watchedOpponent.store(
        token && (active || challengeContext || screen == "shop") && opponent.get() ? opponent.get()->GetFormID() : 0);
    if (result == ttcg::FocusUpdate::Lost) {
        interruptPanel();
        return false;
    }
    if (result == ttcg::FocusUpdate::Releasing) {
        const auto token = epoch.load();
        // The native menu queued its release first. Cross that UI-task boundary before
        // returning to the game thread to acquire the new mode and publish state.
        SKSE::GetTaskInterface()->AddUITask([token]() {
            collectionIntegration::queue([token]() {
                if (epoch.load() != token || !visible || !worldFocus.switching()) return;
                auto player = RE::PlayerCharacter::GetSingleton();
                if (!player || player->IsDead() || player->IsInCombat() || !worldFocus.complete(*api, view)) {
                    interruptPanel();
                    return;
                }
                publish();
                runAI();
            });
        });
    }
    if (result == ttcg::FocusUpdate::Ready) {
        waitOpponent();
        return checkLiveWorld();
    }
    return false;
}
} // namespace tessera_plugin
