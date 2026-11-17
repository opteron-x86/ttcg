#pragma once
#include "Match.h"
#include <string_view>
#include "RuntimeCatalog.h"
#include <map>
namespace ttcg {
struct OpponentProfile {
    std::uint32_t base;
    const char* name;
    const char* location;
    const char* skillName;
    unsigned rules;
    int wager;
    std::array<std::uint32_t, 32> collection; // Permanent collection IDs; unused slots are 0.
    std::uint32_t reference, knownGlobal;
    const char* region;
    std::uint32_t alternateBase = 0, alternateReference = 0;
    std::uint32_t hold = 1; // Permanent Whiterun hold ID.
    // The skill the player is shown, 0 Beginner to 5 Master. It sets how hard they think.
    unsigned level = 0;
    const char* master = "Skyrim.esm";
    bool traveller = false;
    std::uint32_t signature = 0, dialogueFaction = 0;
    bool dynamic = false, automatic = false, tournamentEligible = true, requiredMaster = true;
    bool personalRules = false;
    unsigned personalTrade = 1;
    int reputationGate = -1; // -1 follows the ordinary skill gate.
    bool restock = true;
    // Authored road encounters use their base as a stable collection key.
    bool spawned = false;
};
#include "OpponentProfiles.inc"
inline constexpr RuntimeCatalog<baseOpponents> opponents{};
inline bool isMaiq(const OpponentProfile& p) { return !p.dynamic && p.base == 0x954BF; }
inline auto& opponentPools() {
    static std::map<std::uint32_t, std::map<std::uint32_t, int>> pools;
    return pools;
}
inline int opponentIndex(std::uint32_t base) {
    for (std::size_t i = 0; i < opponents.size(); ++i)
        if (opponents[i].base == base || (opponents[i].alternateBase && opponents[i].alternateBase == base))
            return static_cast<int>(i);
    return -1;
}
inline std::uint32_t opponentLedgerKey(std::uint32_t base, std::uint32_t reference) {
    const int i = opponentIndex(base);
    return i >= 0 ? opponents[i].reference : reference;
}
// How hard an opponent thinks. One entry for each skill a player can be shown:
// Beginner, Learner, Regular, Experienced, Expert, Master.
struct SearchStyle {
    int nodes;    // Positions examined for one move, shared between the guesses.
    int plies;    // How far ahead it may look.
    int guesses;  // Hands imagined for an opponent whose cards are hidden.
    int careless; // One move in this many is played without thought; 0 for never.
};
// Measured with tools/ai-lab; docs/roadmap.md has the tables. Each step scores about
// 0.6 against the one below it when hands are hidden, and more when they are open.
inline constexpr std::array<SearchStyle, 6> searchStyles{
    {{1000, 1, 1, 3}, {2000, 1, 1, 8}, {5000, 2, 1, 0}, {10000, 3, 1, 0}, {20000, 4, 3, 0}, {280000, 9, 16, 0}}};
inline constexpr int masterLevel = int(searchStyles.size()) - 1;
inline constexpr std::array<const char*, 6> skillLevelNames{"Beginner",    "Learner", "Regular",
                                                            "Experienced", "Expert",  "Master"};
inline constexpr const SearchStyle& searchStyle(int level) {
    return searchStyles[std::size_t(std::clamp(level, 0, masterLevel))];
}
// The table as the player to move may imagine it: every card they cannot see in the
// other hand is replaced by one that hand could hold. Unseen ranks are never read.
// A hand is drawn from cards a player can own, and keeps to the deck limits with
// the cards that hand has already shown: one copy of a unique card, two of any other.
inline Match imagineHiddenHand(const Match& actual, std::mt19937& rng) {
    auto belief = actual;
    if (actual.rules & Open) return belief;
    const int mover = actual.turn, other = 1 - mover;
    std::vector<int> pool;
    for (int i = 0; i < int(cards.size()); ++i)
        if (cards[i].available && !cards[i].pending) pool.push_back(i);
    if (pool.empty())
        for (int i = 0; i < int(cards.size()); ++i) pool.push_back(i);
    // Copies of each card dealt to the other hand that are in plain sight.
    std::map<int, int> shown;
    for (int p = 0; p < 2; ++p)
        for (int h = 0; h < 5; ++h)
            if (actual.origins[p][h] / 5 == other && actual.known(p, h, mover)) ++shown[actual.hands[p][h]];
    for (const auto& slot : actual.board)
        if (slot.card >= 0 && slot.origin / 5 == other) ++shown[slot.card];
    for (int h = 0; h < 5; ++h) {
        if (actual.known(other, h, mover)) continue;
        int card = pool[rng() % pool.size()];
        for (int attempt = 0; attempt < 8 && shown[card] >= (cards[card].unique ? 1 : 2); ++attempt)
            card = pool[rng() % pool.size()];
        ++shown[card];
        belief.hands[other][h] = card;
    }
    return belief;
}
// The move for whoever is to play. Against a hidden hand it imagines several hands,
// searches each and plays the move that does best across them all: a move that
// only works against one guess is a worse choice than one that holds up against many.
inline Move opponentMove(const Match& actual, const SearchStyle& style, std::uint32_t seed) {
    auto moves = actual.legalMoves();
    if (moves.empty()) return {};
    std::mt19937 rng(seed);
    if (style.careless && rng() % unsigned(style.careless) == 0) return moves[rng() % moves.size()];
    bool hidden = false;
    for (int h = 0; h < 5; ++h) hidden |= !actual.known(1 - actual.turn, h, actual.turn);
    // Nothing to guess: one search, which may prune at the root.
    if (!hidden) return searchRoot(actual, style.nodes, style.plies).best();
    const int guesses = std::max(1, style.guesses);
    const auto key = [](const Move& move) { return std::array{move.hand, move.square, move.group}; };
    std::map<std::array<int, 3>, long long> totals;
    for (int guess = 0; guess < guesses; ++guess) {
        const auto found = searchRoot(imagineHiddenHand(actual, rng), style.nodes / guesses, style.plies, true);
        for (std::size_t i = 0; i < found.moves.size(); ++i) totals[key(found.moves[i])] += found.values[i];
    }
    // The legal moves are the same under every guess: they depend on the mover's hand and the board.
    auto best = moves.front();
    auto value = totals[key(best)];
    for (const auto& move : moves)
        if (const auto total = totals[key(move)]; actual.turn == 1 ? total > value : total < value) {
            best = move;
            value = total;
        }
    return best;
}
inline Move opponentMove(const Match& actual, int level, std::uint32_t seed) {
    return opponentMove(actual, searchStyle(level), seed);
}
// Games nobody watches, such as the other tables of a tournament, run on the game
// thread. They keep each skill's carelessness and a shallow look ahead, at a small
// fraction of the cost: the result has to be plausible, not the game well played.
inline constexpr SearchStyle offscreenStyle(int level) {
    level = std::clamp(level, 0, masterLevel);
    return {300 + 200 * level, level < 4 ? 1 : 2, 1, searchStyle(level).careless};
}
}
