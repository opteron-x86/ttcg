#pragma once
#include <atomic>
#include <cstdint>

namespace preview::skyrim {
// Transitions run on the game thread. Render/input code only reads atomics.
// A queued action is valid only for the menu opening which produced it.
class Session {
public:
    enum class Phase { hidden, opening, visible, closing };
private:
    std::atomic<Phase> state{Phase::hidden};
    std::atomic<std::uint64_t> generation{0};
public:
    Phase phase() const { return state.load(); }
    std::uint64_t token() const { return generation.load(); }
    bool active() const { return phase()==Phase::visible; }
    bool accepts(std::uint64_t value) const { return active() && token()==value; }
    bool open() {
        if(phase()!=Phase::hidden)return false;
        ++generation;state=Phase::opening;return true;
    }
    bool shown() {
        if(phase()==Phase::visible)return true;
        if(phase()!=Phase::opening)return false;
        state=Phase::visible;return true;
    }
    bool close() {
        if(phase()==Phase::hidden||phase()==Phase::closing)return false;
        ++generation;state=Phase::closing;return true;
    }
    void hidden() { ++generation;state=Phase::hidden; }
};
}
