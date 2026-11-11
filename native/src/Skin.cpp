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
        s.ground = alpha(s.palette.background, .96f);
        s.dialog = s.ground;
        if (j.contains("ground") && j["ground"].is_object()) {
            const auto& g = j["ground"];
            s.ground = colour(g, "fill", s.ground);
            s.dialog = colour(g, "dialog", s.dialog);
            s.border = g.value("border", true);
            s.ruleSelector = g.value("selector", std::string()) == "rule";
            const auto corner = g.value("corner", std::string());
            s.corner = corner == "key" ? 1 : corner == "dot" ? 2 : 0;
            s.ornament = g.value("rule", std::string()) == "diamond";
        }
        constexpr std::string_view boards[]{"", "plain", "slate", "felt", "parchment", "leather", "walnut", "slab"};
        for (int board = 1; board < int(std::size(boards)); ++board)
            if (j.value("board", std::string()) == boards[board]) s.board = board;
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
                art.scale = std::clamp(entry.value("scale", 1.f), .25f, 8.f);
                art.pad = std::clamp(entry.value("pad", 0.f), 0.f, 256.f);
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
void App::skinPanel(ImVec2 at, ImVec2 size, float density, ImU32 tint, const char* name) {
    auto* d = ImGui::GetWindowDrawList();
    const bool main = std::string_view(name) == "window";
    if (main && !skinPiece(name)) name = "panel";
    const auto* piece = skinPiece(name);
    const auto art = skinArt(name);
    if (!art.id || piece->margin <= 0) {
        const auto& s = skin();
        const ImVec2 end = at + size;
        d->AddRectFilled(at, end, main ? s.ground : s.dialog);
        if (s.border) d->AddRect(at, end, palette().line);
        const auto ink = alpha(palette().muted, .62f);
        const float u = 3.2f * scale, weight = std::max(1.f, 1.3f * scale);
        for (int i = 0; i < 4 && s.corner; ++i) {
            const ImVec2 sign(i % 2 ? -1.f : 1.f, i / 2 ? -1.f : 1.f);
            const ImVec2 origin =
                ImVec2(i % 2 ? end.x : at.x, i / 2 ? end.y : at.y) + sign * (s.corner == 1 ? 4 * scale : 0);
            const auto point = [&](float x, float y) { return origin + ImVec2(x * sign.x, y * sign.y) * u; };
            if (s.corner == 2) {
                d->AddCircleFilled(origin, 2.4f * scale, alpha(palette().ink, .85f));
                continue;
            }
            // A meander bracket, the corner mark of Nordic-style panels.
            const ImVec2 outer[]{point(0, 6), point(0, 0), point(6, 0)};
            const ImVec2 inner[]{point(2, 6), point(2, 2), point(6, 2)};
            const ImVec2 hook[]{point(4, 6), point(4, 4), point(6, 4)};
            d->AddPolyline(outer, 3, ink, 0, weight);
            d->AddPolyline(inner, 3, ink, 0, weight);
            d->AddPolyline(hook, 3, ink, 0, weight);
        }
        return;
    }
    // The frame line sits `inset` pixels inside the texture edge and lands on
    // the requested rectangle.
    const float inset = piece->inset + piece->pad, margin = piece->margin;
    // A piece may be translucent, like Edge's inventory ground over the blurred scene.
    tint = (tint & 0xffffff) | (ImU32(((tint >> 24) & 255) * piece->opacity) << 24);
    const float k = std::min({skinUnit() * density * piece->scale, size.x / (2.4f * margin), size.y / (2.4f * margin)});
    nineSlice(art, at - ImVec2(inset, inset) * k, size + ImVec2(inset, inset) * (2 * k), margin, k, piece->tile, tint);
}
void App::nineSlice(Texture art, ImVec2 a, ImVec2 size, float margin, float k, bool tile, ImU32 tint) {
    // Corners keep their detail; edges stretch, or repeat mirrored so torn strokes never smear.
    auto* d = ImGui::GetWindowDrawList();
    const ImVec2 b = a + size;
    const float m = margin * k, u = margin / art.width, v = margin / art.height;
    const float xs[]{a.x, a.x + m, b.x - m, b.x}, ys[]{a.y, a.y + m, b.y - m, b.y};
    const float us[]{0, u, 1 - u, 1}, vs[]{0, v, 1 - v, 1};
    const auto tiles = [&](float length, float source) {
        if (!tile) return 1;
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
    const auto ink = alpha(palette().accent, .59f * std::clamp(strength, 0.f, 1.7f)), clear = ink & 0xffffff;
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
App::GridEdges App::gridScroll(float rowHeight, bool reset) {
    auto& target = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("row-scroll"), -1);
    const float limit = ImGui::GetScrollMaxY(), wheel = ImGui::GetIO().MouseWheel;
    if (reset) {
        target = -1;
        ImGui::SetScrollY(0);
    }
    if (wheel != 0 && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
        !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup)) {
        const float from = target >= 0 ? target : ImGui::GetScrollY();
        const float rows = std::round(from / rowHeight) - (wheel > 0 ? 1 : -1);
        target = std::clamp(rows * rowHeight, 0.f, limit);
    }
    if (target >= 0) {
        const float current = ImGui::GetScrollY(),
                    step = (target - current) * std::min(1.f, ImGui::GetIO().DeltaTime * 16);
        if (std::abs(target - current) < 1) {
            ImGui::SetScrollY(target);
            target = -1;
        } else
            ImGui::SetScrollY(current + step);
    }
    return {ImGui::GetScrollY() > 2, ImGui::GetScrollY() < limit - 2};
}
void App::gridFade(GridEdges edges) {
    auto* d = ImGui::GetWindowDrawList();
    const ImVec2 at = ImGui::GetWindowPos(),
                 size = ImGui::GetWindowSize() - ImVec2(ImGui::GetStyle().ScrollbarSize + 4 * scale, 0);
    const ImU32 solid = alpha(skinned() ? skin().dialog : palette().background, .96f), clear = solid & 0xffffff;
    const float reach = 64 * scale;
    if (edges.above)
        d->AddRectFilledMultiColor(at, ImVec2(at.x + size.x, at.y + reach * .6f), solid, solid, clear, clear);
    if (edges.below)
        d->AddRectFilledMultiColor(ImVec2(at.x, at.y + size.y - reach), at + size, clear, clear, solid, solid);
}
void App::artImage(Texture texture, ImVec2 a, ImVec2 b, ImVec2 uv0, ImVec2 uv1) {
    auto* d = ImGui::GetWindowDrawList();
    const int n = std::max(1, artGrid);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const float x0 = float(x) / n, x1 = float(x + 1) / n, y0 = float(y) / n, y1 = float(y + 1) / n;
            d->AddImage(texture.id, ImVec2(a.x + (b.x - a.x) * x0, a.y + (b.y - a.y) * y0),
                        ImVec2(a.x + (b.x - a.x) * x1, a.y + (b.y - a.y) * y1),
                        ImVec2(uv0.x + (uv1.x - uv0.x) * x0, uv0.y + (uv1.y - uv0.y) * y0),
                        ImVec2(uv0.x + (uv1.x - uv0.x) * x1, uv0.y + (uv1.y - uv0.y) * y1));
        }
}
void App::softShadow(ImVec2 at, ImVec2 size, float strength, float reach) {
    auto* d = ImGui::GetWindowDrawList();
    const ImVec2 drop(0, reach * .35f * scale);
    constexpr int layers = 7;
    for (int i = layers; i > 0; --i) {
        const float r = reach * scale * i / layers, weight = 1 - float(i) / (layers + 1);
        d->AddRectFilled(at - ImVec2(r, r) + drop, at + size + ImVec2(r, r) + drop,
                         IM_COL32(0, 0, 0, int(255 * .11f * strength * weight)), r + 2 * scale);
    }
}
void App::glow(ImVec2 at, ImVec2 size, ImU32 colour, float strength, float reach) {
    if (strength <= 0) return;
    auto* d = ImGui::GetWindowDrawList();
    const int layers = std::max(4, int(reach));
    for (int i = layers; i > 0; --i) {
        const float r = reach * scale * i / layers;
        d->AddRect(at - ImVec2(r, r), at + size + ImVec2(r, r),
                   alpha(colour, strength * .09f * (1 - float(i) / (layers + 1))), r, 0, 2.2f * scale);
    }
}
void App::cornerMarks(ImVec2 at, ImVec2 size, ImU32 colour, float length, float offset) {
    auto* d = ImGui::GetWindowDrawList();
    const float l = length * scale, o = offset * scale, weight = std::max(1.5f, 2 * scale);
    for (int i = 0; i < 4; ++i) {
        const ImVec2 sign(i % 2 ? -1.f : 1.f, i / 2 ? -1.f : 1.f);
        const ImVec2 corner = ImVec2(i % 2 ? at.x + size.x + o : at.x - o, i / 2 ? at.y + size.y + o : at.y - o);
        const ImVec2 points[]{corner + ImVec2(l * sign.x, 0), corner, corner + ImVec2(0, l * sign.y)};
        d->AddPolyline(points, 3, colour, 0, weight);
    }
}
void App::fadeVertices(int firstVertex, float amount) {
    auto* d = ImGui::GetWindowDrawList();
    amount = std::clamp(amount, 0.f, 1.f);
    for (int i = firstVertex; i < d->VtxBuffer.Size; ++i) {
        auto& colour = d->VtxBuffer[i].col;
        colour = (colour & ~IM_COL32_A_MASK) |
                 (unsigned(((colour & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT) * amount) << IM_COL32_A_SHIFT);
    }
}
void App::scaleVertices(int firstVertex, ImVec2 centre, ImVec2 factor) {
    auto* d = ImGui::GetWindowDrawList();
    for (int i = firstVertex; i < d->VtxBuffer.Size; ++i) {
        auto& pos = d->VtxBuffer[i].pos;
        pos = centre + ImVec2((pos.x - centre.x) * factor.x, (pos.y - centre.y) * factor.y);
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
    size = std::max(size, AlbumLayout::smallestCaps);
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
    size = std::max(size, AlbumLayout::smallestCaps);
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
    if (hostInputDevice) return;
    // Prompts follow whichever device the player touched last.
    const auto& io = ImGui::GetIO();
    for (int key = ImGuiKey_Gamepad_BEGIN; key < ImGuiKey_Gamepad_END; ++key)
        if (ImGui::IsKeyPressed(ImGuiKey(key), false)) padPrompts = true;
    if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0 || ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))
        padPrompts = false;
    for (int key = ImGuiKey_Keyboard_BEGIN; key < ImGuiKey_Keyboard_END; ++key)
        if (ImGui::IsKeyPressed(ImGuiKey(key), false)) padPrompts = false;
}
float App::keyPrompt(const char* key, const char* pad, const char* label, ImVec2 at, bool* pressed, const char* id) {
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
        *pressed = ImGui::InvisibleButton(id ? id : label, ImVec2(width, height));
        hover = ImGui::IsItemHovered();
    }
    if (art.id)
        // Icon packs are drawn light for dark interfaces; on paper they take the ink's colour.
        d->AddImage(art.id, at, at + ImVec2(keyWidth, height), ImVec2(0, 0), ImVec2(1, 1),
                    lightGround() ? palette().ink : IM_COL32_WHITE);
    else {
        const ImVec2 a = at + ImVec2(0, 2 * scale), b = at + ImVec2(keyWidth, height - 2 * scale);
        d->AddRectFilled(a, b, lightGround() ? alpha(palette().ink, .82f) : IM_COL32(0, 0, 0, 120), 3 * scale);
        d->AddRect(a, b, alpha(palette().accent, .55f), 3 * scale);
        d->AddText(font(), 15 * scale, at + ImVec2((keyWidth - captionWidth) * .5f, 7 * scale),
                   lightGround() ? IM_COL32(244, 240, 232, 255) : palette().ink, caption.c_str());
    }
    d->AddText(font(), 18 * scale, at + ImVec2(keyWidth + 9 * scale, 5 * scale),
               hover ? palette().ink : alpha(palette().ink, .78f), label);
    return width;
}
}
