#include "App.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace preview {
namespace {
constexpr ImU32 rgb(unsigned n, unsigned alpha = 255) { return IM_COL32(n >> 16, (n >> 8) & 255, n & 255, alpha); }
ImU32 alpha(ImU32 colour, float amount) {
    return (colour & 0xffffff) | (unsigned(std::clamp(amount, 0.f, 1.f) * 255) << 24);
}
unsigned upper(unsigned c) {
    if ((c >= 'a' && c <= 'z') || (c >= 0xe0 && c <= 0xf6) || (c >= 0xf8 && c <= 0xfe)) return c - 32;
    return c;
}
// "#rrggbb" or "#rrggbbaa"; anything else keeps the fallback.
ImU32 colour(const Json& j, const char* key, ImU32 fallback) {
    if (!j.is_object() || !j.contains(key) || !j[key].is_string()) return fallback;
    const auto text = j[key].get<std::string>();
    if ((text.size() != 7 && text.size() != 9) || text[0] != '#') return fallback;
    unsigned value = 0;
    for (std::size_t i = 1; i < text.size(); ++i) {
        const char c = text[i];
        const int digit = c >= '0' && c <= '9'   ? c - '0'
                          : c >= 'a' && c <= 'f' ? c - 'a' + 10
                          : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                 : -1;
        if (digit < 0) return fallback;
        value = value * 16 + unsigned(digit);
    }
    return text.size() == 7 ? rgb(value) : rgb(value >> 8, value & 255);
}
unsigned plain(ImU32 colour) { return ((colour & 255) << 16) | (colour & 0xff00) | ((colour >> 16) & 255); }
}
void App::loadSkins() {
    for (int index = 0; index < ttcg::interfaceThemeCount; ++index) {
        const std::string id(ttcg::interfaceThemes[index].id);
        const auto folder = options.tesseraRoot / "TesseraUI/skins" / id;
        std::ifstream file(folder / "skin.json");
        if (!file) continue;
        Json j = Json::parse(file, nullptr, false);
        if (!j.is_object() || j.value("schema", 0) != 1 || j.value("id", std::string()) != id) {
            std::cerr << "Skin ignored: " << id << '\n';
            continue;
        }
        auto& s = skins[index];
        s.palette = builtInPalette(index);
        s.palette.frame = FrameStyle::Skinned;
        const auto& p = j.contains("palette") ? j["palette"] : Json();
        s.palette.background = colour(p, "background", s.palette.background);
        s.palette.centre = colour(p, "centre", s.palette.centre);
        s.palette.ink = colour(p, "text", s.palette.ink);
        s.palette.muted = colour(p, "muted", s.palette.muted);
        s.palette.accent = colour(p, "accent", s.palette.accent);
        s.palette.line = colour(p, "line", s.palette.line);
        s.palette.selection = colour(p, "selection", s.palette.selection);
        s.value = colour(p, "value", s.palette.accent);
        s.notice = colour(p, "notice", s.notice);
        if (j.contains("control")) {
            s.player = plain(colour(j["control"], "player", 0));
            s.opponent = plain(colour(j["control"], "opponent", 0));
        }
        s.font = j.value("font", std::string("jost"));
        if (j.contains("art") && j["art"].is_object())
            for (const auto& [name, entry] : j["art"].items()) {
                if (!entry.is_object()) continue;
                const auto texture = entry.value("file", std::string());
                // Textures stay inside the skin's own folder.
                if (texture.empty() || texture.find_first_of("/\\") != std::string::npos) continue;
                SkinArt art;
                art.path = "../skins/" + id + "/" + texture;
                art.margin = std::clamp(entry.value("margin", 0.f), 0.f, 2048.f);
                art.inset = std::clamp(entry.value("inset", 0.f), 0.f, 512.f);
                art.cap = std::clamp(entry.value("cap", .1f), 0.f, .5f);
                art.opacity = std::clamp(entry.value("opacity", 1.f), 0.f, 1.f);
                art.tile = entry.value("tile", false);
                art.passes = std::clamp(entry.value("passes", 1), 1, 4);
                s.art.emplace(name, std::move(art));
            }
        s.loaded = true;
    }
}
float App::skinUnit() const { return ImGui::GetIO().DisplaySize.y / 2160.f; }
const SkinArt* App::skinPiece(const char* name) const {
    const auto it = skin().art.find(name);
    return it == skin().art.end() ? nullptr : &it->second;
}
Texture App::skinArt(const char* name) {
    const auto* piece = skinPiece(name);
    return piece ? assets.optional(piece->path) : Texture{};
}
void App::skinPanel(ImVec2 at, ImVec2 size, float density, ImU32 tint) {
    auto* d = ImGui::GetWindowDrawList();
    const auto* piece = skinPiece("panel");
    const auto art = skinArt("panel");
    if (!art.id || piece->margin <= 0) {
        d->AddRectFilled(at, at + size, alpha(palette().background, .96f));
        d->AddRect(at, at + size, palette().line);
        return;
    }
    // The frame line sits `inset` pixels inside the texture edge and lands on
    // the requested rectangle. Corners keep their detail; edges stretch, or
    // repeat mirrored so torn strokes never smear.
    const float inset = piece->inset, margin = piece->margin;
    const float k = std::min({skinUnit() * density, size.x / (2.4f * margin), size.y / (2.4f * margin)});
    const ImVec2 a = at - ImVec2(inset, inset) * k, b = at + size + ImVec2(inset, inset) * k;
    const float m = margin * k, u = margin / art.width, v = margin / art.height;
    const float xs[]{a.x, a.x + m, b.x - m, b.x}, ys[]{a.y, a.y + m, b.y - m, b.y};
    const float us[]{0, u, 1 - u, 1}, vs[]{0, v, 1 - v, 1};
    const auto tiles = [&](float length, float source) {
        if (!piece->tile) return 1;
        const int n = std::max(1, int(std::round(length / (source * k))));
        return n | 1;
    };
    const int columns = tiles(xs[2] - xs[1], art.width - 2 * margin),
              rows = tiles(ys[2] - ys[1], art.height - 2 * margin);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column) {
            const int nx = column == 1 && row != 1 ? columns : 1, ny = row == 1 && column != 1 ? rows : 1;
            const float w = (xs[column + 1] - xs[column]) / nx, h = (ys[row + 1] - ys[row]) / ny;
            for (int j = 0; j < ny; ++j)
                for (int i = 0; i < nx; ++i) {
                    const ImVec2 p(xs[column] + i * w, ys[row] + j * h);
                    const bool flipX = i % 2 != 0, flipY = j % 2 != 0;
                    d->AddImage(art.id, p, p + ImVec2(w, h),
                                ImVec2(us[column + (flipX ? 1 : 0)], vs[row + (flipY ? 1 : 0)]),
                                ImVec2(us[column + (flipX ? 0 : 1)], vs[row + (flipY ? 0 : 1)]), tint);
                }
        }
}
bool App::skinBand(const char* name, ImVec2 at, ImVec2 size, ImU32 tint) {
    const auto* piece = skinPiece(name);
    const auto art = skinArt(name);
    if (!art.id) return false;
    auto* d = ImGui::GetWindowDrawList();
    // Ends keep their brush taper or ornament; only the middle stretches.
    const float cap = piece->cap, end = std::min(cap * art.width * size.y / art.height, size.x * .45f);
    for (int pass = 0; pass < piece->passes; ++pass) {
        d->AddImage(art.id, at, at + ImVec2(end, size.y), ImVec2(0, 0), ImVec2(cap, 1), tint);
        d->AddImage(art.id, at + ImVec2(end, 0), at + ImVec2(size.x - end, size.y), ImVec2(cap, 0), ImVec2(1 - cap, 1),
                    tint);
        d->AddImage(art.id, at + ImVec2(size.x - end, 0), at + size, ImVec2(1 - cap, 0), ImVec2(1, 1), tint);
    }
    return true;
}
void App::skinRule(ImVec2 a, ImVec2 b, float strength) {
    auto* d = ImGui::GetWindowDrawList();
    const float weight = std::max(1.f, std::round(2.1f * skinUnit()));
    const auto ink = alpha(palette().accent, .59f * std::clamp(strength, 0.f, 1.f)), clear = ink & 0xffffff;
    if (a.y == b.y) {
        const float fade = (b.x - a.x) * .18f;
        d->AddRectFilledMultiColor(a, ImVec2(a.x + fade, a.y + weight), clear, ink, ink, clear);
        d->AddRectFilled(ImVec2(a.x + fade, a.y), ImVec2(b.x - fade, a.y + weight), ink);
        d->AddRectFilledMultiColor(ImVec2(b.x - fade, a.y), ImVec2(b.x, a.y + weight), ink, clear, clear, ink);
    } else {
        const float fade = (b.y - a.y) * .18f;
        d->AddRectFilledMultiColor(a, ImVec2(a.x + weight, a.y + fade), clear, clear, ink, ink);
        d->AddRectFilled(ImVec2(a.x, a.y + fade), ImVec2(a.x + weight, b.y - fade), ink);
        d->AddRectFilledMultiColor(ImVec2(a.x, b.y - fade), ImVec2(a.x + weight, b.y), ink, ink, clear, clear);
    }
}
void App::skinBackdrop() {
    // Skyrim blurs the scene behind its own menus; this adds the matching dim.
    const auto display = ImGui::GetIO().DisplaySize;
    auto* d = ImGui::GetBackgroundDrawList();
    d->AddRectFilled(ImVec2(0, 0), display, IM_COL32(0, 0, 0, 96));
    const auto edge = IM_COL32(0, 0, 0, 120), clear = IM_COL32(0, 0, 0, 0);
    d->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(display.x, display.y * .22f), edge, edge, clear, clear);
    d->AddRectFilledMultiColor(ImVec2(0, display.y * .78f), display, clear, clear, edge, edge);
}
float App::capsWidth(const std::string& value, float size, float tracking) const {
    float width = 0;
    const char* p = value.c_str();
    const char* end = p + value.size();
    while (p < end) {
        unsigned c = 0;
        p += ImTextCharFromUtf8(&c, p, end);
        char utf8[5]{};
        ImTextCharToUtf8(utf8, upper(c));
        width += font()->CalcTextSizeA(size * scale, 1e5f, 0, utf8).x + tracking * scale;
    }
    return width - (value.empty() ? 0 : tracking * scale);
}
void App::textCaps(ImVec2 at, const std::string& value, float size, ImU32 colour, float tracking, bool centre) {
    if (centre) at.x -= capsWidth(value, size, tracking) * .5f;
    auto* d = ImGui::GetWindowDrawList();
    const char* p = value.c_str();
    const char* end = p + value.size();
    while (p < end) {
        unsigned c = 0;
        p += ImTextCharFromUtf8(&c, p, end);
        char utf8[5]{};
        ImTextCharToUtf8(utf8, upper(c));
        d->AddText(font(), size * scale, at, colour, utf8);
        at.x += font()->CalcTextSizeA(size * scale, 1e5f, 0, utf8).x + tracking * scale;
    }
}
void App::trackInputDevice() {
    // Prompts follow whichever device the player touched last.
    const auto& io = ImGui::GetIO();
    for (int key = ImGuiKey_Gamepad_BEGIN; key < ImGuiKey_Gamepad_END; ++key)
        if (ImGui::IsKeyPressed(ImGuiKey(key), false)) padPrompts = true;
    if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0 || ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))
        padPrompts = false;
    for (int key = ImGuiKey_Keyboard_BEGIN; key < ImGuiKey_Keyboard_END; ++key)
        if (ImGui::IsKeyPressed(ImGuiKey(key), false)) padPrompts = false;
}
float App::keyPrompt(const char* key, const char* pad, const char* label, ImVec2 at, bool* pressed) {
    auto* d = ImGui::GetWindowDrawList();
    const char* name = padPrompts ? pad : key;
    const auto folder =
        options.keyIcons.empty() ? options.tesseraRoot / "Interface/ImGuiIcons/Icons" : options.keyIcons;
    const auto file = (folder / (std::string(name) + ".png")).u8string();
    const auto art = assets.optional(std::string(file.begin(), file.end()));
    const float height = 30 * scale;
    // Without an icon pack, draw the key's own name on a plain cap.
    std::string caption = name;
    if (const auto split = caption.find('_'); split != std::string::npos) caption.erase(0, split + 1);
    if (caption.starts_with("Mouse")) caption = "M" + caption.substr(5);
    const float captionWidth = font()->CalcTextSizeA(15 * scale, 1e5f, 0, caption.c_str()).x;
    const float keyWidth = art.id ? height * art.width / art.height : std::max(height, captionWidth + 14 * scale);
    const float labelWidth = font()->CalcTextSizeA(18 * scale, 1e5f, 0, label).x;
    const float width = keyWidth + 9 * scale + labelWidth;
    bool hover = false;
    if (pressed) {
        ImGui::SetCursorScreenPos(at);
        *pressed = ImGui::InvisibleButton(label, ImVec2(width, height));
        hover = ImGui::IsItemHovered();
    }
    if (art.id)
        d->AddImage(art.id, at, at + ImVec2(keyWidth, height));
    else {
        const ImVec2 a = at + ImVec2(0, 2 * scale), b = at + ImVec2(keyWidth, height - 2 * scale);
        d->AddRectFilled(a, b, IM_COL32(0, 0, 0, 120), 3 * scale);
        d->AddRect(a, b, alpha(palette().accent, .55f), 3 * scale);
        d->AddText(font(), 15 * scale, at + ImVec2((keyWidth - captionWidth) * .5f, 7 * scale), palette().ink,
                   caption.c_str());
    }
    d->AddText(font(), 18 * scale, at + ImVec2(keyWidth + 9 * scale, 5 * scale),
               hover ? palette().ink : alpha(palette().ink, .78f), label);
    return width;
}
}
