#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>
#include <stdexcept>
#include "CardRegistry.h"
#include "CreatureGroups.h"
#include "Rules.h"

namespace ttcg {
struct Move {
    int hand = -1, square = -1, group = -1;
};
struct Slot {
    int card = -1, owner = -1, origin = -1;
};
struct CaptureStage {
    // Rule bits name the initial special capture; Combo uses its own presentation bit.
    enum { Combo = 8 };
    unsigned rule = 0;
    std::uint16_t highlights = 0, captures = 0;
    int group = -1;
};
struct Result {
    bool legal = false;
    std::array<bool, 9> flipped{};
    bool same = false, plus = false, sameWall = false, fallenAce = false;
    std::vector<CaptureStage> stages;
};
struct Match {
    std::array<Slot, 9> board{};
    std::array<std::array<int, 5>, 2> hands{};
    // Origin is a physical copy's original owner/slot, independent of its current hand.
    std::array<std::array<int, 5>, 2> origins{{{0, 1, 2, 3, 4}, {5, 6, 7, 8, 9}}};
    std::array<std::array<bool, 5>, 2> used{};
    std::array<std::array<bool, 5>, 2> foilHands{}; // Original copies' finishes; indexed by origin.
    int turn = 0, placed = 0;
    unsigned rules = 0;
    unsigned dealSeed = 1, randomState = 1, redeals = 0, publicOrigins = 0;
    int forcedHand = -1;
    std::array<int, 2> swapped{-1, -1};
    int activeGroup = -1;
    // Vanguard. Whoever moves first plays into an empty board and is answered last,
    // which costs them games. In return the first card of a board is one stronger on
    // every side while the player who placed it holds it, until the fifth card is
    // played. These are that card's square and that player, or -1 before the first card.
    int vanguard = -1, vanguardOwner = -1;
    // Off only where the game is being measured without it, and in rule lessons.
    bool vanguardRule = true;
    // The bonus lasts while the board holds no more than this many cards. Kept for
    // the whole board it made the first mover the favourite (docs/roadmap.md, phase 4).
    int vanguardSpan = 4;
    std::array<unsigned, 2> reveals{};
    // std::shuffle and the standard distributions give different results in each
    // standard library. What a seed deals must be the same in the game (MSVC) as in
    // the tools that write the rule demos and their captions (libstdc++).
    template <class Values> static void shuffle(Values& values, std::mt19937& rng) {
        for (std::size_t n = values.size(); n > 1; --n) std::swap(values[n - 1], values[rng() % n]);
    }
    void seedVisibility(std::uint32_t seed) {
        std::mt19937 rng(seed ^ 0x3A0FE1u);
        for (auto& mask : reveals) {
            std::array<int, 5> slots{0, 1, 2, 3, 4};
            shuffle(slots, rng);
            mask = (1u << slots[0]) | (1u << slots[1]) | (1u << slots[2]);
        }
    }
    bool known(int owner, int hand, int observer) const {
        return owner == observer || origins[owner][hand] / 5 == observer ||
               (publicOrigins & (1u << origins[owner][hand])) || used[owner][hand] || finished() ||
               ((rules & Open) || ((rules & ThreeOpen) && (reveals[owner] & (1u << hand))));
    }
    bool foil(int owner, int hand) const {
        const auto origin = origins[owner][hand];
        return foilHands[origin / 5][origin % 5];
    }
    unsigned random() {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return randomState;
    }
    void selectTurnCard() {
        forcedHand = -1;
        if (!(rules & (Order | Chaos)) || placed == 9) return;
        std::array<int, 5> choices{};
        unsigned count = 0;
        for (int h = 0; h < 5; ++h)
            if (!used[turn][h]) choices[count++] = h;
        if (count) forcedHand = choices[rules & Chaos ? random() % count : 0];
    }
    bool playableHand(int hand) const {
        return hand >= 0 && hand < 5 && !used[turn][hand] && (forcedHand < 0 || hand == forcedHand);
    }
    void prepareDeal(std::uint32_t seed) {
        dealSeed = seed;
        randomState = (seed ^ 0xC4A051u) | 1u;
        if (rules & Swap) {
            std::mt19937 rng(seed ^ 0x5A4Fu);
            swapped = {int(rng() % 5), int(rng() % 5)};
            std::swap(hands[0][swapped[0]], hands[1][swapped[1]]);
            std::swap(origins[0][swapped[0]], origins[1][swapped[1]]);
        }
        selectTurnCard();
    }
    int groupModifier(int card) const {
        return card >= 0 && activeGroup >= 0 && (cardGroups[card] & (1ull << activeGroup)) ? (rules & Legion       ? 1
                                                                                              : rules & Decimation ? -1
                                                                                                                   : 0)
                                                                                           : 0;
    }
    int affinityModifier(int card) const {
        if (card < 0 || !(rules & (Ascension | Descension)) || !cards[card].affinityMask) return 0;
        int strongest = 0;
        for (unsigned bit = 1; bit <= 16; bit <<= 1)
            if (cards[card].affinityMask & bit) {
                int count = 0;
                for (const auto& slot : board)
                    if (slot.card >= 0 && (cards[slot.card].affinityMask & bit)) ++count;
                strongest = std::max(strongest, std::clamp(count - 1, 0, 2));
            }
        return rules & Descension ? -strongest : strongest;
    }
    int dominionModifier(int card, int square) const {
        if (card < 0 || square < 0 || square >= 9 || !(rules & Dominion)) return 0;
        const auto own = cardGroups[card];
        std::uint64_t seen = 0;
        for (int side = 0; side < 4; ++side) {
            const int n = neighbor(square, side);
            if (n < 0 || board[n].card < 0) continue;
            const auto outsiders = cardGroups[board[n].card] & ~own;
            if (seen & outsiders) return -1;
            seen |= outsiders;
        }
        return 0;
    }
    std::vector<int> groupChoices(Move move) const {
        std::vector<int> choices;
        if (activeGroup >= 0 || !(rules & (Legion | Decimation)) || !playableHand(move.hand) || move.square < 0 ||
            move.square >= 9 || board[move.square].card >= 0)
            return choices;
        const auto mask = cardGroups[hands[turn][move.hand]];
        for (unsigned g = 0; g < creatureGroups.size(); ++g)
            if (mask & (1ull << g)) {
                int count = 1;
                for (const auto& slot : board)
                    if (slot.card >= 0 && (cardGroups[slot.card] & (1ull << g))) ++count;
                if (count >= 3) choices.push_back(g);
            }
        return choices;
    }
    // One to five marked squares, with independently chosen attributes; fixed per match.
    std::array<unsigned, 9> tiles{};
    void seedTiles(std::uint32_t seed) {
        tiles = {};
        if (!(rules & Affinity)) return;
        std::mt19937 rng(seed ^ 0xA771F17u);
        const int count = 1 + int(rng() % 5);
        for (int i = 0; i < count; ++i) tiles[i] = 1u << (rng() % 5);
        shuffle(tiles, rng);
    }
    // Under Reverse the lower rank wins, so there the Vanguard is one weaker instead.
    int vanguardModifier(int square) const {
        if (square < 0 || square != vanguard || board[square].owner != vanguardOwner || placed > vanguardSpan) return 0;
        return rules & Reverse ? -1 : 1;
    }
    int modifier(int card, int square) const {
        if (card < 0) return 0;
        const int shared =
            groupModifier(card) + affinityModifier(card) + dominionModifier(card, square) + vanguardModifier(square);
        if (!(rules & Affinity) || square < 0 || square >= 9 || !tiles[square]) return shared;
        return shared + (cards[card].affinityMask & tiles[square] ? 1 : -1);
    }
    int rank(int card, int square, int side) const { return cards[card].sides[side] + modifier(card, square); }
    // Who a seed has move first.
    static int firstMover(std::uint32_t seed) { return static_cast<int>(std::mt19937(seed)() % 2); }
    explicit Match(std::uint32_t seed = 1, unsigned flags = 0) : rules(flags) {
        if (!validRules(flags)) throw std::invalid_argument("Incompatible rules");
        seedTiles(seed);
        seedVisibility(seed);
        std::vector<int> ids(cards.size());
        for (std::size_t i = 0; i < ids.size(); ++i) ids[i] = static_cast<int>(i);
        std::mt19937 rng(seed);
        std::shuffle(ids.begin(), ids.end(), rng);
        for (int p = 0; p < 2; ++p)
            for (int h = 0; h < 5; ++h) hands[p][h] = ids[p * 5 + h];
        turn = static_cast<int>(rng() % 2);
        prepareDeal(seed);
    }
    // `first` names who moves first, when that was settled before the deal; otherwise the seed decides.
    Match(const std::array<int, 5>& player, const std::array<int, 5>& rival, std::uint32_t seed, unsigned flags = 0,
          int first = -1)
        : rules(flags) {
        if (!validRules(flags)) throw std::invalid_argument("Incompatible rules");
        seedTiles(seed);
        seedVisibility(seed);
        for (const auto& hand : {player, rival})
            for (int id : hand)
                if (id < 0 || id >= static_cast<int>(cards.size())) throw std::invalid_argument("Unknown card");
        hands = {player, rival};
        turn = first == 0 || first == 1 ? first : firstMover(seed);
        prepareDeal(seed);
    }
    static int neighbor(int pos, int side) {
        if (side == 0) return pos >= 3 ? pos - 3 : -1;
        if (side == 1) return pos % 3 < 2 ? pos + 1 : -1;
        if (side == 2) return pos < 6 ? pos + 3 : -1;
        return pos % 3 > 0 ? pos - 1 : -1;
    }
    bool boardComplete() const { return placed == 9; }
    bool suddenDeathPending() const {
        return boardComplete() && (rules & SuddenDeath) && redeals < 5 && score()[0] == score()[1];
    }
    bool finished() const { return boardComplete() && !suddenDeathPending(); }
    bool acePair(int a, int b) const {
        return (rules & FallenAce) && ((a == lowestRank && b == aceRank) || (a == aceRank && b == lowestRank));
    }
    bool beats(int a, int b, int printedA, int printedB) const {
        if (acePair(printedA, printedB)) return printedA == (rules & Reverse ? aceRank : lowestRank);
        return rules & Reverse ? a < b : a > b;
    }
    bool beats(int a, int b) const { return beats(a, b, a, b); }
    bool captures(int a, int square, int side, int b, int neighbor) const {
        return beats(rank(a, square, side), rank(b, neighbor, (side + 2) % 4), cards[a].sides[side],
                     cards[b].sides[(side + 2) % 4]);
    }
    std::array<int, 2> score() const {
        std::array<int, 2> s{};
        for (auto b : board)
            if (b.owner >= 0) ++s[b.owner];
        for (int p = 0; p < 2; ++p)
            for (bool u : used[p])
                if (!u) ++s[p];
        return s;
    }
    std::vector<Move> legalMoves() const {
        std::vector<Move> result;
        if (boardComplete()) return result;
        for (int h = 0; h < 5; ++h)
            if (playableHand(h))
                for (int s = 0; s < 9; ++s)
                    if (board[s].card < 0) {
                        auto groups = groupChoices({h, s});
                        if (groups.size() > 1)
                            for (int g : groups) result.push_back({h, s, g});
                        else
                            result.push_back({h, s});
                    }
        return result;
    }
    bool redeal() {
        if (!suddenDeathPending()) return false;
        std::array<int, 10> copyCards{}, owners{};
        for (int p = 0; p < 2; ++p)
            for (int h = 0; h < 5; ++h) {
                auto origin = origins[p][h];
                copyCards[origin] = hands[p][h];
                owners[origin] = p;
                if (known(p, h, 1 - p)) publicOrigins |= 1u << origin;
            }
        for (const auto& slot : board) owners[slot.origin] = slot.owner;
        // Own originals in slot order, then acquired originals in their slot order.
        for (int p = 0; p < 2; ++p) {
            int at = 0;
            for (int source : {p, 1 - p})
                for (int h = 0; h < 5; ++h) {
                    const int origin = source * 5 + h;
                    if (owners[origin] == p) {
                        hands[p][at] = copyCards[origin];
                        origins[p][at++] = origin;
                    }
                }
        }
        ++redeals;
        board = {};
        used = {};
        placed = 0;
        activeGroup = -1;
        vanguard = vanguardOwner = -1;
        swapped = {-1, -1};
        const auto seed = dealSeed + redeals * seedSpread;
        seedTiles(seed);
        seedVisibility(seed);
        turn = int(random() % 2);
        selectTurnCard();
        return true;
    }
    // The played card against its four neighbours by printed rank, which is what Same and Plus read.
    struct Contact {
        std::array<int, 4> neighbors{-1, -1, -1, -1}, sums{-1, -1, -1, -1};
        std::array<bool, 4> equal{};
        int equalCount = 0;
        bool wallMatch = false;
        // Same needs two equal sides; under Same Wall an Ace against the edge is one of them.
        bool same(int side) const { return equalCount >= 2 && equal[side]; }
        // Plus needs another side that adds up to the same total.
        bool plus(int side) const {
            if (sums[side] < 0) return false;
            for (int other = 0; other < 4; ++other)
                if (other != side && sums[other] == sums[side]) return true;
            return false;
        }
    };
    Contact touch(int id, int square, const std::array<Slot, 9>& before) const {
        Contact contact;
        for (int side = 0; side < 4; ++side) {
            const int n = neighbor(square, side);
            contact.neighbors[side] = n;
            if (n < 0) {
                if ((rules & SameWall) && cards[id].sides[side] == aceRank) {
                    ++contact.equalCount;
                    contact.wallMatch = true;
                }
                continue;
            }
            if (before[n].card < 0) continue;
            const int a = cards[id].sides[side], b = cards[before[n].card].sides[(side + 2) % 4];
            contact.sums[side] = a + b;
            contact.equal[side] = a == b;
            if (a == b) ++contact.equalCount;
        }
        return contact;
    }
    // Marks what the played card takes from its neighbours and answers which of them fell to
    // Same or Plus: only those go on to Combo.
    std::array<bool, 9> resolveCaptures(int id, int square, int owner, const std::array<Slot, 9>& before,
                                        const Contact& contact, Result& result) const {
        std::array<bool, 9> special{};
        for (int side = 0; side < 4; ++side) {
            const int n = contact.neighbors[side];
            if (n < 0 || before[n].card < 0 || before[n].owner == owner) continue;
            // An ordinary capture uses ranks as the table changes them; Same and Plus use printed ranks.
            if (captures(id, square, side, before[n].card, n)) {
                result.flipped[n] = true;
                result.fallenAce |= acePair(cards[id].sides[side], cards[before[n].card].sides[(side + 2) % 4]);
            }
            if ((rules & Same) && contact.same(side)) {
                special[n] = true;
                result.same = true;
            }
            if ((rules & Plus) && contact.plus(side)) {
                special[n] = true;
                result.plus = true;
            }
        }
        for (int n = 0; n < 9; ++n)
            if (special[n]) result.flipped[n] = true;
        result.sameWall = result.same && contact.wallMatch;
        return special;
    }
    // Same/Plus captures go on capturing their own neighbours; ordinary captures never do.
    void resolveCombo(std::vector<int> queue, int owner, bool recordStages, Result& result) {
        std::array<int, 9> depth{};
        const auto comboStart = result.stages.size();
        for (std::size_t i = 0; i < queue.size(); ++i)
            for (int side = 0; side < 4; ++side) {
                int n = neighbor(queue[i], side);
                if (n < 0 || board[n].card < 0 || board[n].owner == owner) continue;
                if (captures(board[queue[i]].card, queue[i], side, board[n].card, n)) {
                    board[n].owner = owner;
                    result.flipped[n] = true;
                    queue.push_back(n);
                    depth[n] = depth[queue[i]] + 1;
                    if (recordStages) {
                        const auto stage = comboStart + depth[n] - 1;
                        if (result.stages.size() <= stage) result.stages.push_back({CaptureStage::Combo, 0, 0});
                        result.stages[stage].captures |= 1u << n;
                        result.stages[stage].highlights |= (1u << queue[i]) | (1u << n);
                    }
                }
            }
    }
    // The stages below are what the interface plays back. The search skips them.
    CaptureStage groupStage(int square) const {
        CaptureStage stage;
        stage.rule = rules & (Legion | Decimation);
        stage.group = activeGroup;
        stage.highlights = 1u << square;
        for (int i = 0; i < 9; ++i)
            if (board[i].card >= 0 && (cardGroups[board[i].card] & (1ull << activeGroup))) stage.highlights |= 1u << i;
        return stage;
    }
    struct Modifiers {
        std::array<int, 9> affinity{}, dominion{};
    };
    // Every square's modifiers before a card lands; the card itself is measured for `square`.
    Modifiers modifiersBefore(int id, int square) const {
        Modifiers before;
        for (int s = 0; s < 9; ++s) {
            before.affinity[s] = affinityModifier(s == square ? id : board[s].card);
            before.dominion[s] = dominionModifier(board[s].card, s);
        }
        return before;
    }
    // One stage for each modifier rule the new card changed a value under.
    void recordModifierStages(const Modifiers& before, Result& result) const {
        for (unsigned rule : {rules & (Ascension | Descension), rules & Dominion})
            if (rule) {
                CaptureStage stage;
                stage.rule = rule;
                for (int s = 0; s < 9; ++s)
                    if (board[s].card >= 0 &&
                        (rule == Dominion ? dominionModifier(board[s].card, s) != before.dominion[s]
                                          : affinityModifier(board[s].card) != before.affinity[s]))
                        stage.highlights |= 1u << s;
                if (stage.highlights) result.stages.push_back(stage);
            }
    }
    // Ordinary captures first, then those by Same and Plus with the cards that made the match.
    void recordCaptureStages(int square, const std::array<Slot, 9>& before, const Contact& contact,
                             const std::array<bool, 9>& special, Result& result) const {
        CaptureStage normal, matched;
        normal.rule = result.fallenAce ? FallenAce : 0u;
        if (result.fallenAce) normal.highlights = 1u << square;
        matched.rule = (result.same ? Same : 0u) | (result.sameWall ? SameWall : 0u) | (result.plus ? Plus : 0u);
        if (matched.rule) {
            matched.highlights = 1u << square;
            for (int side = 0; side < 4; ++side) {
                const int n = contact.neighbors[side];
                if (n < 0 || before[n].card < 0) continue;
                // The player's own cards count towards a match, so they are shown too.
                if ((result.same && contact.equal[side]) || (result.plus && contact.plus(side)))
                    matched.highlights |= 1u << n;
            }
        }
        for (int n = 0; n < 9; ++n)
            if (result.flipped[n]) (special[n] ? matched : normal).captures |= 1u << n;
        if (normal.captures) result.stages.push_back(normal);
        if (matched.captures) result.stages.push_back(matched);
    }
    Result play(Move m, bool recordStages = true) {
        Result result;
        if (boardComplete() || m.group < -1 || m.group >= static_cast<int>(creatureGroups.size()) ||
            !playableHand(m.hand) || m.square < 0 || m.square >= 9 || board[m.square].card >= 0)
            return result;
        const auto choices = groupChoices(m);
        // A group must be named when there is a choice, and may only be one on offer.
        const bool offered = std::find(choices.begin(), choices.end(), m.group) != choices.end();
        if ((choices.size() > 1 || m.group >= 0) && !offered) return result;
        const int owner = turn, id = hands[owner][m.hand];
        if (!choices.empty()) {
            activeGroup = choices.size() == 1 ? choices[0] : m.group;
            if (recordStages) result.stages.push_back(groupStage(m.square));
        }
        result.legal = true;
        const auto before = board;
        const auto modifiers = recordStages ? modifiersBefore(id, m.square) : Modifiers{};
        // The incoming card affects both sides' modifiers before any capture.
        board[m.square] = {id, owner, origins[owner][m.hand]};
        if (vanguardRule && placed == 0) {
            vanguard = m.square;
            vanguardOwner = owner;
        }
        if (recordStages) recordModifierStages(modifiers, result);
        const auto contact = touch(id, m.square, before);
        const auto special = resolveCaptures(id, m.square, owner, before, contact, result);
        publicOrigins |= 1u << origins[owner][m.hand];
        used[owner][m.hand] = true;
        ++placed;
        std::vector<int> matched;
        for (int n = 0; n < 9; ++n) {
            if (special[n]) matched.push_back(n);
            if (result.flipped[n]) board[n].owner = owner;
        }
        if (recordStages) recordCaptureStages(m.square, before, contact, special, result);
        resolveCombo(std::move(matched), owner, recordStages, result);
        turn = 1 - owner;
        selectTurnCard();
        return result;
    }
};
inline int evaluate(const Match& m) {
    auto s = m.score();
    int value = (s[1] - s[0]) * 100;
    if (m.boardComplete()) return value * 100;
    for (int p = 0; p < 9; ++p)
        if (m.board[p].card >= 0)
            for (int side = 0; side < 4; ++side) {
                int n = Match::neighbor(p, side);
                if (n < 0 || m.board[n].card >= 0) continue;
                int rank = m.rank(m.board[p].card, p, side);
                value += (m.board[p].owner == 1 ? 1 : -1) * (m.rules & Reverse ? reversedRank(rank) : rank);
            }
    return value;
}
inline int search(const Match& m, int depth, int alpha, int beta, int& budget);
inline int searchMoves(const Match& m, int depth, int alpha, int beta, int& budget) {
    int best = m.turn == 1 ? -1000000 : 1000000;
    for (auto move : m.legalMoves()) {
        auto next = m;
        next.play(move, false);
        int value =
            search(next, depth - 1, m.rules & Chaos ? -1000000 : alpha, m.rules & Chaos ? 1000000 : beta, budget);
        if (m.turn == 1) {
            best = std::max(best, value);
            alpha = std::max(alpha, best);
        } else {
            best = std::min(best, value);
            beta = std::min(beta, best);
        }
        if (beta <= alpha || budget <= 0) break;
    }
    return best;
}
inline int search(const Match& m, int depth, int alpha, int beta, int& budget) {
    if (--budget <= 0 || depth == 0 || m.boardComplete()) return evaluate(m);
    if (!(m.rules & Chaos)) return searchMoves(m, depth, alpha, beta, budget);
    // Future Chaos choices are chance nodes, not the actual game's RNG stream.
    // Only the current turn's selection is known at the root of chooseMove.
    int sum = 0, count = 0;
    for (int h = 0; h < 5; ++h)
        if (!m.used[m.turn][h]) {
            auto chance = m;
            chance.forcedHand = h;
            sum += searchMoves(chance, depth, -1000000, 1000000, budget);
            ++count;
            if (budget <= 0) break;
        }
    return count ? sum / count : evaluate(m);
}
// What a search of the side to move's options found: every legal move with its
// value, best first, at the deepest ply that finished inside the node budget.
// Values are from the second player's side, as `evaluate` gives them.
struct RootSearch {
    std::vector<Move> moves;
    std::vector<int> values;
    int depth = 0;
    Move best() const { return moves.empty() ? Move{} : moves.front(); }
};
// Iterative deepening with a node ceiling. Open hands, deterministic search.
// With `exact`, every move is searched on a full window and its value is exact, so
// values from several searches can be added up. Without it only the best move's
// value is exact: the others are cut off as soon as they can't beat it, which
// buys about one more ply from the same budget.
inline RootSearch searchRoot(const Match& m, int nodeLimit = 100000, int maxDepth = 9, bool exact = false) {
    RootSearch result;
    result.moves = m.legalMoves();
    result.values.assign(result.moves.size(), 0);
    if (result.moves.empty()) return result;
    const bool second = m.turn == 1;
    int budget = nodeLimit;
    auto moves = result.moves;
    std::vector<int> values(moves.size());
    for (int depth = 1; depth <= std::min(maxDepth, 9 - m.placed); ++depth) {
        int best = second ? -1000000 : 1000000;
        for (std::size_t i = 0; i < moves.size(); ++i) {
            auto next = m;
            next.play(moves[i], false);
            const bool window = !exact && i;
            values[i] =
                search(next, depth - 1, window && second ? best : -1000000, window && !second ? best : 1000000, budget);
            if (budget <= 0) return result;
            best = second ? std::max(best, values[i]) : std::min(best, values[i]);
        }
        // Best first: the next, deeper pass then prunes against the likeliest move.
        std::vector<std::size_t> order(moves.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return second ? values[a] > values[b] : values[a] < values[b];
        });
        for (std::size_t i = 0; i < order.size(); ++i) {
            result.moves[i] = moves[order[i]];
            result.values[i] = values[order[i]];
        }
        result.depth = depth;
        moves = result.moves;
    }
    return result;
}
inline Move chooseMove(const Match& m, int nodeLimit = 100000, int maxDepth = 9) {
    return searchRoot(m, nodeLimit, maxDepth).best();
}
}
