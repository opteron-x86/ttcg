#include "GameApp.h"
#include "Announcements.h"
#include <cmath>
#include <numbers>
namespace preview {
void GameApp::nativeInspector() {
    if (!inspecting || selected < 0 || selected >= int(collection.cards.size())) return;
    const bool gallery = inspectionList.size() > 1;
    const ImVec2 extent = showTableDetails ? ImVec2(708, 540 + (gallery ? 40 : 0)) * scale
                                           : ImVec2(600, 672 + (gallery ? 40 : 0)) * scale;
    ImGui::SetNextWindowPos(frameAt + frameSize * .5f, ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(extent);
    if (!ImGui::IsPopupOpen("Artwork")) ImGui::OpenPopup("Artwork");
    if (!ImGui::BeginPopupModal("Artwork", nullptr,
                                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | popupFlags()))
        return;
    const auto origin = ImGui::GetWindowPos();
    auto* d = ImGui::GetWindowDrawList();
    d->PushClipRectFullScreen();
    surface(origin, extent);
    if (!skinned()) d->AddRect(origin, origin + extent, palette().line);
    d->PopClipRect();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
        inspecting = false;
        ImGui::CloseCurrentPopup();
    }
    const float h = (showTableDetails ? 430.f : 480.f) * scale, w = h * .75f;
    const ImVec2 artAt =
        showTableDetails ? origin + ImVec2(40, 36) * scale : origin + ImVec2((extent.x - w) / 2, 70 * scale);
    const ImVec2 centre = artAt + ImVec2(w, h) * .5f;
    const auto& c = collection.cards[selected];
    if (!showTableDetails)
        textCaps(origin + ImVec2(extent.x / 2, 26 * scale), c.name,
                 std::min(24.f, 24.f * (extent.x - 90 * scale) / std::max(1.f, capsWidth(c.name, 24, 2))),
                 palette().ink, 2, true);
    ImGui::SetCursorScreenPos(artAt);
    ImGui::InvisibleButton("rotate-art", ImVec2(w, h), ImGuiButtonFlags_EnableNav);
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0)) {
        yaw += ImGui::GetIO().MouseDelta.x * .009f;
        pitch += ImGui::GetIO().MouseDelta.y * .006f;
        turnTarget = yaw;
    }
    if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickLeft)) turnTarget -= ImGui::GetIO().DeltaTime;
    if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickRight)) turnTarget += ImGui::GetIO().DeltaTime;
    if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickUp)) pitch -= ImGui::GetIO().DeltaTime;
    if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickDown)) pitch += ImGui::GetIO().DeltaTime;
    yaw += (turnTarget - yaw) * std::min(1.f, ImGui::GetIO().DeltaTime * 8);
    const bool back = std::cos(yaw) * std::cos(pitch) < 0;
    // The card is drawn flat with its full face, then every vertex is tilted into place.
    const float depth = 1800 * scale;
    const auto project = [&](ImVec2 flat) {
        float x = flat.x - centre.x, y = flat.y - centre.y, z = -x * std::sin(yaw);
        x *= std::cos(yaw);
        const float yy = y * std::cos(pitch) - z * std::sin(pitch);
        z = y * std::sin(pitch) + z * std::cos(pitch);
        return centre + ImVec2(x, yy) * (depth / (depth + z));
    };
    // The shadow is drawn flat with the card, so it tilts with it.
    const int first = d->VtxBuffer.Size;
    softShadow(artAt, ImVec2(w, h), 1.5f, 18);
    if (back) {
        const auto texture = assets.get(customBack);
        artGrid = 16;
        artImage(texture, artAt, artAt + ImVec2(w, h), ImVec2(1, 0), ImVec2(0, 1));
        artGrid = 1;
        d->AddRect(artAt, artAt + ImVec2(w, h), IM_COL32(0, 0, 0, 220), 0, 0, std::max(1.f, 1.5f * scale));
    } else {
        foilLightSet = true;
        foilLight = ImVec2(.5f + std::sin(yaw) * .45f, .5f + std::sin(pitch) * .45f);
        artGrid = 16;
        card(selected, artAt, ImVec2(w, h), false, face.foil && selectedFoil, -1, 21, true);
        artGrid = 1;
        foilLightSet = false;
        // A sheen crosses the face as it turns.
        const float sweep = std::sin(yaw) * 1.6f;
        if (std::abs(sweep) > .02f && std::abs(sweep) < 1.3f) {
            const float x = artAt.x + w * (.5f + sweep * .5f), band = w * .22f;
            const ImU32 lit = IM_COL32(255, 255, 255, int(34 * (1 - std::abs(sweep) / 1.3f))), clear = lit & 0xffffff;
            d->PushClipRect(artAt, artAt + ImVec2(w, h), true);
            d->AddRectFilledMultiColor(ImVec2(x - band, artAt.y), ImVec2(x, artAt.y + h), clear, lit, lit, clear);
            d->AddRectFilledMultiColor(ImVec2(x, artAt.y), ImVec2(x + band, artAt.y + h), lit, clear, clear, lit);
            d->PopClipRect();
        }
    }
    for (int i = first; i < d->VtxBuffer.Size; ++i) d->VtxBuffer[i].pos = project(d->VtxBuffer[i].pos);
    const auto controls = [&](ImVec2 at) {
        if (button("flip", "Turn over", at, ImVec2(132, 38) * scale)) turnTarget += std::numbers::pi_v<float>;
        if (button("reset-art", "Reset", at + ImVec2(144, 0) * scale, ImVec2(96, 38) * scale)) turnTarget = pitch = 0;
    };
    if (showTableDetails) {
        const float infoWidth = 262 * scale, metadataHeight = metadata(c, {}, infoWidth - 12 * scale, false);
        const float infoHeight = std::min(430 * scale, metadataHeight + 64 * scale);
        const auto infoAt = ImVec2(origin.x + 402 * scale, artAt.y + (h - infoHeight) * .5f);
        ImGui::SetCursorScreenPos(infoAt);
        ImGui::BeginChild("inspection-details", ImVec2(infoWidth, infoHeight), ImGuiChildFlags_NavFlattened);
        const auto top = ImGui::GetCursorScreenPos();
        metadata(c, top, infoWidth - 12 * scale);
        controls(top + ImVec2(0, metadataHeight + 22 * scale));
        ImGui::EndChild();
    } else
        controls(origin + ImVec2(extent.x / 2 - 120 * scale, 572 * scale));
    // Prompts sit where the main window keeps its own.
    bool leave = false;
    const ImVec2 promptAt = origin + ImVec2(30 * scale, extent.y - 44 * scale);
    keyPrompt("Esc", "360_B", "Close", promptAt, &leave, "close-art");
    if (leave) {
        inspecting = false;
        ImGui::CloseCurrentPopup();
    }
    textRight(origin + ImVec2(extent.x - 30 * scale, extent.y - 38 * scale),
              padPrompts ? "Right stick turns the card" : "Drag the card to turn it", 14, fade(palette().muted, .8f));
    const int selectedDisplay = selected + (selectedFoil ? collection.cards.size() : 0);
    const auto found = std::find(inspectionList.begin(), inspectionList.end(), selectedDisplay);
    if (found != inspectionList.end() && inspectionList.size() > 1) {
        const int index = int(found - inspectionList.begin());
        const auto nav = origin + ImVec2(extent.x / 2 - 115 * scale, extent.y - 86 * scale);
        const bool previous = button("previous-art", "←", nav, ImVec2(43, 32) * scale, false, index > 0);
        const bool next = button("next-art", "→", nav + ImVec2(187, 0) * scale, ImVec2(43, 32) * scale, false,
                                 index + 1 < int(inspectionList.size()));
        text(nav + ImVec2(115 * scale, 5 * scale),
             std::to_string(index + 1) + " / " + std::to_string(inspectionList.size()), 16, palette().muted, 0, true);
        const int direction =
            previous || ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false)
                ? -1
            : next || ImGui::IsKeyPressed(ImGuiKey_RightArrow, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)
                ? 1
                : 0;
        if (direction && index + direction >= 0 && index + direction < int(inspectionList.size())) {
            const int target = inspectionList[index + direction];
            inspectCard(target, showTableDetails);
        }
    }
    ImGui::EndPopup();
}
}
