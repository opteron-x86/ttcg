// Interface command dispatch and persisted interface settings.
#include "PluginState.h"

namespace tessera_plugin {
bool persistInterfaceSetting(const char* section, const char* key, int value) {
    const std::string a(section), b(key);
    const std::wstring wideSection(a.begin(), a.end()), wideKey(b.begin(), b.end());
    return WritePrivateProfileStringW(wideSection.c_str(), wideKey.c_str(), std::to_wstring(value).c_str(),
                                      settingsPath.c_str()) != 0;
}
bool setGamesEnabled(bool enabled) {
    if (!campaign::interfaceSettings.set("games", enabled, persistInterfaceSetting)) {
        interfaceNotice = "Couldn't save this setting.";
        if (visible)
            publish();
        else
            RE::SendHUDMessage::ShowHUDMessage(interfaceNotice.c_str());
        return false;
    }
    interfaceNotice.clear();
    campaign::syncGameAvailability();
    if (!enabled) {
        cancelPanelRequest();
        if (active) {
            close(false);
            RE::SendHUDMessage::ShowHUDMessage("Tessera disabled. Your game is saved to resume later.");
            return true;
        }
        if (visible && (screen != "album" || challengeContext)) {
            campaign::closeShop();
            opponent = {};
            opponentName.clear();
            challengeContext = false;
            campaign::tournamentID = 0;
            campaign::tournamentRegistration = 0;
            screen = "album";
            albumSection.clear();
            campaign::prepare(nullptr, true);
            ++epoch;
            ++session;
            revision = 0;
            updateMusic();
        }
    }
    if (visible)
        publish();
    else
        RE::SendHUDMessage::ShowHUDMessage(enabled ? "Tessera enabled." : "Tessera disabled.");
    return true;
}
#include "NativeTournamentCommands.h"
bool validCommandSize(std::string_view text) { return text.size() <= (text.starts_with("cardback ") ? 2048u : 128u); }
void command(const char* raw) {
    if (!raw || !visible) return;
    std::string text(raw);
    if (!validCommandSize(text)) return;
    std::istringstream input(text);
    std::string verb, extra;
    std::uint64_t s = 0, r = 0;
    if (!(input >> verb >> s >> r) || s != session || r != revision) return;
    if (worldFocus.switching()) return;
    if (!checkLiveWorld()) return;
    if (verb == "interrupted") {
        std::string action;
        if (active || screen != "album" || !(input >> action) || (input >> extra) || !campaign::savedMatch.present)
            return;
        if (action == "resume") {
            if (!resumeMatch()) {
                if (campaign::savedMatch.present)
                    campaign::notice = campaign::savedMatch.tournament
                                           ? "Return to the tournament inn to resume."
                                           : "Return to " + campaign::savedMatchName() + " to resume.";
                ++revision;
                publish();
            }
            return;
        }
        if (action == "forfeit" && !campaign::savedMatch.table.finished()) {
            campaign::resolveSavedMatch(true);
            campaign::prepare(nullptr, true);
            ++session;
            revision = 0;
            publish();
            return;
        }
        return;
    }
    // Checkpoint a match when toggling games off.
    if (verb == "settings") {
        const auto position = input.tellg();
        std::string key;
        int value = -1;
        if (!(input >> key >> value) || (input >> extra) || value < 0 || !bindingCapture.empty()) return;
        if (key == "games") {
            if (value <= 1) setGamesEnabled(value != 0);
            return;
        }
        if (key == "residents" || key == "vanilla-voices") {
            if (active || thinking || settling) return;
            interfaceNotice = campaign::interfaceSettings.set(key, value, persistInterfaceSetting)
                                  ? ""
                                  : "Couldn't save this setting.";
            campaign::registerNearbyNpcs();
            campaign::syncProgression();
            publish();
            return;
        }
        input.clear();
        input.seekg(position);
    }
    if (!campaign::interfaceSettings.gamesEnabled &&
        (verb == "start" || verb == "play" || verb == "prepare" || verb == "rule-lesson" || verb == "shop"))
        return;
    if (!ttcg::tournamentRewardAllowsCommand(campaign::saved, verb)) {
        publish();
        return;
    }
    if (screen == "shop" && verb != "shop" && verb != "close" && verb != "binding" && verb != "development" &&
        verb != "cardback" && verb != "settings")
        return;
    if (verb == "collection-recovery") {
        std::string action, confirmation;
        if (!(input >> action >> confirmation) || (input >> extra) || confirmation != "confirm" ||
            (action != "clear" && action != "remove-missing" && action != "reset"))
            return;
        if (!campaign::available() || active || thinking || settling || screen != "album" || !bindingCapture.empty())
            return;
        if (action != "remove-missing" && !campaign::development.enabled) return;
        const auto blocked = action == "reset"
                                 ? campaign::resetBlocked()
                                 : ttcg::collectionRecoveryBlocked(campaign::saved, campaign::savedMatch.present);
        if (!blocked.empty()) {
            campaign::notice = blocked;
            publish();
            return;
        }
        bool itemsRemoved = true;
        if (action == "clear" || action == "reset") {
            const auto persist = [](std::string_view key, int value) {
                const std::wstring wide(key.begin(), key.end());
                return WritePrivateProfileStringW(L"Development", wide.c_str(), std::to_wstring(value).c_str(),
                                                  settingsPath.c_str()) != 0;
            };
            std::vector<unsigned> cancelled;
            if (action == "reset")
                for (const auto& [owner, offer] : campaign::saved.exchanges)
                    if (offer.active()) cancelled.push_back(owner);
            const bool changed = action == "reset" ? campaign::development.resetTessera(campaign::saved, persist)
                                                   : campaign::development.clearCollection(campaign::saved, persist);
            if (!changed) {
                campaign::notice =
                    "Couldn't save the development settings. Your Tessera progress has not been changed.";
                publish();
                return;
            }
            if (action == "reset") {
                collectionIntegration::reset();
                releaseOpponent();
                cancelPanelRequest();
                campaign::closeShop();
                auto fresh = std::move(campaign::saved);
                campaign::revert(nullptr);
                campaign::saved = std::move(fresh);
                opponent = {};
                opponentName.clear();
                challengeContext = false;
                albumSection.clear();
                lastResult = {};
                ++epoch;
                ++session;
                revision = 0;
                presentationWait.reset();
                updateMusic();
                itemsRemoved = campaign::removeResetItems();
                campaign::ensureAlbum();
                campaign::npcFactionTracking.allowDiscovery();
                campaign::registerNearbyNpcs();
                collectionIntegration::activate();
                for (auto owner : cancelled)
                    if (auto* quest = campaign::resolveID<RE::TESQuest>(owner))
                        exchangeIntegration::notify(quest, "cancelled");
            }
            developmentNotice.clear();
        } else if (!ttcg::removeUnavailableCards(campaign::saved))
            return;
        campaign::prepare(opponent.get().get(), action == "reset");
        campaign::syncProgression();
        campaign::notice = action == "reset"
                               ? (itemsRemoved ? "Tessera reset. Your album has eight starter cards and a starter deck."
                                               : "Tessera reset. Some packs or old invitations could not be removed.")
                           : action == "clear" ? "Collection cleared. Collect new cards from packs, shops and games."
                                               : "Unavailable cards removed from your collection and decks.";
        log::info("Collection recovery: {}", action);
        match = ttcg::Match(1, campaign::sessionRules);
        ++revision;
        publish();
        return;
    }
    if (verb == "settings") {
        std::string key;
        int value = -1;
        if (active || thinking || settling || screen == "lesson" || !bindingCapture.empty() ||
            !(input >> key >> value) || (input >> extra))
            return;
        const auto option = std::find_if(pauseOptions.begin(), pauseOptions.end(),
                                         [&](const auto& entry) { return entry.command == key; });
        if (key == "rank-finish-reset" && value == 0) {
            interfaceNotice = campaign::interfaceSettings.resetRankFinish(persistInterfaceSetting)
                                  ? ""
                                  : "Couldn't save every setting.";
            publish();
            return;
        }
        if (key == "match-appearance-reset" && value == 0) {
            interfaceNotice = campaign::interfaceSettings.resetMatchAppearance(persistInterfaceSetting)
                                  ? ""
                                  : "Couldn't save every setting.";
            publish();
            return;
        }
        if (ttcg::InterfaceSettings::find(key)) {
            interfaceNotice = campaign::interfaceSettings.set(key, value, persistInterfaceSetting)
                                  ? ""
                                  : "Couldn't save this setting.";
            publish();
            return;
        }
        if (option == pauseOptions.end() || (value != 0 && value != 1)) return;
        if (!WritePrivateProfileStringW(L"Interface", option->iniKey, value ? L"1" : L"0", settingsPath.c_str())) {
            interfaceNotice = "Couldn't save this setting.";
            publish();
            return;
        }
        worldPauseSettings.*option->member = value != 0;
        interfaceNotice.clear();
        publish();
        return;
    }
    if (verb == "npc-add") {
        if (active || thinking || settling || campaign::saved.contract.pending() || (input >> extra)) return;
        auto actor = enrollmentTarget.get();
        const bool added = actor && eligible(actor.get()) && campaign::canEnrollNpc(actor.get()) &&
                           campaign::registerNpcActor(actor.get(), true);
        interfaceNotice =
            added ? std::string(actor->GetName()) + " now plays Tessera." : campaign::npcInspector(actor.get());
        campaign::syncProgression();
        publish();
        return;
    }
    if (verb == "shop") {
        if (screen != "shop" || active || thinking || settling) return;
        auto actor = opponent.get();
        if (!eligible(actor.get()) || !campaign::shopAvailable(actor.get())) {
            close();
            return;
        }
        if (campaign::shopCommand(actor.get(), input)) {
            ++revision;
            publish();
        }
        return;
    }
    if (verb == "tournament") {
        tournamentCommand(input);
        return;
    }
    if (verb == "cardback") {
        if (active || thinking || settling || screen == "lesson" || !bindingCapture.empty()) return;
        if (!ttcg::cardBackCommand(input, cardBackNotice, [](std::string_view id) {
                const auto value = ttcg::cardBackSetting(id);
                const std::wstring wide(value.begin(), value.end());
                return WritePrivateProfileStringW(L"Appearance", L"CardBack", wide.c_str(), settingsPath.c_str()) != 0;
            }))
            return;
        publish();
        return;
    }
    if (verb == "development") {
        if (!campaign::development.enabled || !campaign::available() || active || thinking || settling ||
            screen == "lesson" || !bindingCapture.empty() || campaign::savedMatch.present ||
            campaign::saved.contract.pending() || campaign::saved.goldCredit)
            return;
        std::string action, key;
        int value = 0;
        if (!(input >> action)) return;
        bool resetTerms = false;
        if (action == "set") {
            if (!(input >> key >> value) || (input >> extra)) return;
            auto proposed = campaign::development;
            if (!proposed.set(key, value)) return;
            if (!campaign::development.setPersisted(key, value, [](std::string_view name, int number) {
                    const std::wstring wide(name.begin(), name.end());
                    return WritePrivateProfileStringW(L"Development", wide.c_str(), std::to_wstring(number).c_str(),
                                                      settingsPath.c_str()) != 0;
                })) {
                developmentNotice = "Couldn't save this setting.";
                publish();
                return;
            }
            resetTerms = key == "Rules" || key == "TradeRule" || key == "Wager";
            developmentNotice.clear();
            log::info("Development setting: {} = {}", key, value);
        } else if (action == "cards" || action == "foils") {
            if (input >> extra) return;
            const auto added = campaign::development.addMissingCards(campaign::saved, action == "foils");
            if (added < 0) return;
            developmentNotice =
                added ? std::format("Added {} {} to your album.", added, action == "foils" ? "foils" : "cards")
                : action == "foils" ? "Your album already has every foil."
                                    : "Your album already has every card.";
            campaign::ensureAlbum();
            log::info("Development: {}", developmentNotice);
        } else
            return;
        campaign::prepare(screen == "shop" ? nullptr : opponent.get().get(), resetTerms);
        campaign::syncProgression();
        match = ttcg::Match(1, campaign::sessionRules);
        ++revision;
        publish();
        return;
    }
    if (verb == "binding") {
        if (active || settling || thinking) return;
        std::string action, field;
        unsigned value = 0;
        if (!(input >> action)) return;
        if (action == "cancel") {
            if (input >> extra) return;
            bindingCapture.clear();
            bindingNotice.clear();
            publish();
            return;
        }
        if (!(input >> field)) return;
        const auto setting = std::find_if(hotkeys.fields.begin(), hotkeys.fields.end(),
                                          [&](const auto& entry) { return entry.name == field; });
        if (setting == hotkeys.fields.end()) return;
        if (action == "begin") {
            if (input >> extra) return;
            bindingCapture = field;
            bindingNotice.clear();
            publish();
            return;
        }
        if (action == "set") {
            if (!(input >> value) || bindingCapture != field) return;
        } else if (action == "unbind")
            value = 0;
        else if (action == "reset")
            value = setting->defaultKey;
        else
            return;
        if (input >> extra) return;
        if (!ttcg::validHotkey(value)) {
            bindingNotice = "Choose a different key.";
            publish();
            return;
        }
        if (const auto conflict = hotkeys.conflict(field, value); !conflict.empty()) {
            bindingNotice = "Already used for " + std::string(conflict) + ".";
            publish();
            return;
        }
        if (!WritePrivateProfileStringW(L"Input", setting->ini, std::to_wstring(value).c_str(), settingsPath.c_str())) {
            bindingNotice = "Couldn't save the key.";
            publish();
            return;
        }
        hotkeys.*setting->member = value;
        bindingCapture.clear();
        bindingNotice.clear();
        publish();
        return;
    }
    if (verb == "close") {
        if (input >> extra) return;
        if ((active && campaign::saved.contract.pending()) || (campaign::tournamentID && active && !match.finished())) {
            publish();
            return;
        }
        close();
        return;
    }
    if (verb == "rule-lesson") {
        if ((input >> extra) || active || thinking || settling || screen != "lobby" ||
            !campaign::development.ruleLesson)
            return;
        constexpr unsigned types[] = {0, ttcg::ThreeOpen, ttcg::Legion, ttcg::Decimation, ttcg::Affinity};
        ttcg::abandonRuleCulture(campaign::saved, campaign::gameHour());
        campaign::sessionRules = campaign::terms(opponent.get().get()).rules;
        lesson = ttcg::Lesson::workshop(types[campaign::development.ruleLesson]);
        playedRuleSounds = 0;
        screen = "lesson";
        ++session;
        revision = 0;
        publish();
        return;
    }
    if (verb == "lesson") {
        std::string action;
        if (!(input >> action) || active || settling || screen != "lesson") return;
        auto actor = opponent.get();
        if (!eligible(actor.get())) return;
        if (action == "place") {
            int h = -1, square = -1;
            if (!(input >> h >> square) || (input >> extra) || lesson.match.turn != 0 || !lesson.advance(h, square))
                return;

            ++revision;
            publish();
            return;
        }
        if (input >> extra) return;
        if (action == "next") {
            if (lesson.match.turn != 1 || !lesson.advance()) return;
            ++revision;
            publish();
            return;
        }
        if (action == "replay") {
            lesson = lesson.riftTour       ? ttcg::Lesson::riftLesson()
                     : lesson.affinityTour ? ttcg::Lesson::affinityLesson()
                     : lesson.workshopRule ? ttcg::Lesson::workshop(lesson.workshopRule)
                                           : ttcg::Lesson(lesson.sameLesson, lesson.plusLesson);
            playedRuleSounds = 0;
            ++session;
            revision = 0;
            publish();
            return;
        }
        if (action == "play") {
            if ((lesson.sameLesson || lesson.workshopRule) && !campaign::canChallenge(actor.get())) {
                close();
                return;
            }
            if (!campaign::prepare(actor.get())) return;
            screen = "lobby";
            challengeContext = true;
            match = ttcg::Match(1, campaign::sessionRules);
            ++session;
            revision = 0;
            updateMusic();
            publish();
            return;
        }
        return;
    }
    if (verb == "culture") {
        std::string action;
        unsigned value = 0;
        if (!(input >> action >> value) || (input >> extra) || active || thinking || settling || screen != "lobby")
            return;
        auto actor = opponent.get();
        const auto i = campaign::profileIndex(actor.get());
        if (!eligible(actor.get()) || !campaign::canChallenge(actor.get()) || !campaign::cultureEnabled(actor.get()))
            return;
        bool changed = false;
        if (action == "trial" && value <= 1)
            changed = ttcg::answerSpread(campaign::saved, i, campaign::gameHour(), value != 0);
        else if (action == "abolish")
            changed = ttcg::challengeAbolition(campaign::saved, i, campaign::gameHour(), value);
        if (!changed) {
            ++revision;
            publish();
            return;
        }
        const auto& attempt = campaign::saved.culture.pending;
        campaign::sessionRules = attempt.kind ? attempt.rules : campaign::terms(actor.get()).rules;
        if (!campaign::prepare(actor.get())) {
            ++revision;
            publish();
            return;
        }
        match = ttcg::Match(1, campaign::sessionRules);
        ++session;
        revision = 0;
        publish();
        return;
    }
    if (verb == "terms") {
        std::string action;
        unsigned value = 0;
        if (!(input >> action >> value) || (input >> extra) || active || settling || screen != "lobby") return;
        auto actor = opponent.get();
        if (!eligible(actor.get()) || !campaign::canChallenge(actor.get())) return;
        if (action == "rule" && campaign::isChild(actor.get()) && campaign::ruleRequest && !campaign::requestAnswered &&
            (value == 0 || value == campaign::ruleRequest)) {
            campaign::sessionRules = campaign::terms(actor.get()).rules | value;
            campaign::requestAnswered = true;
        } else
            return;
        if (!campaign::prepare(actor.get())) {
            ++revision;
            publish();
            return;
        }
        match = ttcg::Match(1, campaign::sessionRules);
        ++session;
        revision = 0;
        publish();
        return;
    }
    if (verb == "browse") {
        std::string target;
        if (!(input >> target) || (input >> extra) || active || campaign::saved.contract.pending() ||
            (target != "album" && target != "lobby"))
            return;
        auto actor = opponent.get();
        if (target != "album" && !campaign::tournamentID && !eligible(actor.get())) return;
        if (!campaign::prepare(actor.get())) {
            publish();
            return;
        }
        if (target == "lobby" && !campaign::canChallenge(actor.get())) return;
        match = ttcg::Match(1, campaign::sessionRules);
        screen = target;
        if (target == "lobby") challengeContext = true;
        ++session;
        revision = 0;
        updateMusic();
        publish();
        return;
    }
    if (verb == "open-pack") {
        if ((input >> extra) || active || campaign::savedMatch.present || screen != "album") return;
        const auto seed = static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        campaign::notice = campaign::openPack(seed) ? "" : "Pack unavailable.";
        ++revision;
        publish();
        return;
    }
    if (verb == "forfeit") {
        if ((input >> extra) || !active || match.finished()) return;
        campaign::savedMatch = {};
        releaseOpponent();
        if (campaign::tournamentID) {
            ttcg::withdrawTournament(campaign::saved, campaign::tournamentID);
            tournamentBoard();
            return;
        }
        if (!campaign::saved.contract.held) {
            close();
            return;
        }
        auto& contract = campaign::saved.contract;
        auto actor = opponent.get();
        campaign::recordGame(actor.get(), 1, false);
        contract.forfeit();
        campaign::finish(1);
        if (!contract.pending()) {
            RE::SendHUDMessage::ShowHUDMessage(campaign::receipt.c_str());
            close();
        } else
            publish();
        return;
    }
    if (verb == "claim" || verb == "claim-many") {
        unsigned selection = 0;
        auto& contract = campaign::saved.contract;
        if (!(input >> selection) || (input >> extra) || settling || !active || !match.finished() ||
            !contract.choicePending())
            return;
        if (verb == "claim") {
            if (contract.rule() != ttcg::TradeOne || selection > 4) return;
            selection = 1u << selection;
        }
        if ((selection & ~31u) || std::popcount(selection) != static_cast<int>(contract.required(0))) return;
        campaign::finish(0, selection);
        publish();
        return;
    }
    if (verb == "payout") {
        auto& contract = campaign::saved.contract;
        if ((input >> extra) || settling || !active || !match.finished() || !contract.paying) return;
        campaign::recover();
        publish();
        return;
    }
    if (verb == "prepare") {
        if (input >> extra) return;
        log::info("Rematch requested: session {}, revision {}, settling {}, thinking {}, pending {}", session, revision,
                  settling, thinking, campaign::saved.contract.pending());
        if (settling || thinking || (active && !match.finished()) || campaign::saved.contract.pending()) {
            campaign::notice = "The previous game is still finishing.";
            publish();
            return;
        }
        auto actor = opponent.get();
        if (!eligible(actor.get())) {
            close();
            return;
        }
        releaseOpponent();
        campaign::savedMatch = {};
        if (!campaign::prepare(actor.get(), true)) {
            log::warn("Rematch preparation failed: {}", campaign::notice);
            publish();
            return;
        }
        ++epoch;
        ++session;
        revision = 0;
        active = false;
        lastResult = {};
        match = ttcg::Match(1, campaign::sessionRules);
        log::info("Rematch prepared: session {}, opponent {}", session, opponentName);
        updateMusic();
        publish();
        return;
    }
    if (verb == "deck-manage") {
        if (active || settling || thinking || !campaign::available() || campaign::savedMatch.present ||
            campaign::saved.contract.pending() || campaign::saved.goldCredit ||
            (screen != "album" && screen != "lobby"))
            return;
        std::string action, name;
        unsigned id = 0;
        if (!(input >> action >> id)) return;
        if (action == "new" || action == "copy" || action == "rename")
            std::getline(input >> std::ws, name);
        else if (input >> extra)
            return;
        if (ttcg::manageDeck(campaign::saved, action, id, name)) {
            ++session;
            revision = 0;
            publish();
        }
        return;
    }
    if (verb == "deck") {
        if (active || settling || campaign::savedMatch.present || campaign::saved.contract.pending() ||
            campaign::saved.goldCredit || (screen != "album" && screen != "lobby"))
            return;
        ttcg::Hand deck{};
        for (auto& id : deck) {
            int index = -1;
            if (!(input >> index) || index < -1 || index >= static_cast<int>(ttcg::cards.size() * 2)) return;
            id = index < 0 ? 0 : ttcg::displayCardID(index);
        }
        if (input >> extra) return;
        auto stock = campaign::inventory(RE::PlayerCharacter::GetSingleton());
        if (ttcg::validDeck(deck, stock)) {
            campaign::saved.deck = deck;
            ttcg::saveActiveDeck(campaign::saved);
            publish();
        }
        return;
    }
    if (verb == "opening") {
        if ((input >> extra) || !active || !settling || match.placed != 0 || playedOpeningSound) return;
        playedOpeningSound = true;
        const RE::FormID localID = match.turn == 0 ? 0x86A : 0x869;
        auto descriptor =
            RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSSoundDescriptorForm>(localID, "Tessera TCG.esp");
        auto audio = RE::BSAudioManager::GetSingleton();
        const bool started = descriptor && descriptor->soundDescriptor && audio && audio->Play(descriptor);
        if (started)
            log::debug("Opening sound {:06X}: playback submitted", localID);
        else
            log::warn("Opening sound {:06X}: playback failed", localID);
        return;
    }
    if (verb == "result") {
        if ((input >> extra) || !active || !settling || !match.finished() || playedResultSound) return;
        playedResultSound = true;
        const auto score = match.score();
        const RE::FormID localID = score[0] == score[1] ? 0x815 : score[0] > score[1] ? 0x817 : 0x816;
        auto descriptor =
            RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSSoundDescriptorForm>(localID, "Tessera TCG.esp");
        auto audio = RE::BSAudioManager::GetSingleton();
        const bool started = descriptor && descriptor->soundDescriptor && audio && audio->Play(descriptor);
        if (started)
            log::debug("Result sound {:06X}: playback submitted", localID);
        else
            log::warn("Result sound {:06X}: playback failed", localID);
        return;
    }
    if (verb == "rule") {
        std::string rule;
        if (!(input >> rule) || (input >> extra) || (!settling && screen != "lesson")) return;
        const unsigned bit = rule == "same" || rule == "samewall" ? 1u
                             : rule == "plus"                     ? 2u
                             : rule == "combo"                    ? 8u
                             : rule == "legion"                   ? ttcg::Legion
                             : rule == "decimation"               ? ttcg::Decimation
                                                                  : 0u;
        if (!bit || (playedRuleSounds & bit)) return;
        const bool triggered = std::any_of((screen == "lesson" ? lesson.last : lastResult).stages.begin(),
                                           (screen == "lesson" ? lesson.last : lastResult).stages.end(),
                                           [bit](auto& stage) { return (stage.rule & bit) != 0; });
        if (!triggered) return;
        playedRuleSounds |= bit;
        playRuleSound(bit);
        return;
    }
    if (verb == "music") {
        int enabled = -1;
        if (!(input >> enabled) || (input >> extra) || (enabled != 0 && enabled != 1)) return;
        musicEnabled = enabled != 0;
        if (musicEnabled && !musicPlaying) scanMusicFolder();
        if (!WritePrivateProfileStringW(L"Audio", L"Enabled", musicEnabled ? L"1" : L"0", settingsPath.c_str()))
            log::warn("Could not save music preference");
        updateMusic();
        publish();
        return;
    }
    if (verb == "settled") {
        if ((input >> extra) || !settling) return;
        log::debug("Presentation acknowledged: session {}, revision {}, placed {}, turn {}", session, revision,
                   match.placed, match.turn);
        settling = false;
        if (match.redeal()) {
            ++revision;
            lastResult = {};
            settling = true;
            playedOpeningSound = false;
            playedRuleSounds = 0;
            checkpointMatch();
            publish();
            return;
        }
        auto& contract = campaign::saved.contract;
        if (match.finished() && contract.held && !contract.choicePending()) {
            const int winner = contract.outcome;
            campaign::finish(winner);
        }
        publish();
        runAI();
        return;
    }
    if (verb == "start") {
        unsigned rules = 0;
        int tradeRule = -1, wager = -1;
        if (!(input >> rules >> tradeRule >> wager) || !ttcg::validRules(rules) || (tradeRule < 0 || tradeRule > 4) ||
            (wager < 0 || wager > ttcg::maximumWager || wager % 5) || thinking || settling || active ||
            screen != "lobby" || campaign::savedMatch.present || campaign::saved.contract.pending() ||
            campaign::saved.goldCredit)
            return;
        ttcg::Hand deck{};
        for (auto& id : deck) {
            int index = -1;
            if (!(input >> index) || index < 0 || index >= static_cast<int>(ttcg::cards.size() * 2)) return;
            id = ttcg::displayCardID(index);
        }
        if (input >> extra) return;
        auto actor = opponent.get();
        if (!campaign::tournamentID && !eligible(actor.get())) {
            close();
            return;
        }
        if (!campaign::canChallenge(actor.get()) ||
            (!campaign::tournamentID && !campaign::canStake && !campaign::practice))
            return;
        const int expected = !campaign::practice && campaign::canStake ? campaign::terms(actor.get()).trade : 0;
        const int fixed = campaign::fixedWager();
        if ((campaign::cultureEnabled(actor.get()) &&
             ttcg::spreadOffer(campaign::saved, campaign::profileIndex(actor.get()), campaign::gameHour())) ||
            rules != campaign::sessionRules || tradeRule != expected || wager > campaign::maxWager(actor.get()) ||
            (fixed >= 0 && wager != fixed) || (campaign::ruleRequest && !campaign::requestAnswered))
            return;
        auto stock = campaign::inventory(RE::PlayerCharacter::GetSingleton());
        if (ttcg::playableCount(stock) < 5) {
            campaign::notice = "You need five cards.";
            publish();
            return;
        }
        if (!ttcg::validHand(deck, stock)) {
            campaign::notice = "Choose five cards you own.";
            publish();
            return;
        }
        if (campaign::saved.culture.pending.kind &&
            (!campaign::cultureEnabled(actor.get()) ||
             !ttcg::canStartCultureMatch(campaign::saved, campaign::profileIndex(actor.get()), rules)))
            return;
        if (campaign::tournamentID) {
            const auto* e = ttcg::tournament(campaign::saved, campaign::tournamentID);
            if (!e || campaign::tournamentVenue() != e->hold ||
                !ttcg::startTournamentRound(campaign::saved, campaign::tournamentID, campaign::gameHour())) {
                campaign::notice = "This tournament round is unavailable.";
                ++revision;
                publish();
                return;
            }
        }
        campaign::Bank bank(campaign::persistentID(actor.get()));
        if (tradeRule || wager) {
            if (!campaign::canStake || !ttcg::validHand(campaign::rivalDeck, campaign::inventory(actor.get()))) {
                campaign::notice = "They don't have enough cards for a game.";
                publish();
                return;
            }
            if (!campaign::saved.contract.reserve(bank, {deck, campaign::rivalDeck}, static_cast<unsigned>(tradeRule),
                                                  wager, campaign::persistentID(actor.get()))) {
                log::warn(
                    "Match stakes unavailable: {} ({:08X}), actor resolved {}, ledger {:08X}, trade {}, wager {}, gold {}/{}, cards valid {}/{}, transfer pending {}",
                    actor->GetName(), actor->GetFormID(), bank.actors[1] != nullptr, bank.rival, tradeRule, wager,
                    bank.count(0, 0), bank.count(1, 0),
                    ttcg::validHand(deck, campaign::inventory(RE::PlayerCharacter::GetSingleton())),
                    ttcg::validHand(campaign::rivalDeck, campaign::inventory(actor.get())),
                    campaign::saved.contract.pending());
                campaign::notice = "Cards or gold unavailable.";
                publish();
                return;
            }
        } else
            campaign::saved.contract = {};
        campaign::cultureGame = campaign::cultureEnabled(actor.get()) && (tradeRule || wager);
        if (campaign::cultureGame) ttcg::startCultureMatch(campaign::saved, campaign::profileIndex(actor.get()), rules);
        campaign::competitiveMatch = !campaign::practice;
        campaign::saved.deck = deck;
        ttcg::saveActiveDeck(campaign::saved);
        campaign::notice.clear();
        campaign::receipt.clear();
        ++epoch;
        ++session;
        revision = 0;
        const auto* event = ttcg::tournament(campaign::saved, campaign::tournamentID);
        const auto seed =
            event ? ttcg::tournamentHash(
                        event->seed ^ event->matchNode ^ event->replays ^
                        ((event->wins[event->matchNode - 8][0] + event->wins[event->matchNode - 8][1]) * 2654435761u))
                  : static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        match = ttcg::Match(ttcg::indices(deck), ttcg::indices(campaign::rivalDeck), seed, rules);
        active = true;
        for (int p = 0; p < 2; ++p)
            for (int h = 0; h < 5; ++h) match.foilHands[p][h] = ttcg::isFoil(p == 0 ? deck[h] : campaign::rivalDeck[h]);
        ttcg::retainDealVisibility(campaign::saved, match);
        lastResult = {};
        settling = true;
        playedResultSound = false;
        playedOpeningSound = false;
        checkpointMatch();
        log::info("Match {} seed {} rules {} trade {} wager {}", session, seed, rules, tradeRule, wager);
        updateMusic();
        publish();
        runAI();
        return;
    }

    if (verb == "play") {
        int hand = -1, square = -1, group = -1;
        if (!(input >> hand >> square) || !active || thinking || settling || match.turn != 0) return;
        input >> std::ws;
        if (input.peek() != std::char_traits<char>::eof()) {
            if (!(input >> group) || (input >> extra)) return;
        }
        auto result = match.play({hand, square, group});
        if (!result.legal) return;
        recordResult(result);
        publish();
    }
}
} // namespace tessera_plugin
