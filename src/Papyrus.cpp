// Papyrus entry points and the background update.
#include "PluginState.h"

namespace tessera_plugin {
void logNpcSyncTiming(const ttcg::NpcSyncTiming& timing, std::string_view source) {
    if (!spdlog::should_log(spdlog::level::debug)) return;
    std::string phases, work;
    for (unsigned i = 0; i < timing.phases.size(); ++i) {
        if (i) phases += ", ";
        phases += std::format("{} {:.3f}", ttcg::npcSyncPhaseNames[i], ttcg::NpcSyncTiming::ms(timing.phases[i]));
    }
    for (unsigned i = 0; i < timing.work.size(); ++i) {
        if (i) work += ", ";
        work += std::format("{} {:.3f}/{}", ttcg::npcSyncWorkNames[i], ttcg::NpcSyncTiming::ms(timing.work[i]),
                            timing.calls[i]);
    }
    log::debug("NPC sync profile [{}]: {:.3f} ms; phases: {} ({} opponents, {} clock reads)", source,
               ttcg::NpcSyncTiming::ms(timing.total), phases, ttcg::opponents.size(), timing.clockReads);
    log::debug("NPC sync work [{}]: {} (exclusive ms/calls)", source, work);
}
void backgroundUpdate() {
    if (worldReady() && campaign::available()) {
        // Detailed profiling is opt-in. Keep only coarse slow-update timing during
        // normal play; diagnostic sessions sample the first and every twelfth refresh.
        static std::atomic_uint profileCount{0};
        const bool diagnostics = spdlog::should_log(spdlog::level::debug);
        const bool profile = diagnostics && profileCount.fetch_add(1, std::memory_order_relaxed) % 12 == 0;
        ttcg::NpcSyncTiming syncTiming;
        const auto started = std::chrono::steady_clock::now();
        campaign::registerNearbyNpcs();
        const auto registered = std::chrono::steady_clock::now();
        campaign::syncProgression(profile ? &syncTiming : nullptr);
        const auto refreshed = std::chrono::steady_clock::now();
        campaign::updateTournaments();
        const auto finished = std::chrono::steady_clock::now();
        auto ms = [](auto duration) { return std::chrono::duration<double, std::milli>(duration).count(); };
        if (ms(finished - started) >= 16.0)
            log::warn(
                "Slow background update: {:.1f} ms (residents {:.1f}, progression {:.1f}, tournaments {:.1f}; {} opponents)",
                ms(finished - started), ms(registered - started), ms(refreshed - registered), ms(finished - refreshed),
                ttcg::opponents.size());
        if (profile) logNpcSyncTiming(syncTiming, "background");
        // Successful summaries are diagnostic-only, including their accumulation
        // and mutex. These measure native work, not frame time or task-queue delay.
        if (diagnostics) {
            static std::mutex timingsMutex;
            static std::array<double, 4> totals{};
            static double peak = 0;
            static unsigned samples = 0;
            std::scoped_lock timingLock(timingsMutex);
            const std::array<double, 4> elapsed{ms(finished - started), ms(registered - started),
                                                ms(refreshed - registered), ms(finished - refreshed)};
            for (unsigned i = 0; i < totals.size(); ++i) totals[i] += elapsed[i];
            peak = std::max(peak, elapsed[0]);
            if (++samples == 12) {
                log::debug(
                    "Background update summary: {} samples, average {:.2f} ms, peak {:.2f} ms (residents {:.2f}, progression {:.2f}, tournaments {:.2f}; {} opponents)",
                    samples, totals[0] / samples, peak, totals[1] / samples, totals[2] / samples, totals[3] / samples,
                    ttcg::opponents.size());
                totals = {};
                peak = 0;
                samples = 0;
            }
        }
    }
}
void backgroundUpdatePapyrus(RE::StaticFunctionTag*) {
    // Queue one refresh for the current save.
    static std::atomic_bool queued{false};
    if (queued.exchange(true)) return;
    const auto generation = campaign::npcFactionTracking.generation();
    collectionIntegration::queue([generation]() {
        struct Complete {
            ~Complete() { queued.store(false); }
        } complete;
        if (campaign::npcFactionTracking.current(generation)) backgroundUpdate();
    });
}
namespace legacyPapyrus {
RE::Actor* nextMaster(RE::StaticFunctionTag*) { return nullptr; }
void masterArrived(RE::StaticFunctionTag*, RE::Actor*) {}
void masterResponse(RE::StaticFunctionTag*, RE::Actor*, bool) {}
}
void albumPapyrus(RE::StaticFunctionTag*) {
    collectionIntegration::queue([]() {
        auto player = RE::PlayerCharacter::GetSingleton();
        auto ui = RE::UI::GetSingleton();
        if (visible || pendingAlbum || !campaign::available() || !player || player->IsDead() || player->IsInCombat() ||
            !ui || !uiReady || !api || !api->IsValid(view))
            return;
        if (!ui->IsMenuOpen(RE::BookMenu::MENU_NAME) || RE::BookMenu::GetTargetForm() != campaign::albumForm) return;
        pendingAlbum = true;
        auto queue = RE::UIMessageQueue::GetSingleton();
        if (!queue) {
            pendingAlbum = false;
            return;
        }
        if (auto book = ui->GetMenu<RE::BookMenu>()) {
            auto& data = book->GetRuntimeData();
            if (book->uiMovie) book->uiMovie->SetVisible(false);
            if (data.book) data.book->SetVisible(false);
            if (data.bookModel) data.bookModel->SetAppCulled(true);
            data.closeMenu = true;
        }
        queue->AddMessage(RE::BookMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kForceHide, nullptr);
        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME))
            queue->AddMessage(RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
    });
}
int npcAvailabilityPapyrus(RE::StaticFunctionTag*, RE::Actor* actor) {
    if (!actor) return -1;
    std::scoped_lock lock(campaign::npcCacheMutex);
    const auto it = campaign::npcAvailabilityCache.find(actor->GetFormID());
    return it == campaign::npcAvailabilityCache.end() ? -1 : it->second;
}
bool npcRegisteredPapyrus(RE::StaticFunctionTag* tag, RE::Actor* actor) {
    return npcAvailabilityPapyrus(tag, actor) >= 0;
}
bool npcGrantedPapyrus(RE::StaticFunctionTag*, RE::Actor* actor, RE::BSFixedString grant) {
    if (!actor || grant.empty()) return false;
    std::scoped_lock lock(campaign::npcCacheMutex);
    const auto it = campaign::npcIdentityCache.find(actor->GetFormID());
    return it != campaign::npcIdentityCache.end() &&
           campaign::npcGrantedCache.contains("script:" + std::string(grant.c_str()) + ":" + it->second);
}
void npcAddPapyrus(RE::StaticFunctionTag*, RE::Actor* actor) {
    if (!actor) return;
    const auto handle = actor->GetHandle();
    collectionIntegration::queue([handle]() {
        auto actor = handle.get();
        if (actor) {
            const bool added = campaign::registerNpcActor(actor.get(), true);
            campaign::syncProgression();
            log::info("NPC enrollment {}: {}", actor->GetName(), added);
        }
    });
}
void npcRefreshPapyrus(RE::StaticFunctionTag*, RE::Actor* actor) {
    if (!actor) return;
    const auto handle = actor->GetHandle();
    collectionIntegration::queue([handle]() {
        auto actor = handle.get();
        if (actor) {
            campaign::registerNpcActor(actor.get());
            campaign::syncProgression();
        }
    });
}
void npcEnabledPapyrus(RE::StaticFunctionTag*, RE::Actor* actor, bool enabled) {
    if (!actor) return;
    const auto handle = actor->GetHandle();
    collectionIntegration::queue([handle, enabled]() {
        auto actor = handle.get();
        if (!actor) return;
        campaign::registerNpcActor(actor.get());
        const auto base = campaign::persistentID(actor->GetActorBase());
        if (auto it = campaign::saved.residents.profiles.find(base); it != campaign::saved.residents.profiles.end())
            it->second.enabled = enabled;
        campaign::syncProgression();
    });
}
void npcGivePapyrus(RE::StaticFunctionTag*, RE::Actor* actor, RE::BSFixedString card, RE::BSFixedString grant,
                    int quantity, bool foil) {
    if (!actor || card.empty() || grant.empty()) return;
    const auto handle = actor->GetHandle();
    const std::string key = card.c_str(), token = grant.c_str();
    collectionIntegration::queueCurrent([handle, key, token, quantity, foil]() {
        auto actor = handle.get();
        if (!actor || !ttcg::validExpansionKey(key) || !ttcg::validExpansionKey(token) || quantity < 1 ||
            quantity > 99 ||
            (campaign::profileIndex(actor.get()) < 0 && !campaign::registerNpcActor(actor.get(), true))) {
            log::warn("NPC card grant rejected: {}", token);
            return;
        }
        campaign::seedOpponent(actor.get());
        const auto base = campaign::persistentID(actor->GetActorBase());
        const bool given =
            ttcg::grantNpcCards(campaign::saved, campaign::ledgerKey(actor.get()),
                                "script:" + token + ":" + ttcg::worldKey(base), {{key, unsigned(quantity), foil}});
        campaign::syncProgression();
        log::info("NPC card grant {}: {}", token, given ? "delivered" : "already delivered or unavailable");
    });
}
void beginPapyrus(RE::StaticFunctionTag*, RE::Actor* actor) { queuePanelRequest(actor, "lobby"); }
void receiveCommand(const char* raw) {
    if (!raw) return;
    std::string text(raw);
    if (!validCommandSize(text)) return;
    // Native render callbacks run on the render thread. Game state and audio belong
    // to the game task queue; validate session/revision when the task executes.
    collectionIntegration::queue([text = std::move(text)]() { command(text.c_str()); });
}
RE::TESObjectBOOK* nextTournamentInvitationPapyrus(RE::StaticFunctionTag*) {
    const auto request = campaign::tournamentLetterReady.exchange(0);
    if (!request) return nullptr;
    campaign::tournamentLetterSentAt = campaign::tournamentCourierClock();
    campaign::tournamentLetterInFlight = request;
    log::info("Tournament {} invitation handed to courier script", unsigned(request >> 32));
    return RE::TESForm::LookupByID<RE::TESObjectBOOK>(unsigned(request));
}
void tournamentInvitationPapyrus(RE::StaticFunctionTag*, RE::TESObjectBOOK* letter, bool delivered) {
    if (!letter) return;
    const auto form = letter->GetFormID();
    const auto request = campaign::tournamentLetterInFlight.load();
    collectionIntegration::queueCurrent([form, delivered, request]() {
        if (!delivered && request && unsigned(request) == form) campaign::acknowledgeTournamentCourier(request);
        campaign::updateTournaments();
    });
}
void tournamentEntryPapyrus(RE::StaticFunctionTag*, RE::Actor* actor) { queuePanelRequest(actor, "registration"); }
void tournamentPapyrus(RE::StaticFunctionTag*, RE::Actor* actor) { queuePanelRequest(actor, "tournaments"); }
void shopPapyrus(RE::StaticFunctionTag*, RE::Actor* actor) { queuePanelRequest(actor, "shop"); }
bool expansionLoadedPapyrus(RE::StaticFunctionTag*, RE::BSFixedString key) {
    return ttcg::expansionCardID(key.c_str()) != 0;
}
void expansionGivePapyrus(RE::StaticFunctionTag*, RE::BSFixedString key, std::int32_t quantity, bool foil) {
    const std::string identity(key.c_str());
    collectionIntegration::queueCurrent([identity, quantity, foil]() {
        const bool granted = campaign::available() && RE::PlayerCharacter::GetSingleton() &&
                             ttcg::grantExpansionCard(campaign::saved, identity, quantity, foil);
        if (granted) {
            log::info("Expansion reward: {} x{}, foil {}", identity, quantity, foil);
            ++revision;
            publish();
        } else
            log::warn(
                "Expansion reward rejected: {} x{}, foil {} (unavailable card, duplicate unique, or invalid quantity/state)",
                identity, quantity, foil);
    });
}
void buyAlbumPapyrus(RE::StaticFunctionTag*, RE::Actor* actor) {
    if (!actor) return;
    const auto handle = actor->GetHandle();
    collectionIntegration::queue([handle]() {
        auto merchant = handle.get();
        if (!merchant) return;
        const bool bought = campaign::buyAlbum(merchant.get());
        const char* message =
            bought                 ? "Your Tessera Album includes eight starter cards."
            : campaign::hasAlbum() ? "You already own a Tessera Album."
            : campaign::physicalCount(RE::PlayerCharacter::GetSingleton(), campaign::goldForm) < ttcg::albumPrice
                ? "You need 100 gold to buy a Tessera Album."
                : "The album purchase could not be completed.";
        RE::SendHUDMessage::ShowHUDMessage(message);
        if (spdlog::should_log(spdlog::level::debug))
            log::debug("Album purchase callback: {} ({:08X}), eligible {}, success {}", merchant->GetName(),
                       merchant->GetFormID(), vendors::albumEligible(merchant.get()), bought);
    });
}
} // namespace tessera_plugin
