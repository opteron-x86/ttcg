// Match results and the opponent turn.
#include "PluginState.h"

namespace tessera_plugin {
void recordResult(const ttcg::Result& result) {
    lastResult = result;
    settling = true;
    playedRuleSounds = 0;
    ++revision;
    if (match.finished() && campaign::saved.contract.held && !campaign::saved.contract.capture(match)) {
        log::error("Invalid match provenance");
        close();
        return;
    }
    if (match.finished()) {
        campaign::saved.dealReveals = {};
        const auto s = match.score();
        auto actor = opponent.get();
        campaign::recordGame(actor.get(), s[0] == s[1] ? -1 : s[0] > s[1] ? 0 : 1);
    }
    checkpointMatch();
    playSound("UIMenuFocus");
}
void runAI() {
    if (!visible || !active || thinking || settling || worldFocus.switching() || match.finished() || match.turn != 1 ||
        !checkLiveWorld())
        return;
    thinking = true;
    publish();
    const auto snapshot = match;
    auto actor = opponent.get();
    const auto* profile = campaign::profile(actor.get());
    const int skill = campaign::development.opponentSkill >= 0 ? campaign::development.opponentSkill
                      : profile                                ? profile->skill
                      : campaign::isChild(actor.get())
                          ? ttcg::childProfile(campaign::persistentID(actor->GetActorBase())).skill
                          : 3;
    const auto token = epoch.load();
    const auto round = session;
    const auto stateRevision = revision;
    std::thread([snapshot, token, round, stateRevision, skill]() {
        const bool diagnostics = spdlog::should_log(spdlog::level::debug);
        const auto started = diagnostics ? musicTime() : 0;
        auto move = ttcg::opponentMove(snapshot, skill, ttcg::matchDecisionSeed(snapshot));
        if (diagnostics)
            log::debug("AI decision: session {}, revision {}, placed {}, skill {}, hand {}, square {}, elapsed {} ms",
                       round, stateRevision, snapshot.placed, skill, move.hand, move.square, musicTime() - started);
        // Presentation has acknowledged the previous move before this search starts.
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if (epoch.load() != token) return;
        collectionIntegration::queue([move, token, round, stateRevision]() {
            if (epoch.load() != token || !visible || session != round || revision != stateRevision || match.turn != 1)
                return;
            if (!checkLiveWorld()) return;
            thinking = false;
            auto result = match.play(move);
            if (!result.legal) {
                log::error("AI returned an illegal move");
                close();
                return;
            }
            recordResult(result);
            publish();
        });
    }).detach();
}
} // namespace tessera_plugin
