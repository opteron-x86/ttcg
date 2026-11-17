#include "SavedMatch.h"
#include "Lesson.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace ttcg;
int main() {
    Match m(4);
    assert(m.score()[0] == 5 && m.score()[1] == 5);
    auto before = m;
    assert(!m.play({-1, 0}).legal);
    assert(!m.play({0, 9}).legal);
    assert(m.placed == before.placed);
    assert(m.play({0, 0}).legal);
    assert(!m.play({1, 0}).legal);
    // The actual native resolver is exercised, including both starting players.
    for (unsigned flags : {0u, 3u, 4u, 32u, 35u, 36u, 48u, 51u})
        for (unsigned seed = 0; seed < 60; ++seed) {
            Match game(seed, flags);
            std::mt19937 rng(seed);
            while (!game.finished()) {
                auto legal = game.legalMoves();
                assert(!legal.empty());
                auto action = legal[rng() % legal.size()];
                const auto before = game;
                auto fast = game;
                auto result = game.play(action);
                auto unrecorded = fast.play(action, false);
                assert(result.legal && unrecorded.stages.empty());
                assert(result.flipped == unrecorded.flipped && result.same == unrecorded.same &&
                       result.plus == unrecorded.plus);
                unsigned seen = 0;
                for (const auto& stage : result.stages) {
                    assert(stage.captures && !(seen & stage.captures));
                    if (stage.rule) assert((stage.highlights & stage.captures) == stage.captures);
                    for (int i = 0; i < 9; ++i)
                        if (stage.captures & (1u << i))
                            assert(before.board[i].owner == 1 - before.turn && game.board[i].owner == before.turn);
                    seen |= stage.captures;
                }
                for (int i = 0; i < 9; ++i) {
                    assert(bool(seen & (1u << i)) == result.flipped[i]);
                    assert(game.board[i].owner == fast.board[i].owner && game.board[i].card == fast.board[i].card);
                }
                auto score = game.score();
                assert(score[0] + score[1] == 10);
                for (auto slot : game.board) assert(slot.owner >= -1 && slot.owner <= 1);
            }
            assert(game.legalMoves().empty());
            assert(!game.play({0, 0}).legal);
        }
    // Same includes a friendly matching participant, but ordinary comparisons do not.
    auto same = Lesson(true).match;
    same.board[1].owner = 0;
    auto basic = same;
    basic.rules = Open;
    auto result = same.play({3, 0});
    assert(result.same && result.flipped[3]);
    assert(result.stages[0].rule == Same && result.stages[0].highlights == 11);
    assert(!basic.play({3, 0}).flipped[3]);
    // Plus starts a three-wave capture chain; each wave highlights its own source.
    auto waves = Lesson(true, true).match;
    waves.board[5] = {Lesson::card("murkwater-goblin"), 1};
    ++waves.placed;
    auto cascade = waves.play({3, 0});
    assert(cascade.stages.size() == 3 && cascade.plus);
    assert(cascade.stages[0].rule == Plus && cascade.stages[0].captures == 10 && cascade.stages[0].highlights == 11);
    assert(cascade.stages[1].rule == CaptureStage::Combo && cascade.stages[1].captures == 4 &&
           cascade.stages[1].highlights == 6);
    assert(cascade.stages[2].rule == CaptureStage::Combo && cascade.stages[2].captures == 32 &&
           cascade.stages[2].highlights == 36);
    // Ordinary capture never recurses. Use actual profiles satisfying both edges.
    int attacker = Lesson::card("legends-220827"), defender = Lesson::card("oldgate-warden"),
        bystander = Lesson::card("legends-299178");
    assert(cards[attacker].sides[0] > cards[defender].sides[2] && cards[defender].sides[3] > cards[bystander].sides[1]);
    Match noCombo(1);
    noCombo.turn = 0;
    noCombo.hands[0][0] = attacker;
    noCombo.board[1] = {defender, 1};
    noCombo.board[0] = {bystander, 1};
    noCombo.placed = 2;
    auto ordinary = noCombo.play({0, 4});
    assert(ordinary.flipped[1] && !ordinary.flipped[0]);
    Match draw;
    draw.placed = 9;
    for (int i = 0; i < 9; ++i) draw.board[i] = {i, i < 5 ? 0 : 1};
    draw.used[0].fill(true);
    draw.used[1].fill(true);
    draw.used[1][4] = false;
    assert(draw.finished() && draw.score()[0] == 5 && draw.score()[1] == 5);
    auto reverse = Lesson::workshop(Reverse);
    assert(reverse.advance(1, 4) && reverse.advance());
    assert(reverse.match.board[4].owner == 1);
    // What a seed deals is fixed, whichever standard library built the game: the
    // rule demos and their captions are written on one and shown on the other.
    {
        struct Deal {
            unsigned seed;
            std::array<unsigned, 9> tiles;
            std::array<unsigned, 2> reveals;
        };
        for (const auto& deal :
             {Deal{1, {0, 16, 0, 8, 16, 0, 0, 0, 4}, {14, 25}}, Deal{73, {0, 0, 0, 16, 0, 16, 0, 0, 0}, {11, 14}},
              Deal{20251116, {1, 0, 0, 0, 1, 0, 0, 0, 0}, {11, 22}}}) {
            const Match pinned({0, 1, 2, 3, 4}, {5, 6, 7, 8, 9}, deal.seed, Affinity | ThreeOpen);
            assert(pinned.tiles == deal.tiles && pinned.reveals == deal.reveals);
        }
    }
    // Affinity marks 1–5 distinct squares. Attributes may repeat; no marker when off.
    Match affinity(17, Affinity), repeat(17, Affinity), unmarked(17);
    assert(affinity.tiles == repeat.tiles);
    unsigned countsSeen = 0, attributesSeen = 0;
    bool repeatedAttribute = false;
    for (unsigned seed = 0; seed < 1000; ++seed) {
        Match layout(seed, Affinity), again(seed, Affinity), off(seed);
        Match dealt(layout.hands[0], layout.hands[1], seed, Affinity);
        assert(layout.tiles == again.tiles && layout.tiles == dealt.tiles);
        assert(std::all_of(off.tiles.begin(), off.tiles.end(), [](auto t) { return !t; }));
        int count = 0;
        unsigned seen = 0;
        for (unsigned tile : layout.tiles)
            if (tile) {
                assert(tile <= 16 && (tile & (tile - 1)) == 0);
                ++count;
                repeatedAttribute |= bool(seen & tile);
                seen |= tile;
            }
        assert(count >= 1 && count <= 5);
        countsSeen |= 1u << count;
        attributesSeen |= seen;
    }
    assert(countsSeen == 62 && attributesSeen == 31 && repeatedAttribute);
    assert(std::all_of(unmarked.tiles.begin(), unmarked.tiles.end(), [](auto t) { return !t; }));
    const int attuned = Lesson::card("murkwater-goblin"), neutral = Lesson::card("legends-257893");
    affinity.tiles.fill(0);
    affinity.tiles[4] = cards[attuned].affinityMask;
    assert(affinity.modifier(attuned, 4) == 1 && affinity.modifier(neutral, 4) == -1 &&
           affinity.modifier(attuned, 0) == 0);
    int one = -1;
    for (int i = 0; i < cards.size(); ++i)
        if (cards[i].sides[0] == 1 && !(cards[i].affinityMask & affinity.tiles[4])) {
            one = i;
            break;
        }
    assert(one >= 0 && affinity.rank(one, 4, 0) == 0);
    // Printed-rank Same remains valid when either tile changes effective numbers.
    auto elementalSame = Lesson(true).match;
    elementalSame.rules |= Affinity;
    elementalSame.tiles = {};
    elementalSame.tiles[0] = 1;
    assert(elementalSame.play({3, 0}).same);
    // A matching tile protects an equal printed edge; a mismatch exposes it.
    for (unsigned rule : {Affinity, Legion, Decimation}) {
        auto d = Lesson::workshop(rule);
        assert(d.advance(1, 4) && d.advance());
        assert(d.match.board[4].owner == (rule == Decimation ? 1 : 0));
    }
    {
        // Vanguard: the first card of a board is one stronger on every side while
        // the player who placed it holds it.
        int first = -1, level = -1, stronger = -1, retaker = -1, spare = -1;
        for (int f = 0; f < int(cards.size()) && first < 0; ++f) {
            const auto& c = cards[f];
            if (c.sides[1] > 8 || c.sides[3] > 7 || c.sides[0] > 8) continue;
            int a = -1, b = -1, r = -1;
            for (int i = 0; i < int(cards.size()); ++i) {
                if (i == f) continue;
                if (a < 0 && cards[i].sides[3] == c.sides[1] + 1) a = i;
                if (b < 0 && i != a && cards[i].sides[1] >= c.sides[3] + 2 && cards[i].sides[0] == 1) b = i;
                if (r < 0 && i != a && i != b && cards[i].sides[2] > c.sides[0] && cards[i].sides[2] <= 9) r = i;
            }
            if (a >= 0 && b >= 0 && r >= 0) first = f, level = a, stronger = b, retaker = r;
        }
        for (int i = 0; i < int(cards.size()) && spare < 0; ++i)
            if (i != first && i != level && i != stronger && i != retaker && cards[i].sides[2] >= 2) spare = i;
        assert(first >= 0 && spare >= 0);
        const auto deal = [&](unsigned rules, bool rule = true) {
            Match m({first, spare, retaker, spare, spare}, {level, stronger, spare, spare, spare}, 5, rules, 0);
            m.vanguardRule = rule;
            return m;
        };
        auto m = deal(Open);
        assert(m.turn == 0 && m.vanguard == -1 && m.modifier(first, 4) == 0);
        assert(m.play({0, 4}).legal && m.vanguard == 4 && m.vanguardOwner == 0 && m.modifier(first, 4) == 1);
        assert(m.rank(first, 4, 1) == cards[first].sides[1] + 1);
        // A card one higher than the printed rank no longer takes it.
        assert(m.play({0, 5}).legal && m.board[4].owner == 0);
        auto without = deal(Open, false);
        assert(without.play({0, 4}).legal && without.vanguard == -1 && without.play({0, 5}).legal &&
               without.board[4].owner == 1);
        // Captured, it is an ordinary card; retaken by the first mover, it is the Vanguard again.
        assert(m.play({1, 0}).legal && m.play({1, 3}).legal && m.board[4].owner == 1 && m.modifier(first, 4) == 0);
        assert(m.vanguard == 4 && m.vanguardOwner == 0);
        assert(m.play({2, 1}).legal && m.board[4].owner == 0 && m.modifier(first, 4) == 1);
        // Only the first card: no later card is given the bonus.
        for (int square = 0; square < 9; ++square)
            if (square != 4) assert(m.vanguardModifier(square) == 0);
        // The table saves with it, and a table from before it has none.
        SavedMatch saved, loaded;
        saved.present = true;
        saved.opponent = 123;
        saved.table = m;
        auto wire = encodeMatch(saved);
        assert(wire.size() == savedMatchWords && decodeMatch(wire, loaded));
        assert(loaded.table.vanguard == 4 && loaded.table.vanguardOwner == 0 && loaded.table.modifier(first, 4) == 1);
        auto broken = wire;
        broken.back() = 3 | (1u << 4); // Names an empty square.
        assert(!decodeMatch(broken, loaded));
        broken.back() = 0;
        assert(!decodeMatch(broken, loaded));
        wire.pop_back();
        assert(decodeMatch(wire, loaded) && loaded.table.vanguard == -1 && loaded.table.modifier(first, 4) == 0);
        // Under Reverse lower is stronger, so there the Vanguard is one lower.
        auto reverse = deal(Open | Reverse);
        assert(reverse.play({0, 4}).legal && reverse.modifier(first, 4) == -1);
        // Same and Plus compare printed ranks; the Vanguard changes neither.
        // Sudden Death deals a new board, whose first card is its own Vanguard.
        for (unsigned seed = 1; seed < 400; ++seed) {
            Match sudden(seed, Open | SuddenDeath);
            while (!sudden.boardComplete()) sudden.play(sudden.legalMoves().front());
            if (!sudden.suddenDeathPending()) continue;
            assert(sudden.vanguard >= 0 && sudden.redeal() && sudden.vanguard == -1 && sudden.vanguardOwner == -1);
            const int opener = sudden.turn;
            assert(sudden.play(sudden.legalMoves().front()).legal && sudden.vanguardOwner == opener);
            break;
        }
        // The search sees the bonus: it is part of every rank it compares.
        auto searched = deal(Open);
        searched.play({0, 4});
        const auto answer = chooseMove(searched, 5000, 2);
        auto reply = searched;
        reply.play(answer);
        assert(!(answer.hand == 0 && answer.square == 5) || reply.board[4].owner == 0);
    }
    // Every affinity bit is used by at least one finished card, and every printed side is valid.
    unsigned attributes = 0;
    for (const auto& card : cards) {
        assert(card.tier >= 1 && card.tier <= 10);
        for (int n : card.sides) assert(n >= 1 && n <= 10);
        if (!card.pending) attributes |= card.affinityMask;
    }
    assert(attributes == 31 && creatureGroups.size() == 18);
    auto start = std::chrono::steady_clock::now();
    for (unsigned seed = 0; seed < 4; ++seed) {
        Match game(seed, seed % 2 ? 3 : 0);
        while (!game.finished()) {
            auto action = chooseMove(game);
            assert(game.play(action).legal);
        }
        assert(game.score()[0] + game.score()[1] == 10);
    }
    std::cout << "Native rules and AI: passed ("
              << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
              << " ms AI matches)\n";
}
