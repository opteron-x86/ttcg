#pragma once
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace preview::skyrim {
// Skyrim moves MenuCursor with either a mouse or the right stick. Its position
// cannot identify the device, so retain the source of the original input events.
class MenuInput {
    bool controller_ = false, pointer_ = false;
    ImVec2 cursor_{-FLT_MAX, -FLT_MAX};
    std::array<bool, ImGuiKey_Gamepad_END - ImGuiKey_Gamepad_BEGIN> held_{};
    struct Click {
        bool held = false, pointer = false;
    } accept_, inspect_;

    void pointer() {
        pointer_ = true;
        ImGui::GetIO().AddMousePosEvent(cursor_.x, cursor_.y);
        if (accept_.held && !accept_.pointer) ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadFaceDown, false);
        if (inspect_.held && !inspect_.pointer) ImGui::GetIO().AddKeyEvent(ImGuiKey_GamepadFaceUp, false);
        if (ImGui::GetCurrentContext()->ActiveIdSource == ImGuiInputSource_Gamepad) ImGui::ClearActiveID();
        ImGui::SetNavCursorVisible(false);
    }
    void navigation() {
        pointer_ = false;
        ImGui::GetIO().AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        // Release a drag when changing mode, but do not turn its still-held A
        // into an activation of the newly focused control.
        if (accept_.held && accept_.pointer) ImGui::GetIO().AddMouseButtonEvent(0, false);
        if (inspect_.held && inspect_.pointer) ImGui::GetIO().AddMouseButtonEvent(1, false);
    }
    void click(Click& click, ImGuiKey key, int button, bool down) {
        if (click.held == down) return;
        if (down) click.pointer = pointer_;
        click.held = down;
        if (click.pointer)
            ImGui::GetIO().AddMouseButtonEvent(button, down);
        else
            ImGui::GetIO().AddKeyEvent(key, down);
    }

public:
    bool controller() const { return controller_; }
    bool pointerMode() const { return pointer_; }
    void cursor(ImVec2 position) {
        cursor_ = position;
        ImGui::GetIO().AddMousePosEvent(pointer_ ? position.x : -FLT_MAX, pointer_ ? position.y : -FLT_MAX);
    }
    // The host clears ImGui's keys, mouse and event queue at the same boundary.
    void reset(bool controller) {
        controller_ = controller;
        pointer_ = !controller;
        accept_ = inspect_ = {};
        held_.fill(false);
    }
    void disconnect() {
        if (accept_.held && accept_.pointer) ImGui::GetIO().AddMouseButtonEvent(0, false);
        if (inspect_.held && inspect_.pointer) ImGui::GetIO().AddMouseButtonEvent(1, false);
        for (int key = ImGuiKey_Gamepad_BEGIN; key < ImGuiKey_Gamepad_END; ++key)
            ImGui::GetIO().AddKeyEvent(ImGuiKey(key), false);
        reset(false);
    }
    void keyboard(ImGuiKey key, bool down) {
        if (down) {
            controller_ = false;
            navigation();
        }
        if (key != ImGuiKey_None) ImGui::GetIO().AddKeyEvent(key, down);
    }
    void mouseMotion(float x, float y) {
        if (x == 0 && y == 0) return;
        controller_ = false;
        navigation();
        pointer();
    }
    void mouseButton(int button, bool down) {
        if (down) {
            controller_ = false;
            navigation();
            pointer();
        }
        ImGui::GetIO().AddMouseButtonEvent(button, down);
    }
    void wheel(float value) {
        if (value == 0) return;
        controller_ = false;
        navigation();
        pointer();
        ImGui::GetIO().AddMouseWheelEvent(0, value);
    }
    void gamepad(ImGuiKey key, float value) {
        if (key < ImGuiKey_Gamepad_BEGIN || key >= ImGuiKey_Gamepad_END) return;
        const bool down = value > .1f;
        const bool fresh = down && !held_[key - ImGuiKey_Gamepad_BEGIN];
        held_[key - ImGuiKey_Gamepad_BEGIN] = down;
        if (fresh) controller_ = true;
        if (fresh && key >= ImGuiKey_GamepadDpadLeft && key <= ImGuiKey_GamepadDpadDown) navigation();
        if (key == ImGuiKey_GamepadFaceDown)
            click(accept_, key, 0, down);
        else if (key == ImGuiKey_GamepadFaceUp)
            click(inspect_, key, 1, down);
        else if (key != ImGuiKey_None)
            ImGui::GetIO().AddKeyAnalogEvent(key, down, value);
    }
    void stick(bool left, float x, float y) {
        x = std::isfinite(x) ? x : 0;
        y = std::isfinite(y) ? y : 0;
        if (std::hypot(x, y) > .2f) {
            controller_ = true;
            if (left)
                navigation();
            else {
                pointer_ = true;
                pointer();
            }
        }
        // The right stick already drives Skyrim's visible cursor. Feeding it
        // into ImGui as well would also scroll windows and rotate inspected art.
        if (!left) return;
        const auto axis = [](ImGuiKey key, float value) {
            const float v = std::clamp((value - .2f) / .8f, 0.f, 1.f);
            ImGui::GetIO().AddKeyAnalogEvent(key, v > 0, v);
        };
        axis(ImGuiKey_GamepadLStickLeft, -x);
        axis(ImGuiKey_GamepadLStickRight, x);
        axis(ImGuiKey_GamepadLStickUp, y);
        axis(ImGuiKey_GamepadLStickDown, -y);
    }
};
}
