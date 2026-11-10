#pragma once
namespace preview::skyrim {
enum class Interaction { hidden, active, suspended, lost };
// Losing the desktop foreground (or opening the console) suspends input. Only
// removal of our menu or another game menu taking ownership ends the session.
inline Interaction interaction(bool visible, bool menuOpen, bool foreign, bool foreground, bool console) {
    if (!visible) return Interaction::hidden;
    if (!menuOpen || foreign) return Interaction::lost;
    return foreground && !console ? Interaction::active : Interaction::suspended;
}
}
