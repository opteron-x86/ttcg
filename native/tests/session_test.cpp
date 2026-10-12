#include "Session.h"
#include "Keys.h"
#include <stdexcept>
#include <iostream>

void require(bool okay) { if(!okay)throw std::runtime_error("Native menu lifecycle/input regression"); }
int main() {
    using namespace preview::skyrim;
    Session s;
    require(!s.active()&&!s.shown()&&!s.close());
    require(s.open()&&!s.open());
    auto pending=s.token();require(!s.accepts(pending));
    require(s.shown()&&s.shown()&&s.accepts(pending));
    require(s.close()&&!s.close()&&!s.accepts(pending)&&!s.shown()&&!s.open());
    s.hidden();require(s.open());auto next=s.token();
    require(next!=pending&&s.shown()&&s.accepts(next)&&!s.accepts(pending));
    s.close();s.hidden();require(s.open());
    // Loading/canceling before Show must not reactivate a queued menu opening.
    s.close();require(!s.shown());s.hidden();
    for(int i=0;i<1000;++i) {
        require(s.open()&&s.shown());auto token=s.token();require(s.close());s.hidden();require(!s.accepts(token));
    }
    require(keyboardKey(87)==ImGuiKey_F11&&keyboardKey(1)==ImGuiKey_Escape);
    require(keyboardKey(30)==ImGuiKey_A&&keyboardKey(203)==ImGuiKey_LeftArrow);
    require(controllerKey(4096)==ImGuiKey_GamepadFaceDown&&controllerKey(8192)==ImGuiKey_GamepadFaceRight);
    require(keyboardKey(255)==ImGuiKey_None&&controllerKey(999)==ImGuiKey_None);
    std::cout<<"Native menu lifecycle: repeated opens, close-before-show, stale input/commands and key mapping passed\n";
}
