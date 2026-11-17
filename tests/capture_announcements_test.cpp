#include "Presentation.h"
#include "Lesson.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <iostream>

using namespace ttcg;
using Json = nlohmann::json;
Json snapshot(const Match& match, const Result& result) {
    auto data = Json::parse(snapshotJson(match, 1, 1, "Hulda", true, false, result, true));
    assert(data["stages"].size() == result.stages.size());
    for (unsigned i = 0; i < result.stages.size(); ++i) {
        assert(data["stages"][i]["captures"] == result.stages[i].captures);
        assert(data["stages"][i]["highlights"] == result.stages[i].highlights);
    }
    for (int i = 0; i < 9; ++i) assert(data["modifiers"][i] == match.modifier(match.board[i].card, i));
    return data;
}
int main() {
    for (unsigned rule : {Ascension, Descension, Dominion}) {
        assert(captureLabel(rule).empty());
        assert(captureLabel(rule | Same | Plus) == "Same / Plus");
        auto demo = ruleExample(rule);
        bool triggered = false;
        while (!demo.complete()) {
            const auto move = demo.expected();
            assert(demo.match.turn == 0 ? demo.advance(move.hand, move.square) : demo.advance());
            auto data = snapshot(demo.match, demo.last);
            for (unsigned i = 0; i < demo.last.stages.size(); ++i)
                if (demo.last.stages[i].rule == rule) {
                    triggered = true;
                    assert(data["stages"][i]["label"] == "");
                }
        }
        assert(triggered); // Silent announcements still retain real strength changes.
    }
    auto waves = Lesson(true, true).match;
    waves.board[5] = {Lesson::card("murkwater-goblin"), 1};
    ++waves.placed;
    const auto cascade = waves.play({3, 0});
    assert(cascade.stages.size() == 3);
    const auto data = snapshot(waves, cascade);
    assert(data["stages"][0]["label"] == "Plus");
    assert(data["stages"][1]["label"] == "Combo" && data["stages"][2]["label"] == "");
    assert(data["stages"][1]["captures"] == 4 && data["stages"][2]["captures"] == 32);
    assert(snapshot(waves, cascade) == data); // Snapshot publication cannot consume an announcement.
    for (int move = 0; move < 2; ++move) {
        CaptureAnnouncements announcements;
        assert(announcements.next(CaptureStage::Combo) == "Combo");
        assert(announcements.next(CaptureStage::Combo).empty());
        assert(announcements.next(Legion) == "Legion");
        assert(announcements.next(Decimation) == "Decimation");
    }
    {
        // The capture preview is the move itself, played on a copy: for every card
        // and square it names exactly the cards that play then takes.
        unsigned previews = 0, captures = 0, chains = 0;
        for (unsigned seed = 1; seed <= 120; ++seed) {
            const unsigned rules[]{0,
                                   Open,
                                   Same | Plus,
                                   Open | Same | Plus | SameWall,
                                   Reverse | FallenAce,
                                   Affinity | Legion,
                                   Open | Decimation | Ascension,
                                   Dominion | Descension | Swap};
            Match match(seed, rules[seed % 8]);
            std::mt19937 rng(seed);
            while (!match.boardComplete()) {
                const auto shown = capturePreviews(match, true);
                const auto data = Json::parse(snapshotJson(match, 1, 1, "Hulda", true, false, {}, false));
                if (match.turn != 0) {
                    assert(shown == (std::array<std::array<unsigned, 9>, 5>{}));
                } else {
                    for (int h = 0; h < 5; ++h)
                        for (int square = 0; square < 9; ++square) {
                            assert(data["capturePreviews"][h][square] == shown[h][square]);
                            auto next = match;
                            const auto groups = match.groupChoices({h, square});
                            const auto played = next.play({h, square, groups.size() > 1 ? groups.front() : -1});
                            if (!played.legal) {
                                assert(!shown[h][square]);
                                continue;
                            }
                            unsigned taken = 0;
                            for (int q = 0; q < 9; ++q)
                                if (q != square && match.board[q].card >= 0 && next.board[q].owner == 0 &&
                                    match.board[q].owner == 1)
                                    taken |= 1u << q;
                            assert(shown[h][square] == taken && !(taken & (1u << square)));
                            ++previews;
                            captures += taken != 0;
                            chains += played.stages.size() > 1;
                        }
                    // It reads the player's hand and the board, never the opponent's cards.
                    auto other = match;
                    for (auto& card : other.hands[1]) card = (card + 7) % int(cards.size());
                    assert(capturePreviews(other, true) == shown);
                }
                // Nothing to show before the table is live, or while the last play is still settling.
                assert(capturePreviews(match, false) == (std::array<std::array<unsigned, 9>, 5>{}));
                const auto moves = match.legalMoves();
                match.play(moves[rng() % moves.size()]);
            }
            assert(capturePreviews(match, true) == (std::array<std::array<unsigned, 9>, 5>{}));
        }
        assert(previews > 3000 && captures > 300 && chains > 20);
    }
    std::cout
        << "Capture announcements: silent strength rules, one Combo per chain, repeatable snapshots, ordered waves and unchanged modifiers passed\n";
}
