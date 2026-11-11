#include "App.h"

namespace preview {
namespace {
const char* packName(unsigned kind) { return kind ? "Tournament pack" : "Card pack"; }
void fitImage(Texture texture, ImVec2 at, ImVec2 size) {
    if (!texture.id) return;
    const float zoom = std::min(size.x / texture.width, size.y / texture.height);
    const ImVec2 image(texture.width * zoom, texture.height * zoom);
    const auto p = at + (size - image) * .5f;
    ImGui::GetWindowDrawList()->AddImage(texture.id, p, p + image);
}
}
float App::packStrip(ImVec2 at, float width) {
    if (!collection.connected || collection.packs[0] + collection.packs[1] <= 0) return 0;
    auto* draw = ImGui::GetWindowDrawList();
    const float height = 62 * scale, tile = std::min(350 * scale, (width - 14 * scale) * .5f);
    const bool enabled = collection.ready && collection.writable && !collection.busy;
    float x = at.x;
    for (unsigned kind = 0; kind < 2; ++kind) {
        if (collection.packs[kind] <= 0) continue;
        const ImVec2 pos(x, at.y);
        draw->AddRectFilled(pos, pos + ImVec2(tile, height), IM_COL32(0, 0, 0, 46), 3 * scale);
        draw->AddRect(pos, pos + ImVec2(tile, height), palette().line, 3 * scale);
        fitImage(assets.pack(kind), pos + ImVec2(8, 4) * scale, ImVec2(46, 54) * scale);
        text(pos + ImVec2(64, 9) * scale, packName(kind), 18, palette().ink);
        text(pos + ImVec2(64, 34) * scale, std::to_string(collection.packs[kind]) + " unopened", 14, palette().muted);
        if (button(kind ? "open-pack-tournament" : "open-pack", collection.busy ? "Opening…" : "Open",
                   pos + ImVec2(tile - 104 * scale, 13 * scale), ImVec2(94, 36) * scale, true, enabled))
            request({{"action", "open-pack"}, {"kind", kind}});
        x += tile + 14 * scale;
    }
    return height + 14 * scale;
}
void App::packModal() {
    if (collection.revealID && !ImGui::IsPopupOpen("Pack contents")) ImGui::OpenPopup("Pack contents");
    const auto display = ImGui::GetIO().DisplaySize;
    const int count = int(collection.revealed.size()), columns = count > 5 ? 4 : std::max(1, count),
              rows = (count + columns - 1) / columns;
    const ImVec2 size(std::min(display.x - 68 * scale, columns * (232 + 24) * scale + 48 * scale),
                      std::min(display.y - 90 * scale, (rows > 1 ? 840 : 610) * scale));
    ImGui::SetNextWindowPos(display * .5f, ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(size);
    if (!ImGui::BeginPopupModal("Pack contents", nullptr,
                                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | popupFlags()))
        return;
    popupSurface();
    if (!collection.revealID || !collection.ready) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const auto at = ImGui::GetWindowPos();
    const float gap = 24 * scale,
                w = std::min({232 * scale, (size.x - 72 * scale - (columns - 1) * gap) / columns,
                              ((size.y - 202 * scale) / std::max(1, rows) - 38 * scale) * .75f}),
                h = w / .75f;
    const float rowWidth = columns * w + (columns - 1) * gap;
    text(at + ImVec2(size.x * .5f, 28 * scale), count == 8 ? "Starter cards" : packName(collection.revealKind), 30,
         palette().accent, 0, true);
    line(at + ImVec2(30, 77) * scale, at + ImVec2(size.x - 30 * scale, 77 * scale));
    const float rowY = at.y + 105 * scale;
    // Cards are dealt face down and turn over one at a time; rarer cards make the player wait a beat.
    const auto tone = [](const std::string& rarity) {
        return rarity == "Rare"        ? IM_COL32(86, 223, 255, 255)
               : rarity == "Epic"      ? IM_COL32(214, 132, 250, 255)
               : rarity == "Legendary" ? IM_COL32(240, 196, 101, 255)
                                       : IM_COL32(226, 222, 208, 255);
    };
    const auto weight = [](const std::string& rarity) {
        return rarity == "Rare" ? 1 : rarity == "Epic" ? 2 : rarity == "Legendary" ? 3 : 0;
    };
    constexpr double turn = .44;
    std::vector<double> starts;
    double cursor = .55;
    for (const auto id : collection.revealed) {
        const int index = collection.index(id);
        const int rank = index < 0 ? 0 : weight(collection.cards[index].rarity) + (ttcg::isFoil(id) ? 1 : 0);
        cursor += rank * .16;
        starts.push_back(cursor);
        cursor += count > 5 ? .24 : .36;
    }
    const double now = ImGui::GetTime() - revealedAt, finished = starts.empty() ? 0 : starts.back() + turn;
    const bool revealing = now < finished;
    auto* draw = ImGui::GetWindowDrawList();
    for (int n = 0; n < int(collection.revealed.size()); ++n) {
        const auto id = collection.revealed[n];
        const int index = collection.index(id);
        if (index < 0) continue;
        const auto& c = collection.cards[index];
        const bool foil = ttcg::isFoil(id);
        const float dealt = std::clamp(float((now - n * .05) / .25), 0.f, 1.f), fade = dealt * dealt * (3 - 2 * dealt);
        const float progress = float((now - starts[n]) / turn);
        const bool shown = progress >= .5f;
        const ImVec2 p(at.x + (size.x - rowWidth) * .5f + (n % columns) * (w + gap),
                       rowY + (n / columns) * (h + 52 * scale) + (1 - fade) * 18 * scale);
        ImGui::PushID(n);
        ImGui::SetCursorScreenPos(p);
        const bool hit = ImGui::InvisibleButton("revealed-card", ImVec2(w, h), ImGuiButtonFlags_EnableNav);
        if (hit && progress >= 1) inspectCard(index + (foil ? collection.cards.size() : 0), false);
        const bool focus = ImGui::IsItemHovered() || ImGui::IsItemFocused();
        const int rank = weight(c.rarity) + (foil ? 1 : 0);
        const float after = std::max(0.f, progress - .5f) * float(turn);
        // The flare as the face appears, and a glow that stays on anything better than common.
        const float flare = shown ? std::clamp(1 - after / (.5f + .18f * rank), 0.f, 1.f) : 0;
        if (shown && rank > 0) glow(p, ImVec2(w, h), tone(c.rarity), .55f + .25f * rank + 2.2f * flare, 12 + 5 * rank);
        softShadow(p, ImVec2(w, h), fade, 9);
        const int start = draw->VtxBuffer.Size;
        if (shown)
            card(index, p, ImVec2(w, h), focus && progress >= 1, foil && face.foil, -1, MaterialSlot::packs + n, true);
        else {
            const auto back = assets.get(customBack);
            draw->AddImage(back.id, p, p + ImVec2(w, h));
            draw->AddRect(p, p + ImVec2(w, h), IM_COL32(0, 0, 0, 200), 0, 0, std::max(1.f, scale));
        }
        if (progress > 0 && progress < 1) {
            const float swing = std::sin(progress * 3.14159265f);
            scaleVertices(start, p + ImVec2(w, h) * .5f,
                          ImVec2(std::max(.03f, std::abs(std::cos(progress * 3.14159265f))) * (1 + .06f * swing),
                                 1 + .06f * swing));
        }
        if (flare > 0)
            draw->AddRectFilled(p, p + ImVec2(w, h),
                                (tone(c.rarity) & 0xffffff) | (ImU32(255 * .5f * flare * flare) << 24));
        if (shown)
            text(p + ImVec2(w * .5f, h + 17 * scale), c.rarity + (foil ? " · Foil" : ""), 18,
                 rank > 0 ? tone(c.rarity) : palette().muted, 0, true);
        fadeVertices(start, fade);
        ImGui::PopID();
    }
    const bool accept = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false) ||
                        ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false);
    const bool back = !inspecting && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                                      ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false));
    const bool pressed = button("pack-continue", revealing ? "Reveal all" : "Continue",
                                at + ImVec2((size.x - 180 * scale) * .5f, size.y - 66 * scale), ImVec2(180, 42) * scale,
                                true, !collection.busy);
    if (revealing) {
        // Any confirmation turns the rest over at once.
        if (pressed || back || accept) revealedAt = ImGui::GetTime() - finished;
    } else if (pressed || back)
        request({{"action", "dismiss-pack"}, {"revealID", collection.revealID}});
    inspect(); // Nested modal: Back closes artwork before dismissing the receipt.
    ImGui::EndPopup();
}
}
