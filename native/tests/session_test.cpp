#include "Session.h"
#include "Keys.h"
#include "Interaction.h"
#include <stdexcept>
#include <iostream>

void require(bool okay) {
    if (!okay) throw std::runtime_error("Native menu lifecycle/input regression");
}
int main() {
    using namespace preview::skyrim;
    Session s;
    require(!s.active() && !s.shown() && !s.close());
    require(s.open() && !s.open());
    auto pending = s.token();
    require(!s.accepts(pending));
    require(s.shown() && s.shown() && s.accepts(pending));
    require(s.close() && !s.close() && !s.accepts(pending) && !s.shown() && !s.open());
    s.hidden();
    require(s.open());
    auto next = s.token();
    require(next != pending && s.shown() && s.accepts(next) && !s.accepts(pending));
    s.close();
    s.hidden();
    require(s.open());
    // Loading/canceling before Show must not reactivate a queued menu opening.
    s.close();
    require(!s.shown());
    s.hidden();
    for (int i = 0; i < 1000; ++i) {
        require(s.open() && s.shown());
        auto token = s.token();
        require(s.close());
        s.hidden();
        require(!s.accepts(token));
    }
    require(keyboardKey(87) == ImGuiKey_F11 && keyboardKey(1) == ImGuiKey_Escape);
    require(keyboardKey(30) == ImGuiKey_A && keyboardKey(203) == ImGuiKey_LeftArrow);
    require(controllerKey(4096) == ImGuiKey_GamepadFaceDown && controllerKey(8192) == ImGuiKey_GamepadFaceRight);
    require(keyboardKey(255) == ImGuiKey_None && controllerKey(999) == ImGuiKey_None);
    // Alt-Tab and console input leave the menu generation intact. Returning to
    // Skyrim resumes the same session; another game menu still interrupts it.
    require(s.open() && s.shown());
    const auto beforeTab = s.token();
    for (bool console : {false, true}) {
        require(interaction(s.active(), true, false, false, console) == Interaction::suspended);
        require(s.accepts(beforeTab));
        require(interaction(s.active(), true, false, true, console) ==
                (console ? Interaction::suspended : Interaction::active));
    }
    require(interaction(s.active(), true, true, true, false) == Interaction::lost);
    require(interaction(s.active(), false, false, false, false) == Interaction::lost);
    s.close();
    s.hidden();
    require(interaction(s.active(), false, false, true, false) == Interaction::hidden);
    std::cout
        << "Native menu lifecycle: repeated opens, close-before-show, stale input/commands and key mapping passed\n";
}
