// Measures the opponents against each other. Not shipped; see docs/roadmap.md, phase 3.
//
//   ai-lab match RULES DEALS A B [LOW HIGH]   A's score against B over mirrored deals
//   ai-lab ladder RULES DEALS                 each skill against the one below it
//   ai-lab depth RULES DEALS AGENT            plies finished at each move of a game
//   ai-lab first RULES DEALS AGENT            the first mover's score in self-play
//   ai-lab tournament EVENTS                  time to play out whole tournaments off screen
//   ai-lab smoke                              a short run that fails if the ladder is upside down
//
// RULES is the rule mask (0 hidden hands, 16 Open, 19 Open + Same + Plus, ...).
// An agent is a skill name (beginner, learner, regular, experienced, expert, master),
// "nodes:plies:guesses[:careless]" for a search of your own, "sees:AGENT" for that
// agent shown the hidden hand (an upper bound), or "v033:0-3" for the 0.33 opponents.
#include "Tournaments.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
using namespace ttcg;

namespace {
struct Agent {
    std::string name;
    SearchStyle style{};
    bool sees = false;
    int legacy = -1;
};
const char* const levels[]{"beginner", "learner", "regular", "experienced", "expert", "master"};
Agent parse(std::string text) {
    Agent agent{text};
    if (text.rfind("sees:", 0) == 0) {
        agent = parse(text.substr(5));
        agent.name = text;
        agent.sees = true;
        return agent;
    }
    if (text.rfind("v033:", 0) == 0) {
        agent.legacy = std::clamp(std::atoi(text.c_str() + 5), 0, 3);
        return agent;
    }
    for (int level = 0; level < 6; ++level)
        if (text == levels[level]) {
            agent.style = searchStyle(level);
            return agent;
        }
    int values[4]{0, 9, 1, 0}, count = 0;
    for (std::size_t at = 0; at <= text.size() && count < 4;) {
        const auto colon = std::min(text.find(':', at), text.size());
        values[count++] = std::atoi(text.substr(at, colon - at).c_str());
        at = colon + 1;
    }
    if (values[0] <= 0) {
        std::fprintf(stderr, "unknown agent %s\n", text.c_str());
        std::exit(2);
    }
    agent.style = {values[0], values[1], values[2], values[3]};
    return agent;
}
// The opponents as 0.33.0 shipped them: one hand guessed from every card.
Move legacyMove(const Match& actual, int skill, std::uint32_t seed) {
    auto moves = actual.legalMoves();
    std::mt19937 rng(seed);
    if (skill <= 0 && rng() % 3 == 0) return moves[rng() % moves.size()];
    auto belief = actual;
    if (!(actual.rules & Open))
        for (int h = 0; h < 5; ++h)
            if (!actual.known(1 - actual.turn, h, actual.turn))
                belief.hands[1 - actual.turn][h] = int(rng() % cards.size());
    const int nodes = skill <= 0   ? 1000
                      : skill == 1 ? 5000
                      : skill == 2 ? 20000
                                   : 70000,
              plies = skill <= 0   ? 1
                      : skill == 1 ? 2
                      : skill == 2 ? 4
                                   : 7;
    // The 0.33.0 root search: a full window for every move, in hand order.
    Move completed = moves.front();
    int budget = nodes;
    for (int depth = 1; depth <= std::min(plies, 9 - belief.placed); ++depth) {
        int best = belief.turn == 1 ? -1000000 : 1000000;
        Move candidate = completed;
        for (auto move : moves) {
            auto next = belief;
            next.play(move, false);
            const int value = search(next, depth - 1, -1000000, 1000000, budget);
            if (budget <= 0) return completed;
            if ((belief.turn == 1 && value > best) || (belief.turn == 0 && value < best)) {
                best = value;
                candidate = move;
            }
        }
        completed = candidate;
    }
    return completed;
}
Move play(const Agent& agent, const Match& table, std::uint32_t seed) {
    if (agent.legacy >= 0) return legacyMove(table, agent.legacy, seed);
    if (!agent.sees) return opponentMove(table, agent.style, seed);
    auto open = table;
    open.rules |= Open;
    open.rules &= ~ThreeOpen;
    return opponentMove(open, agent.style, seed);
}
std::vector<int> tierPool(int low, int high) {
    std::vector<int> pool;
    for (int i = 0; i < int(cards.size()); ++i)
        if (cards[i].available && !cards[i].pending && cards[i].tier >= low && cards[i].tier <= high) pool.push_back(i);
    return pool;
}
std::array<int, 5> deal(std::mt19937& rng, const std::vector<int>& pool) {
    std::array<int, 5> hand{};
    for (auto& card : hand) card = pool[rng() % pool.size()];
    return hand;
}
template <class Work> void parallel(int count, Work work) {
    std::atomic<int> next{0};
    std::vector<std::thread> threads;
    const unsigned workers = std::max(1u, std::thread::hardware_concurrency() - 2);
    for (unsigned n = 0; n < workers; ++n)
        threads.emplace_back([&] {
            for (int item; (item = next++) < count;) work(item);
        });
    for (auto& thread : threads) thread.join();
}
struct Result {
    double score = 0, milliseconds = 0;
    int games = 0, wins = 0, draws = 0, losses = 0;
};
// A's score against B. Every deal is played twice, with the seats exchanged.
Result match(unsigned rules, int deals, const Agent& a, const Agent& b, int low = 4, int high = 7) {
    const auto pool = tierPool(low, high);
    std::mutex lock;
    Result result;
    long long micros = 0, moves = 0;
    parallel(deals, [&](int index) {
        std::mt19937 rng(0xC0FFEEu + unsigned(index) * 7919u);
        const auto first = deal(rng, pool), second = deal(rng, pool);
        const auto seed = rng();
        for (int seat = 0; seat < 2; ++seat) {
            Match table(first, second, seed, rules);
            std::uint32_t moveSeed = seed ^ 0x9E3779B9u;
            long long spent = 0, played = 0;
            while (!table.finished()) {
                if (table.redeal()) continue;
                const bool mine = table.turn == seat;
                const auto started = std::chrono::steady_clock::now();
                const auto move = play(mine ? a : b, table, moveSeed = moveSeed * 1664525u + 1013904223u);
                if (mine) {
                    spent += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                                   started)
                                 .count();
                    ++played;
                }
                if (!table.play(move).legal) {
                    std::fprintf(stderr, "illegal move\n");
                    std::abort();
                }
            }
            const auto score = table.score();
            std::lock_guard guard(lock);
            ++result.games;
            micros += spent;
            moves += played;
            if (score[seat] > score[1 - seat])
                ++result.wins;
            else if (score[seat] == score[1 - seat])
                ++result.draws;
            else
                ++result.losses;
        }
    });
    result.score = (result.wins + .5 * result.draws) / std::max(1, result.games);
    result.milliseconds = moves ? micros / 1000.0 / moves : 0;
    return result;
}
void report(unsigned rules, const Agent& a, const Agent& b, const Result& r) {
    std::printf("rules %-5u %-14s vs %-14s score %.3f  W/D/L %d/%d/%d  %.1f ms/move\n", rules, a.name.c_str(),
                b.name.c_str(), r.score, r.wins, r.draws, r.losses, r.milliseconds);
}
void depth(unsigned rules, int deals, const Agent& agent) {
    if (agent.legacy >= 0) {
        std::fprintf(stderr, "depth needs a current agent\n");
        std::exit(2);
    }
    const auto pool = tierPool(4, 7);
    std::array<double, 9> total{};
    std::array<int, 9> count{};
    std::mutex lock;
    parallel(deals, [&](int index) {
        std::mt19937 rng(unsigned(index) * 31 + 7);
        Match table(deal(rng, pool), deal(rng, pool), rng(), rules);
        while (!table.boardComplete()) {
            const auto found = searchRoot(imagineHiddenHand(table, rng), agent.style.nodes, agent.style.plies);
            {
                std::lock_guard guard(lock);
                total[table.placed] += found.depth;
                ++count[table.placed];
            }
            table.play(found.best());
        }
    });
    for (int placed = 0; placed < 9; ++placed)
        if (count[placed])
            std::printf("rules %u %s move %d: %.2f plies of %d left\n", rules, agent.name.c_str(), placed + 1,
                        total[placed] / count[placed], 9 - placed);
}
void first(unsigned rules, int deals, const Agent& agent) {
    const auto pool = tierPool(4, 7);
    std::atomic<int> wins{0}, draws{0};
    parallel(deals, [&](int index) {
        std::mt19937 rng(unsigned(index) * 13 + 5);
        Match table(deal(rng, pool), deal(rng, pool), rng(), rules);
        const int opener = table.turn;
        while (!table.finished()) {
            if (table.redeal()) continue;
            table.play(play(agent, table, rng()));
        }
        const auto score = table.score();
        wins += score[opener] > score[1 - opener];
        draws += score[opener] == score[1 - opener];
    });
    std::printf("rules %u %s self-play: first mover %.3f (%d wins, %d draws of %d)\n", rules, agent.name.c_str(),
                (wins + .5 * draws) / deals, wins.load(), draws.load(), deals);
}
// Whole tournaments played out off screen, as when an event ends with nobody watching:
// all at once, and in the slices a background update is allowed.
void tournaments(int events) {
    double worst = 0, total = 0, slice = 0;
    int games = 0;
    for (int n = 0; n < events; ++n) {
        CollectionSave save;
        grantStarter(save);
        const auto id = newTournament(save, 1 + unsigned(n) % 10, 100 + unsigned(n), false, unsigned(n) % 3);
        if (!id) continue;
        auto* event = tournament(save, id);
        if (!event->ends) event->ends = event->starts + 24;
        auto sliced = save;
        auto started = std::chrono::steady_clock::now();
        resolveTournament(save, *event, true);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        worst = std::max(worst, ms);
        total += ms;
        for (auto* partial = tournament(sliced, id); !partial->awarded;) {
            int budget = backgroundTournamentGames;
            started = std::chrono::steady_clock::now();
            resolveTournament(sliced, *partial, true, &budget);
            slice = std::max(
                slice, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
            games += backgroundTournamentGames - std::max(0, budget);
        }
        if (encodeCollection(sliced) != encodeCollection(save)) {
            std::fprintf(stderr, "a tournament played in slices ended differently\n");
            std::exit(1);
        }
    }
    std::printf("%d tournaments off screen: %.1f games each; at once mean %.1f ms, worst %.1f ms; "
                "worst background slice of %d games %.1f ms\n",
                events, double(games) / events, total / events, worst, backgroundTournamentGames, slice);
}
int usage() {
    std::fprintf(stderr, "usage: ai-lab match|ladder|depth|first|tournament|smoke ...  (see tools/ai-lab/main.cpp)\n");
    return 2;
}
}
int main(int argc, char** argv) {
    const std::string command = argc > 1 ? argv[1] : "";
    const unsigned rules = argc > 2 ? unsigned(std::strtoul(argv[2], nullptr, 0)) : 0;
    const int deals = argc > 3 ? std::atoi(argv[3]) : 200;
    if (command == "match" && argc >= 6) {
        const auto a = parse(argv[4]), b = parse(argv[5]);
        report(rules, a, b,
               match(rules, deals, a, b, argc > 6 ? std::atoi(argv[6]) : 4, argc > 7 ? std::atoi(argv[7]) : 7));
    } else if (command == "ladder" && argc >= 4) {
        for (int level = 1; level < 6; ++level) {
            const auto a = parse(levels[level]), b = parse(levels[level - 1]);
            report(rules, a, b, match(rules, deals, a, b));
        }
    } else if (command == "depth" && argc >= 5)
        depth(rules, deals, parse(argv[4]));
    else if (command == "first" && argc >= 5)
        first(rules, deals, parse(argv[4]));
    else if (command == "tournament")
        tournaments(argc > 2 ? std::atoi(argv[2]) : 30);
    else if (command == "smoke") {
        // Enough games to tell the ends of the ladder apart, in a second or two.
        for (unsigned mask : {0u, unsigned(Open)}) {
            const auto top = parse("master"), bottom = parse("beginner");
            const auto result = match(mask, 24, top, bottom);
            report(mask, top, bottom, result);
            if (result.score < .6) {
                std::fprintf(stderr, "Master should beat Beginner\n");
                return 1;
            }
        }
    } else
        return usage();
    return 0;
}
