#pragma once
#include <imgui.h>
#include <cstdint>

namespace preview::skyrim {
inline ImGuiKey keyboardKey(std::uint32_t code) {
    struct Key { std::uint32_t scan; ImGuiKey key; };
    static constexpr Key keys[]{
        {1,ImGuiKey_Escape},{14,ImGuiKey_Backspace},{15,ImGuiKey_Tab},{28,ImGuiKey_Enter},
        {29,ImGuiKey_LeftCtrl},{42,ImGuiKey_LeftShift},{54,ImGuiKey_RightShift},{56,ImGuiKey_LeftAlt},
        {57,ImGuiKey_Space},{156,ImGuiKey_KeypadEnter},{157,ImGuiKey_RightCtrl},{184,ImGuiKey_RightAlt},
        {199,ImGuiKey_Home},{200,ImGuiKey_UpArrow},{201,ImGuiKey_PageUp},{203,ImGuiKey_LeftArrow},
        {205,ImGuiKey_RightArrow},{207,ImGuiKey_End},{208,ImGuiKey_DownArrow},{209,ImGuiKey_PageDown},
        {210,ImGuiKey_Insert},{211,ImGuiKey_Delete},
        {30,ImGuiKey_A},{48,ImGuiKey_B},{46,ImGuiKey_C},{32,ImGuiKey_D},{18,ImGuiKey_E},
        {33,ImGuiKey_F},{34,ImGuiKey_G},{35,ImGuiKey_H},{23,ImGuiKey_I},{36,ImGuiKey_J},
        {37,ImGuiKey_K},{38,ImGuiKey_L},{50,ImGuiKey_M},{49,ImGuiKey_N},{24,ImGuiKey_O},
        {25,ImGuiKey_P},{16,ImGuiKey_Q},{19,ImGuiKey_R},{31,ImGuiKey_S},{20,ImGuiKey_T},
        {22,ImGuiKey_U},{47,ImGuiKey_V},{17,ImGuiKey_W},{45,ImGuiKey_X},{21,ImGuiKey_Y},{44,ImGuiKey_Z},
        {11,ImGuiKey_0},{2,ImGuiKey_1},{3,ImGuiKey_2},{4,ImGuiKey_3},{5,ImGuiKey_4},
        {6,ImGuiKey_5},{7,ImGuiKey_6},{8,ImGuiKey_7},{9,ImGuiKey_8},{10,ImGuiKey_9},
        {12,ImGuiKey_Minus},{13,ImGuiKey_Equal},{26,ImGuiKey_LeftBracket},{27,ImGuiKey_RightBracket},
        {39,ImGuiKey_Semicolon},{40,ImGuiKey_Apostrophe},{41,ImGuiKey_GraveAccent},{43,ImGuiKey_Backslash},
        {51,ImGuiKey_Comma},{52,ImGuiKey_Period},{53,ImGuiKey_Slash}
    };
    if(code>=59&&code<=68)return ImGuiKey(ImGuiKey_F1+code-59);
    if(code==87)return ImGuiKey_F11;
    if(code==88)return ImGuiKey_F12;
    for(const auto& key:keys)if(key.scan==code)return key.key;
    return ImGuiKey_None;
}
inline ImGuiKey controllerKey(std::uint32_t code) {
    switch(code) {
    case 1:return ImGuiKey_GamepadDpadUp;case 2:return ImGuiKey_GamepadDpadDown;
    case 4:return ImGuiKey_GamepadDpadLeft;case 8:return ImGuiKey_GamepadDpadRight;
    case 16:return ImGuiKey_GamepadStart;case 32:return ImGuiKey_GamepadBack;
    case 64:return ImGuiKey_GamepadL3;case 128:return ImGuiKey_GamepadR3;
    case 256:return ImGuiKey_GamepadL1;case 512:return ImGuiKey_GamepadR1;
    case 4096:return ImGuiKey_GamepadFaceDown;case 8192:return ImGuiKey_GamepadFaceRight;
    case 16384:return ImGuiKey_GamepadFaceLeft;case 32768:return ImGuiKey_GamepadFaceUp;
    case 9:return ImGuiKey_GamepadL2;case 10:return ImGuiKey_GamepadR2;
    default:return ImGuiKey_None;
    }
}
}
