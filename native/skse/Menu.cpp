#include "Host.h"
#include "BuildInfo.h"

static_assert(TESSERA_COMMONLIB_BSTHASHMAP_LIFETIME_PATCH == 1);

namespace preview::skyrim {
namespace {
class Menu final : public RE::IMenu {
public:
    Menu() {
        depthPriority = 11;
        if (Host::get().pauses()) menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame);
        menuFlags.set(RE::UI_MENU_FLAGS::kUsesCursor, RE::UI_MENU_FLAGS::kUpdateUsesCursor,
                      RE::UI_MENU_FLAGS::kUsesMenuContext, RE::UI_MENU_FLAGS::kCustomRendering,
                      RE::UI_MENU_FLAGS::kDisablePauseMenu, RE::UI_MENU_FLAGS::kDontHideCursorWhenTopmost);
        inputContext.set(RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
        // Blur the scene as Skyrim's own menus do, for as long as this menu exists.
        if (auto* blur = RE::UIBlurManager::GetSingleton()) {
            blur->IncrementBlurCount();
            blurred = true;
        }
    }
    ~Menu() override {
        if (blurred)
            if (auto* blur = RE::UIBlurManager::GetSingleton()) blur->DecrementBlurCount();
    }
    void RefreshPlatform() override {
        // Native widgets support mouse and controller together. The base
        // Scaleform policy hides the cursor when a controller is enabled.
        Host::get().refreshCursor();
    }
    void PostDisplay() override { Host::get().draw(); }
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& message) override {
        switch (*message.type) {
            case RE::UI_MESSAGE_TYPE::kShow:
                Host::get().shown();
                RefreshPlatform();
                break;
            case RE::UI_MESSAGE_TYPE::kHide:
            case RE::UI_MESSAGE_TYPE::kForceHide: Host::get().hidden(); break;
            case RE::UI_MESSAGE_TYPE::kScaleformEvent:
            case RE::UI_MESSAGE_TYPE::kUserEvent:
                // Raw input drives the native widgets, including nested Back/Escape.
                return RE::UI_MESSAGE_RESULTS::kHandled;
            default: break;
        }
        return RE::IMenu::ProcessMessage(message);
    }
    static RE::IMenu* create() { return new Menu; }

private:
    bool blurred = false;
};
}
void registerMenu() {
    auto* ui = RE::UI::GetSingleton();
    if constexpr (sizeof(TTCG_BUILD_LABEL) > 1) {
        std::size_t live = 0;
        for (const auto& entry : ui->menuMap)
            if (entry.second.menu) ++live;
        SKSE::log::info("Tessera menu registration: entries before={}, live menus={}", ui->menuMap.size(), live);
    }
    ui->Register(menuName, Menu::create);
    if constexpr (sizeof(TTCG_BUILD_LABEL) > 1)
        SKSE::log::info("Tessera menu registration: entries after={}", ui->menuMap.size());
}
}
