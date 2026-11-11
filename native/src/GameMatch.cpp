#include "GameApp.h"
#include "Presentation.h"
#include "Announcements.h"
#include <bit>
#include <cmath>
#include <numbers>
namespace preview {
bool GameApp::ruleRequestModal() {
    const auto& c = member(state, "collection");
    const auto& culture = member(c, "culture");
    const int offer = number(culture, "offerRule"), request = number(c, "ruleRequest");
    if (label(state, "screen") != "lobby" || label(state, "phase") != "ready" || (!offer && !request)) {
        ruleAnswerPending = false;
        return false;
    }
    std::string copy =
        offer ? "You brought " + rulesLabel(offer, false) + " from " + label(culture, "source") + ". Shall we try it?"
              : "Can we play with " + rulesLabel(request, false) + "?";
    if (offer) {
        if (const auto replaced = number(culture, "replaces"))
            copy += " It replaces " + rulesLabel(replaced, false) + " for this game.";
        copy += " If it catches on, local players may adopt it.";
    }
    const float width = 560 * scale, content = width - 64 * scale;
    const auto wrap = [&](const std::string& value, float pixels) {
        std::vector<std::string> lines;
        const char* begin = value.c_str();
        const char* end = begin + value.size();
        while (begin < end) {
            const char* stop = font()->CalcWordWrapPosition(pixels * scale, begin, end, content);
            if (stop <= begin) stop = end;
            lines.emplace_back(begin, stop);
            begin = stop;
            while (begin < end && (*begin == ' ' || *begin == '\n')) ++begin;
        }
        return lines;
    };
    const auto speaker = wrap(label(state, "opponent"), 26), message = wrap(copy, 23),
               notice = wrap(collection.notice, 16);
    const float noticeHeight = notice.empty() ? 0 : (18 + notice.size() * 21) * scale;
    const ImVec2 extent(width,
                        (36 + speaker.size() * 32 + 18 + message.size() * 29 + 28 + 42 + 36) * scale + noticeHeight);
    if (!ImGui::IsPopupOpen("Rule request")) ImGui::OpenPopup("Rule request");
    ImGui::SetNextWindowPos(frameAt + frameSize * .5f, ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(extent);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32, 36) * scale);
    if (ImGui::BeginPopupModal("Rule request", nullptr,
                               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | popupFlags())) {
        const auto at = ImGui::GetWindowPos();
        surface(at, extent);
        float y = at.y + 36 * scale;
        for (const auto& line : speaker) {
            text(ImVec2(at.x + width / 2, y), line, 26, palette().accent, 0, true);
            y += 32 * scale;
        }
        y += 18 * scale;
        for (const auto& line : message) {
            text(ImVec2(at.x + width / 2, y), line, 23, palette().ink, 0, true);
            y += 29 * scale;
        }
        if (!notice.empty()) {
            y += 18 * scale;
            for (const auto& line : notice) {
                text(ImVec2(at.x + width / 2, y), line, 16, palette().muted, 0, true);
                y += 21 * scale;
            }
        }
        const bool ready = !pendingCommand && !ruleAnswerPending;
        const auto answer = [&](bool accept) {
            send(offer ? "culture" : "terms",
                 offer ? std::string("trial ") + (accept ? "1" : "0") : "rule " + std::to_string(accept ? request : 0));
            ruleAnswerPending = true;
        };
        const auto buttons = ImVec2(at.x + (width - 320 * scale) / 2, y + 28 * scale);
        if (button("accept-rule", "Yes", buttons, ImVec2(150, 42) * scale, true, ready)) answer(true);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetItemDefaultFocus();
            ImGui::SetNavCursorVisible(true);
        }
        if (button("decline-rule", "No", buttons + ImVec2(170, 0) * scale, ImVec2(150, 42) * scale, false, ready))
            answer(false);
        // Close the offer conversation on Escape.
        const bool back = escapeRequested || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                          ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
        escapeRequested = false;
        if (back && ready) {
            send("close");
            ruleAnswerPending = true;
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    return true;
}
void GameApp::lobby(ImVec2 at, ImVec2 size) {
    const auto& c = member(state, "collection");
    const auto& culture = member(c, "culture");
    const int offer = number(culture, "offerRule");
    const bool tournamentGame = number(c, "tournamentID") != 0, casual = flag(c, "practice");
    // Who you are facing, and how you have fared against them.
    text(at, label(state, "opponent"), 35, palette().ink);
    auto subtitle = label(c, "hold") + " · " + (tournamentGame ? "Tournament" : label(c, "skill"));
    for (const auto& rival : arrayMember(c, "roster"))
        if (label(rival, "name") == label(state, "opponent") &&
            number(rival, "wins") + number(rival, "losses") + number(rival, "draws") > 0)
            subtitle += " · " + std::to_string(number(rival, "wins")) + " W · " +
                        std::to_string(number(rival, "losses")) + " L · " + std::to_string(number(rival, "draws")) +
                        " D";
    text(at + ImVec2(0, 46) * scale, subtitle, 18, palette().muted, size.x);
    const float top = at.y + 92 * scale, footerY = at.y + size.y - 73 * scale;
    const float panel = std::min(400 * scale, size.x * .31f), gutter = 40 * scale, left = size.x - panel - gutter;
    // Left: the deck you will play.
    float y = top;
    text(ImVec2(at.x, y + 7 * scale), "Deck", 18, palette().muted);
    std::vector<std::string> decks;
    int active = 0;
    for (const auto& deck : collection.decks) {
        if (deck.id == collection.activeDeck) active = int(decks.size());
        decks.push_back(deck.name);
    }
    ImGui::SetCursorScreenPos(ImVec2(at.x + 55 * scale, y));
    if (select("##lobby-deck", active, decks, 295 * scale))
        request({{"action", "select"}, {"deckID", collection.decks[active].id}});
    if (button("edit-deck", "Edit deck", ImVec2(at.x + left - 125 * scale, y), ImVec2(125, 38) * scale)) {
        tab = 1;
        send("browse", "album");
    }
    y += 58 * scale;
    const float gap = 18 * scale, room = footerY - y, w = std::min((left - 4 * gap) / 5, (room - 60 * scale) * .75f),
                h = w / .75f;
    const float first = at.x + (left - (w * 5 + gap * 4)) / 2;
    y += std::max(14 * scale, (room - h - 30 * scale) * .5f);
    for (int i = 0; i < 5; ++i) {
        const auto p = ImVec2(first + i * (w + gap), y);
        const int id = itemNumber(member(c, "deck"), i);
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(p);
        if (ImGui::InvisibleButton("setup-card", ImVec2(w, h), ImGuiButtonFlags_EnableNav)) {
            if (id >= 0)
                inspectCard(id);
            else {
                tab = 1;
                deckSlot = i;
                send("browse", "album");
            }
        }
        if (id >= 0 && ((ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) ||
                        (ImGui::IsItemFocused() &&
                         (ImGui::IsKeyPressed(ImGuiKey_I) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp)))))
            inspectCard(id);
        const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
        if (id >= 0) {
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 drawn = p - ImVec2(0, (hovered || focused ? 6 : 0) * scale);
            softShadow(drawn, ImVec2(w, h), hovered || focused ? 1.4f : 1, hovered || focused ? 14 : 10);
            displayCard(id, drawn, ImVec2(w, h), focused, -1, 1 + i);
        } else {
            ImGui::GetWindowDrawList()->AddRect(p, p + ImVec2(w, h), focused ? palette().accent : palette().line);
            text(p + ImVec2(w / 2, h / 2), "+", 30, palette().muted, 0, true);
        }
        text(p + ImVec2(w * .5f, h + 12 * scale), std::to_string(i + 1), 14, palette().muted, 0, true);
        ImGui::PopID();
    }
    // Right: what is being played for, stated in full before the game begins.
    const ImVec2 terms(at.x + size.x - panel, top);
    const float edge = terms.x - gutter * .5f;
    skinRule(ImVec2(edge, top), ImVec2(edge, footerY - 14 * scale), .45f);
    textCaps(terms + ImVec2(0, 8 * scale), "Rules of this game", 14, palette().muted, 1.6f);
    const int fixed = number(c, "fixedWager", -1),
              affordable = std::max(0, std::min({number(c, "gold"), number(c, "rivalGold"), number(c, "maxWager")}));
    if (fixed < 0 && (wager > affordable || !flag(c, "canStake"))) wager = 0;
    const bool stakes = !tournamentGame && !casual;
    const float actions = (stakes ? 150 : 96) * scale, listHeight = footerY - 14 * scale - top - 40 * scale - actions;
    ImGui::SetCursorScreenPos(terms + ImVec2(-8 * scale, 40 * scale));
    ImGui::BeginChild("terms", ImVec2(panel + 8 * scale, listHeight), ImGuiChildFlags_NavFlattened);
    const auto listAt = ImGui::GetCursorScreenPos() + ImVec2(8 * scale, 0);
    const float used = ruleChips(listAt, panel - 22 * scale, number(state, "rules"),
                                 tournamentGame ? -1 : number(c, "fixedTrade"), casual, true, nullptr, false, true);
    ImGui::SetCursorScreenPos(listAt + ImVec2(0, used));
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();
    float ay = footerY - 14 * scale - actions + 12 * scale;
    if (stakes) {
        textCaps(ImVec2(terms.x, ay + 11 * scale), "Wager", 14, palette().muted, 1.6f);
        if (fixed < 0 && number(c, "maxWager")) {
            std::vector<std::string> amounts;
            std::vector<int> values;
            int chosen = 0;
            for (int value : ttcg::wagerAmounts)
                if (value <= affordable) {
                    if (wager == value) chosen = int(values.size());
                    values.push_back(value);
                    amounts.push_back(std::to_string(value) + " gold");
                }
            ImGui::SetCursorScreenPos(ImVec2(terms.x + panel - 150 * scale, ay));
            ImGui::BeginDisabled(!flag(c, "canStake"));
            if (select("##wager", chosen, amounts, 150 * scale)) wager = values[chosen];
            ImGui::EndDisabled();
        } else
            textRight(ImVec2(terms.x + panel, ay + 8 * scale), fixed > 0 ? std::to_string(fixed) + " gold" : "None", 18,
                      palette().ink);
        textRight(ImVec2(terms.x + panel, ay + 44 * scale), "You carry " + std::to_string(number(c, "gold")) + " gold",
                  14, palette().muted);
        ay += 72 * scale;
    }
    const bool complete = std::all_of(collection.deck.begin(), collection.deck.end(), [](auto id) { return id != 0; });
    const bool ready = !pendingCommand && !offer && !number(c, "ruleRequest") &&
                       (!complete || (flag(c, "canPlay") && fixed <= affordable));
    if (button("play", complete ? "Play" : "Complete deck", ImVec2(terms.x, ay), ImVec2(panel, 50 * scale), true,
               ready)) {
        if (!complete) {
            tab = 1;
            deckSlot = -1;
            send("browse", "album");
        } else {
            std::string args = std::to_string(number(state, "rules")) + " " + std::to_string(number(c, "fixedTrade")) +
                               " " + std::to_string(tournamentGame || casual ? 0 : wager);
            for (const auto& id : member(c, "deck")) args += " " + std::to_string(id.get<int>());
            send("start", args);
        }
    }
    if (!ready && complete) {
        auto reason = fixed > affordable ? "Not enough gold for this wager." : label(c, "playUnavailableReason");
        text(ImVec2(terms.x, ay + 56 * scale), reason, 15, palette().muted, panel);
    }
    const auto footer = ImVec2(at.x, footerY);
    line(footer, footer + ImVec2(size.x, 0));
    if (button("album", "Album", footer + ImVec2(0, 23) * scale, ImVec2(95, 38) * scale)) {
        tab = 0;
        send("browse", "album");
    }
    if (button("lobby-rules", "Rules", footer + ImVec2(105, 23) * scale, ImVec2(95, 38) * scale)) {
        showRuleReference = true;
        topic = "play";
        demoStep = 0;
    }
    if (flag(culture, "canAbolish")) {
        if (button("abolish-menu", "Challenge a rule", footer + ImVec2(210, 23) * scale, ImVec2(165, 38) * scale))
            ImGui::OpenPopup("Abolish a rule");
        if (ImGui::BeginPopup("Abolish a rule")) {
            const unsigned allowed = number(culture, "abolishableRules");
            for (auto [bit, name] : ruleNames)
                if (allowed & bit)
                    if (ImGui::Selectable(name)) {
                        confirm("Challenge " + std::string(name) + "?", "Win to try abolishing this rule in the hold.",
                                "culture", "abolish " + std::to_string(bit), "Challenge");
                    }
            ImGui::EndPopup();
        }
    }
    if (number(c, "ruleLesson") || number(member(member(state, "settings"), "development"), "RuleLesson"))
        if (button("lesson", "Practice rule", footer + ImVec2(390, 23) * scale, ImVec2(150, 38) * scale))
            send("rule-lesson");
}
void GameApp::place(int hand, int square) {
    if (hand < 0 || hand >= 5 || square < 0 || square >= 9 || pendingCommand || presentation.pending(state) ||
        flag(state, "thinking"))
        return;
    const int id = itemNumber(member(state, "hands")[0], hand);
    if (id < 0 || !member(state, "board")[square].is_null()) return;
    const auto mask = collection.cards[id % collection.cards.size()].groups;
    if ((number(state, "rules") & (128 | 256)) && label(state, "activeGroup").empty() && std::popcount(mask) > 1) {
        groupHand = hand;
        groupSquare = square;
        groupChoices.clear();
        for (int g = 0; g < int(ttcg::creatureGroups.size()); ++g)
            if (mask & (1ull << g)) groupChoices.push_back(g);
        return;
    }
    send("play", std::to_string(hand) + " " + std::to_string(square));
}
void GameApp::groupModal() {
    if (groupHand < 0) return;
    if (!ImGui::IsPopupOpen("Choose group")) ImGui::OpenPopup("Choose group");
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * .5f, ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(ImVec2(460 * scale, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(25, 25) * scale);
    if (ImGui::BeginPopupModal("Choose group", nullptr,
                               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | popupFlags())) {
        popupSurface();
        textBlock(number(state, "rules") & 128 ? "Choose the Legion group" : "Choose the Decimation group", 400 * scale,
                  25, palette().accent);
        ImGui::Dummy(ImVec2(0, 15 * scale));
        for (int g : groupChoices) {
            auto p = ImGui::GetCursorScreenPos();
            if (button(("group" + std::to_string(g)).c_str(), ttcg::creatureGroups[g].data(), p,
                       ImVec2(400, 40) * scale)) {
                send("play", std::to_string(groupHand) + " " + std::to_string(groupSquare) + " " + std::to_string(g));
                groupHand = -1;
                ImGui::CloseCurrentPopup();
                break;
            }
        }
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
            groupHand = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}
std::pair<ImVec2, ImVec2> GameApp::table(const Json& data, ImVec2 at, ImVec2 size, bool demo, bool liveLesson) {
    if (demo) {
        const std::string identity = (liveLesson ? "lesson" + std::to_string(state.value("session", 0ull)) : topic);
        if (identity != demoIdentity || number(demoAnimation, "step", -1) != number(data, "step")) {
            Json next = data;
            next["session"] = std::hash<std::string>{}(identity);
            next["revision"] = number(data, "step");
            next["phase"] = "match";
            next["settling"] =
                identity == demoIdentity && number(data, "step") == number(demoAnimation, "step", -2) + 1;
            ttcg::CaptureAnnouncements announcements;
            for (auto& stage : next["stages"]) stage["label"] = announcements.next(number(stage, "rule"));
            demoPresentation.begin(next, ImGui::GetTime(), demoAnimation);
            demoAnimation = std::move(next);
            demoIdentity = identity;
        }
        if (liveLesson && demoPresentation.pending(demoAnimation)) ruleSounds(demoAnimation, demoPresentation);
        if (ImGui::GetTime() - demoPresentation.start >= demoPresentation.duration)
            demoPresentation.acknowledged = true;
    }
    const auto& clock = demo ? demoPresentation : presentation;
    const auto& animation = demo ? demoAnimation : state;
    const bool animating = clock.pending(animation);
    const bool collected = !demo && label(state, "phase") == "result" && trade.phase != TradeView::None;
    if (!demo && !collected) trade.sourceMask = 0;
    const bool staticDemo = demo && !liveLesson && (topic == "open" || topic == "threeopen");
    const bool busy =
        animating || (!demo && (flag(state, "thinking") || pendingCommand || label(state, "phase") != "match"));
    const bool playerTurn = demo ? number(data, "hand", -1) >= 0 : number(state, "turn") == 0;
    const float gap = 10 * scale, boardH = size.y - 55 * scale, tileH = (boardH - 2 * gap - 22 * scale) / 3,
                tileW = tileH * .75f, boardW = 3 * tileW + 2 * gap + 22 * scale;
    const ImVec2 boardAt = at + ImVec2((size.x - boardW) / 2, 45 * scale);
    auto* d = ImGui::GetWindowDrawList();
    const int configuredSurface = std::clamp(number(member(state, "settings"), "boardSurface"), 0, 7),
              surface = configuredSurface ? configuredSurface
                        : skin().board    ? skin().board
                                          : ttcg::interfaceThemes[theme].boardSurface;
    struct BoardColours {
        ImU32 edge, grid, mark, cell;
    };
    constexpr BoardColours colours[]{
        {IM_COL32(146, 151, 132, 119), IM_COL32(210, 216, 197, 33), IM_COL32(203, 209, 186, 82),
         IM_COL32(255, 255, 255, 3)},
        {IM_COL32(139, 151, 155, 153), IM_COL32(224, 232, 225, 43), IM_COL32(180, 193, 199, 82), IM_COL32(0, 0, 0, 5)},
        {IM_COL32(179, 157, 107, 153), IM_COL32(186, 204, 171, 54), IM_COL32(217, 202, 156, 82), IM_COL32(0, 0, 0, 8)},
        {IM_COL32(118, 85, 55, 199), IM_COL32(103, 67, 35, 82), IM_COL32(114, 78, 41, 82), IM_COL32(137, 94, 34, 3)},
        {IM_COL32(169, 154, 133, 130), IM_COL32(201, 185, 164, 43), IM_COL32(186, 170, 143, 82),
         IM_COL32(10, 4, 3, 16)},
        {IM_COL32(196, 158, 105, 140), IM_COL32(226, 193, 144, 56), IM_COL32(211, 182, 136, 82),
         IM_COL32(31, 17, 10, 21)},
        {IM_COL32(233, 230, 221, 90), IM_COL32(233, 230, 221, 40), IM_COL32(233, 230, 221, 70), IM_COL32(0, 0, 0, 10)}};
    const auto boardColour = colours[surface - 1];
    boardSurface(surface, boardAt, ImVec2(boardW, boardH));
    // A board material is a field texture, a railed frame around it and a recess for each empty square.
    // The Board texture setting fades the material into the flat board; a tenth or more shows it in full.
    constexpr const char* materials[]{"", "", "slate", "felt", "parchment", "leather", "walnut", "slab"};
    const float material = std::clamp(face.grain * 10, 0.f, 1.f);
    const auto boardArt = [&](const char* file) {
        return surface > 1 && material > 0
                   ? assets.optional(std::string("../assets/boards/") + materials[surface] + file)
                   : Texture{};
    };
    const auto field = boardArt("/surface.webp"), rail = boardArt("/frame.png");
    const auto slot = field.id ? assets.optional("../assets/boards/cell.png") : Texture{};
    const auto strength = IM_COL32(255, 255, 255, int(material * 255));
    if (field.id)
        d->AddImage(field.id, boardAt, boardAt + ImVec2(boardW, boardH), ImVec2(0, 0), ImVec2(1, 1), strength);
    if (rail.id) {
        // The texture's rail is 64 pixels wide inside a 96 pixel corner.
        const float width = 15 * scale;
        nineSlice(rail, boardAt - ImVec2(width, width), ImVec2(boardW, boardH) + ImVec2(width, width) * 2, 96,
                  width / 64, false, strength);
    } else {
        d->AddRect(boardAt, boardAt + ImVec2(boardW, boardH), boardColour.edge);
        const ImVec2 a = boardAt - ImVec2(5, 5) * scale, b = boardAt + ImVec2(boardW, boardH) + ImVec2(5, 5) * scale;
        skinRule(a, ImVec2(b.x, a.y));
        skinRule(ImVec2(a.x, b.y), b);
        skinRule(a, ImVec2(a.x, b.y), .7f);
        skinRule(ImVec2(b.x, a.y), b, .7f);
    }
    const float handArea = (size.x - boardW) / 2 - 30 * scale;
    const bool stack = demo;
    const bool live = !demo && label(state, "phase") == "match";
    if (!demo) {
        const auto session = state.value("session", 0ull);
        if (session != lastMoveSession) {
            lastMoveSession = session;
            lastMove = -1;
            shownScores = {-1, -1};
        }
        if (clock.placedSquare >= 0) lastMove = clock.placedSquare;
        const auto& cells = arrayMember(data, "board");
        if (lastMove >= int(cells.size()) || (lastMove >= 0 && cells[lastMove].is_null())) lastMove = -1;
    }
    const float breath = .5f + .5f * std::sin(float(ImGui::GetTime()) * 2.1f);
    if (live && !animating && !flag(state, "thinking")) {
        // The board's edge nearest whoever is to move is lit in their colour.
        const ImU32 tone = controlColour(playerTurn ? 0 : 1, true);
        const ImU32 lit = fade(tone, .10f + .07f * breath), clear = lit & 0xffffff;
        const ImU32 bright = fade(tone, .55f + .35f * breath);
        const float reach = 30 * scale, rail = 15 * scale, weight = std::max(2.f, 2.5f * scale);
        const float edge = playerTurn ? boardAt.x - rail : boardAt.x + boardW + rail, out = playerTurn ? -1.f : 1.f;
        const float top = boardAt.y - rail, bottom = boardAt.y + boardH + rail, taper = (bottom - top) * .22f;
        const auto strip = [&](float x0, float x1, ImU32 inner, ImU32 outer) {
            const float l = std::min(x0, x1), r = std::max(x0, x1);
            const ImU32 lc = x0 < x1 ? inner : outer, rc = x0 < x1 ? outer : inner;
            d->AddRectFilledMultiColor(ImVec2(l, top), ImVec2(r, top + taper), lc & 0xffffff, rc & 0xffffff, rc, lc);
            d->AddRectFilledMultiColor(ImVec2(l, top + taper), ImVec2(r, bottom - taper), lc, rc, rc, lc);
            d->AddRectFilledMultiColor(ImVec2(l, bottom - taper), ImVec2(r, bottom), lc, rc, rc & 0xffffff,
                                       lc & 0xffffff);
        };
        strip(edge, edge + out * reach, lit, clear);
        strip(edge, edge + out * weight, bright, bright);
    }
    // Hand cards are as large as board cards: their values matter most before they are played.
    const float hw = stack ? std::min(handArea * .70f, tileW * .84f) : std::min((handArea - gap) / 2, tileW),
                hh = hw / .75f;
    const float handY = at.y + 55 * scale + (boardH - (stack ? hh * 2.15f : hh * 3 + 2 * gap)) * .5f;
    const double elapsed = ImGui::GetTime() - clock.start;
    const double swapLength =
        flag(state, "opening") && itemNumber(member(state, "swapped"), 0) >= 0 && number(state, "redeals") == 0 ? 2.2
                                                                                                                : 0;
    std::array<std::array<ImVec2, 5>, 2> handPositions{};
    std::array<int, 2> scores{};
    if (member(data, "hands").is_array())
        for (int p = 0; p < 2; ++p)
            for (const auto& id : data["hands"][p])
                if (!id.is_null() && (!id.is_number_integer() || id.get<int>() != -1)) ++scores[p];
    for (int q = 0; q < int(arrayMember(data, "board").size()); ++q) {
        const auto& cell = data["board"][q];
        if (cell.is_null()) continue;
        int owner = number(cell, "owner");
        if (animating)
            for (int i = 0; i < int(arrayMember(animation, "stages").size()); ++i) {
                const auto& stage = animation["stages"][i];
                if (number(stage, "captures") & (1 << q))
                    if (elapsed < clock.stageStarts[i] + (label(stage, "label").empty() ? 0 : .76) + .2)
                        owner = 1 - owner;
            }
        ++scores[owner];
    }
    for (int player = 0; player < 2; ++player) {
        const float left = player ? boardAt.x + boardW + 30 * scale : at.x;
        const ImU32 colour = controlColour(player, true);
        const float firstProgress = float((elapsed - swapLength - 1.15) / 1.1);
        const float firstGlow = !demo && animating && flag(state, "opening") && number(state, "turn") == player
                                    ? std::sin(std::numbers::pi_v<float> * std::clamp(firstProgress, 0.f, 1.f))
                                    : 0;
        const auto name = player ? (demo ? "Opponent" : label(state, "opponent", "Opponent")) : "You";
        const float nameSize = std::clamp(22.f * (handArea - 86 * scale) /
                                              std::max(1.f, font()->CalcTextSizeA(22 * scale, 1e5f, 0, name.c_str()).x),
                                          12.f, 22.f);
        const float nameWidth = font()->CalcTextSizeA(nameSize * scale, 1e5f, 0, name.c_str()).x;
        // The skin's band sits behind whoever is to move.
        if (playerTurn == (player == 0) && !animating)
            skinBand("selector", ImVec2(left, at.y - 4 * scale), ImVec2(handArea, 40 * scale));
        text(ImVec2(player ? left + handArea - nameWidth - 24 * scale : left + 24 * scale, at.y + 3 * scale), name,
             nameSize, firstGlow > 0 ? colour : palette().ink);
        float pop = 0;
        if (!demo) {
            if (shownScores[player] != scores[player]) {
                if (shownScores[player] >= 0) scoreChanged[player] = ImGui::GetTime();
                shownScores[player] = scores[player];
            }
            pop = std::clamp(1 - float(ImGui::GetTime() - scoreChanged[player]) / .45f, 0.f, 1.f);
            pop *= pop;
        }
        const auto score = std::to_string(scores[player]);
        const float scoreWidth = font()->CalcTextSizeA((36 + 12 * pop) * scale, 1e5f, 0, score.c_str()).x;
        text(ImVec2(player ? left + 8 * scale : left + handArea - 8 * scale - scoreWidth, at.y - (3 + 7 * pop) * scale),
             score, 36 + 12 * pop, colour);
        line(ImVec2(left, at.y + 35 * scale), ImVec2(left + handArea, at.y + 35 * scale),
             playerTurn == (player == 0) ? colour : palette().line);
        const auto& hand = member(data, "hands");
        if (!hand.is_array() || hand.size() < 2) continue;
        for (int order = 0; order < 5; ++order) {
            const int target = number(data, "hand", -1);
            const int h = stack && player == 0 && target >= 0 ? (order == 4       ? target
                                                                 : order < target ? order
                                                                                  : order + 1)
                                                              : order;
            const int display = collected ? -1 : itemNumber(hand[player], h);
            const float x =
                stack ? left + (handArea - hw) / 2
                      : left + (h == 4 ? (handArea - hw) / 2 : (handArea - 2 * hw - gap) / 2 + (h % 2) * (hw + gap));
            const float y = handY + (stack ? h * hh * .29f : (h / 2) * (hh + gap)) - firstGlow * 6 * scale;
            const ImVec2 pos(x, y);
            handPositions[player][h] = pos;
            if (display == -1) {
                // A played card leaves its place marked, so the hand keeps its shape to the last card.
                if (!stack && !collected) {
                    d->AddRectFilled(pos, pos + ImVec2(hw, hh), IM_COL32(0, 0, 0, 38), 3 * scale);
                    d->AddRect(pos, pos + ImVec2(hw, hh), fade(palette().line, .55f), 3 * scale);
                }
                continue;
            }
            if (!demo && !collected) {
                const int origin = member(data, "handOrigins").is_array()
                                       ? itemNumber(data["handOrigins"][player], h, player * 5 + h)
                                       : player * 5 + h;
                if (origin >= 0 && origin < 10) {
                    trade.sources[origin] = pos;
                    trade.sizes[origin] = ImVec2(hw, hh);
                    trade.sourceMask |= 1u << origin;
                }
            }
            const int forced = number(data, "forcedHand", -1);
            const bool selectable = !busy && player == 0 && playerTurn && (forced < 0 || forced == h) &&
                                    (!demo || staticDemo || number(data, "hand", -1) == h);
            ImGui::PushID(player * 5 + h);
            ImVec2 hitAt = pos, hitSize(hw, stack && h < 4 && h != number(data, "hand", -1) ? hh * .29f : hh);
            // Reserve the raised tutorial card’s input area.
            if (stack && player == 0 && target >= 0 && h > target) {
                hitAt.y = std::max(pos.y, handY + target * hh * .29f + hh);
                hitSize.y = pos.y + hitSize.y - hitAt.y;
            }
            ImGui::SetCursorScreenPos(hitAt);
            const bool hittable = hitSize.y > 0;
            const bool hit = hittable && ImGui::InvisibleButton("hand", hitSize, ImGuiButtonFlags_EnableNav);
            if (hit && selectable && !staticDemo) {
                selectedHand = h;
                demoSelected = demo;
            }
            if (hittable && display >= 0 &&
                ((ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) ||
                 (ImGui::IsItemFocused() &&
                  (ImGui::IsKeyPressed(ImGuiKey_I) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp)))))
                inspectCard(display, true,
                            itemNumber(member(data, "handModifiers").is_array() ? member(data, "handModifiers")[player]
                                                                                : Json{},
                                       h, 0));
            if (selectable && !demo && ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("TESSERA_HAND", &h, sizeof(h));
                selectedHand = h;
                dragCard(display % collection.cards.size(), display >= int(collection.cards.size()), 0,
                         CardContext::Hand);
                ImGui::EndDragDropSource();
            }
            const bool selected =
                player == 0 && (demo ? !staticDemo && number(data, "hand", -1) == h : selectedHand == h && playerTurn);
            const bool swapped = !demo && presentation.pending(state) && elapsed < swapLength &&
                                 itemNumber(member(state, "swapped"), player) == h;
            if (!swapped) {
                if (firstGlow > 0)
                    for (int r = 16; r > 0; --r) {
                        const float padding = r * scale;
                        d->AddRect(pos - ImVec2(padding, padding), pos + ImVec2(hw + padding, hh + padding),
                                   fade(colour, firstGlow * .065f * (1 - r / 17.f)), padding, 0, 2 * scale);
                    }
                // The chosen card rises out of the hand; a hovered one stirs.
                const bool hovered = hittable && selectable && ImGui::IsItemHovered();
                const ImVec2 drawn = pos - ImVec2(0, stack ? 0 : (selected ? 9 : hovered ? 4 : 0) * scale);
                if (!stack) softShadow(drawn, ImVec2(hw, hh), selected ? 1.5f : .8f, selected ? 14 : 8);
                displayCard(display, drawn, ImVec2(hw, hh),
                            selected || (hittable && ImGui::IsItemFocused() && ImGui::GetIO().NavVisible), player,
                            (demo ? 110 : 1) + player * 5 + h,
                            itemNumber(member(data, "handModifiers").is_array() ? member(data, "handModifiers")[player]
                                                                                : Json{},
                                       h, 0),
                            false, CardContext::Hand);
            }
            if (forced >= 0 && player == 0 && forced == h)
                text(pos + ImVec2(hw / 2, hh - 20 * scale), number(data, "rules") & 512 ? "Order" : "Chaos", 13,
                     palette().accent, 0, true);
            ImGui::PopID();
        }
    }
    for (int square = 0; square < 9; ++square) {
        const auto pos =
            boardAt + ImVec2(11 * scale + (square % 3) * (tileW + gap), 11 * scale + (square / 3) * (tileH + gap));
        const ImVec2 cardSize(tileW, tileH);
        const auto& board = member(data, "board");
        if (!board.is_array() || square >= int(board.size())) continue;
        const auto& cell = board[square];
        ImGui::PushID(30 + square);
        ImGui::SetCursorScreenPos(pos);
        const bool hit = ImGui::InvisibleButton("square", cardSize, ImGuiButtonFlags_EnableNav);
        if (!cell.is_null() && !collected) {
            if (!demo) {
                const int origin = number(cell, "origin", -1);
                if (origin >= 0 && origin < 10) {
                    trade.sources[origin] = pos;
                    trade.sizes[origin] = cardSize;
                    trade.sourceMask |= 1u << origin;
                }
            }
            int owner = number(cell, "owner");
            float turnWidth = 1, flash = 0, lift = 0;
            bool highlight = false;
            if (animating) {
                const auto& stages = arrayMember(animation, "stages");
                for (int i = 0; i < int(stages.size()); ++i) {
                    const auto& stage = stages[i];
                    const double start = clock.stageStarts[i];
                    const double labelDelay = label(stage, "label").empty() ? 0 : .76;
                    if ((number(stage, "highlights") & (1 << square)) && elapsed >= start &&
                        elapsed < start + labelDelay)
                        highlight = true;
                    if (number(stage, "captures") & (1 << square)) {
                        const double progress = (elapsed - start - labelDelay) / .4;
                        if (progress < .5) owner = 1 - owner;
                        if (progress >= 0 && progress < 1) {
                            turnWidth = std::max(.025f, float(std::abs(std::cos(progress * std::numbers::pi))));
                            lift = float(std::sin(progress * std::numbers::pi));
                        }
                        // The captor's colour flares as the card lands, then drains away.
                        if (progress >= .5 && progress < 2.4) flash = std::max(flash, float(1 - (progress - .5) / 1.9));
                    }
                }
            }
            const int firstVertex = d->VtxBuffer.Size;
            const bool moving = square == clock.placedSquare && clock.placedHand >= 0 && animating && elapsed < .36;
            if (!moving) softShadow(pos, cardSize, .9f + lift, 7 + 7 * lift);
            if (!moving)
                displayCard(number(cell, "card", -1), pos, cardSize,
                            highlight || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible), owner,
                            (demo ? 120 : 11) + square, itemNumber(member(data, "modifiers"), square, 0), false,
                            CardContext::Board);
            if (turnWidth < 1 || lift > 0)
                scaleVertices(firstVertex, pos + cardSize * .5f,
                              ImVec2(turnWidth * (1 + .07f * lift), 1 + .07f * lift));
            if (flash > 0 && !moving) {
                const ImU32 tone = controlColour(owner, true);
                d->AddRectFilled(pos, pos + cardSize, fade(tone, .42f * flash * flash));
                glow(pos, cardSize, tone, 1.6f * flash, 16);
            }
            if (!demo && square == lastMove && !animating && live)
                cornerMarks(pos, cardSize, fade(palette().ink, .78f + .22f * breath), 13, 4);
            if ((hit || ImGui::IsItemHovered() && ImGui::IsMouseClicked(1) ||
                 ImGui::IsItemFocused() &&
                     (ImGui::IsKeyPressed(ImGuiKey_I) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp))) &&
                !busy)
                inspectCard(number(cell, "card", -1), true, itemNumber(member(data, "modifiers"), square, 0));
        } else {
            if (slot.id)
                d->AddImage(slot.id, pos, pos + cardSize, ImVec2(0, 0), ImVec2(1, 1), strength);
            else {
                d->AddRectFilled(pos, pos + cardSize, boardColour.cell);
                d->AddRect(pos, pos + cardSize, boardColour.grid, 1);
            }
            const int affinity = itemNumber(member(data, "tiles"), square, 0);
            if (affinity) {
                // The square's affinity is carved into the board inside a ring, not laid on top of it.
                const int a = std::countr_zero(unsigned(affinity));
                const float icon = tileW * .30f, ring = icon * .86f, cut = std::max(1.f, 1.3f * scale);
                const auto centre = pos + cardSize * .5f;
                auto image = assets.get("../icons/affinity-" + std::to_string(a) + ".png");
                const ImVec2 half(icon * .5f, icon * .5f);
                d->AddCircleFilled(centre, ring, IM_COL32(0, 0, 0, 62), 48);
                d->AddCircle(centre + ImVec2(0, cut), ring, IM_COL32(255, 255, 255, 26), 48, cut);
                d->AddCircle(centre, ring, IM_COL32(0, 0, 0, 150), 48, cut);
                d->AddCircle(centre, ring - 4 * scale, fade(boardColour.mark, .30f), 48, std::max(1.f, scale));
                d->AddImage(image.id, centre - half + ImVec2(cut, cut * 1.4f), centre + half + ImVec2(cut, cut * 1.4f),
                            ImVec2(0, 0), ImVec2(1, 1), IM_COL32(0, 0, 0, 170));
                d->AddImage(image.id, centre - half, centre + half, ImVec2(0, 0), ImVec2(1, 1),
                            IM_COL32(205, 200, 188, 150));
            } else {
                const auto centre = pos + cardSize * .5f;
                const float r = 3 * scale;
                d->AddQuadFilled(centre + ImVec2(0, -r), centre + ImVec2(r, 0), centre + ImVec2(0, r),
                                 centre + ImVec2(-r, 0), boardColour.mark);
            }
            const bool target = !busy && playerTurn && selectedHand >= 0 &&
                                (!demo || !staticDemo && demoSelected && square == number(data, "square", -1));
            const bool aimed =
                (target && (ImGui::IsItemHovered() || demo)) || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible);
            if (aimed && target && !demo) {
                // A ghost of the chosen card shows where it would land.
                const int ghost = itemNumber(member(data, "hands")[0], selectedHand);
                if (ghost >= 0) {
                    const int first = d->VtxBuffer.Size;
                    displayCard(ghost, pos, cardSize, false, 0, 161, 0, false, CardContext::Board);
                    fadeVertices(first, .42f);
                }
            }
            if (aimed) {
                glow(pos, cardSize, palette().accent, .9f, 10);
                d->AddRect(pos, pos + cardSize, palette().accent, 2, 0, 2 * scale);
            }
            if (hit && target) {
                if (demo) {
                    if (liveLesson) {
                        demoSelected = false;
                        send("lesson", "place " + std::to_string(number(data, "hand")) + " " + std::to_string(square));
                    } else {
                        ++demoStep;
                        demoSelected = false;
                        demoChanged = ImGui::GetTime();
                    }
                } else
                    place(selectedHand, square);
            }
            if (!demo && ImGui::BeginDragDropTarget()) {
                if (auto* payload = ImGui::AcceptDragDropPayload("TESSERA_HAND"))
                    if (payload->DataSize == sizeof(int) && !busy)
                        place(*static_cast<const int*>(payload->Data), square);
                ImGui::EndDragDropTarget();
            }
        }
        ImGui::PopID();
    }
    if (!demo && animating && elapsed < swapLength) {
        const float t = std::clamp(float((elapsed - .60) / .70), 0.f, 1.f), ease = t * t * (3 - 2 * t);
        for (int player = 0; player < 2; ++player) {
            const int hand = itemNumber(member(state, "swapped"), player),
                      other = itemNumber(member(state, "swapped"), 1 - player);
            if (hand < 0 || other < 0) continue;
            const auto source = handPositions[1 - player][other], target = handPositions[player][hand];
            displayCard(itemNumber(state["hands"][player], hand), source + (target - source) * ease, ImVec2(hw, hh),
                        true, t < .5f ? 1 - player : player, 162 + player, 0, false, CardContext::Hand);
        }
    }
    if (animating && clock.placedSquare >= 0 && clock.placedHand >= 0 && elapsed < .36) {
        const int q = clock.placedSquare;
        const auto target =
            boardAt + ImVec2(11 * scale + (q % 3) * (tileW + gap), 11 * scale + (q / 3) * (tileH + gap));
        const auto source = handPositions[clock.placedPlayer][clock.placedHand];
        const float t = 1 - std::pow(1 - std::clamp(float(elapsed / .36), 0.f, 1.f), 3);
        displayCard(number(data["board"][q], "card", -1), source + (target - source) * t,
                    ImVec2(hw, hh) + (ImVec2(tileW, tileH) - ImVec2(hw, hh)) * t, false, clock.placedPlayer, 160, 0,
                    false, t < .5f ? CardContext::Hand : CardContext::Board);
    }
    if (demo && animating) {
        for (int i = 0; i < int(arrayMember(animation, "stages").size()); ++i) {
            const auto& stage = animation["stages"][i];
            const auto title = label(stage, "label");
            if (!title.empty() && elapsed >= clock.stageStarts[i] && elapsed < clock.stageStarts[i] + .76) {
                drawAnnouncement(d, font(), boardAt, ImVec2(boardW, boardH), scale, title,
                                 float((elapsed - clock.stageStarts[i]) / .76), title == "Combo" ? "combo" : "rule");
            }
        }
    }
    if (demo) {
        for (const auto& comparison : arrayMember(data, "comparisons")) {
            if (!comparison.is_array() || comparison.size() < 2) continue;
            const int sq = itemNumber(comparison, 0), side = itemNumber(comparison, 1);
            if (sq < 0 || sq > 8 || side < 0 || side > 3) continue;
            const auto p =
                boardAt + ImVec2(11 * scale + (sq % 3) * (tileW + gap), 11 * scale + (sq / 3) * (tileH + gap));
            d->AddCircle(p + valueAnchor(side, ImVec2(tileW, tileH)), tileW * .105f, palette().accent, 32, 2 * scale);
        }
    }
    return {boardAt, ImVec2(boardW, boardH)};
}
void GameApp::matchPanel(ImVec2 at, ImVec2 size) {
    const float rulesHeight = ruleChips(
        at + ImVec2(size.x * .28f, -4 * scale), size.x * .44f, number(state, "rules"),
        number(member(state, "collection"), "tournamentID") ? -1 : number(member(state, "collection"), "trade"),
        flag(member(state, "collection"), "practice"), true, nullptr, true);
    const float tableTop = std::max(0.f, rulesHeight - 32 * scale);
    const auto [boardAt, boardSize] =
        table(state, at + ImVec2(0, tableTop), ImVec2(size.x, size.y - tableTop - 40 * scale));
    const auto foot = at + ImVec2(0, size.y - 24 * scale);
    // Whose move it is reads from the middle of the table, in that player's colour.
    const bool thinking = flag(state, "thinking"), yours = number(state, "turn") == 0;
    const auto status = label(state, "phase") == "result" ? std::string()
                        : thinking                        ? label(state, "opponent") + " is thinking…"
                        : yours                           ? std::string("Your turn")
                                                          : label(state, "opponent") + "’s turn";
    if (!status.empty() && !presentation.pending(state))
        text(ImVec2(boardAt.x + boardSize.x * .5f, foot.y - 7 * scale), status, 21, controlColour(yours ? 0 : 1, true),
             0, true);
    const auto group = label(state, "activeGroup");
    if (!group.empty())
        text(foot, group + " " + (number(state, "groupModifier") > 0 ? "+1" : "−1"), 18, palette().accent);
    if (button("reference", "Rules", foot + ImVec2(size.x - 100 * scale, -7 * scale), ImVec2(100, 32) * scale, false,
               !presentation.pending(state))) {
        showRuleReference = true;
        demoStep = 0;
    }
    if (!presentation.pending(state)) return;
    const double elapsed = ImGui::GetTime() - presentation.start;
    std::string callout, tone = "rule";
    double start = 0, duration = .76;
    bool result = false;
    if (flag(state, "opening")) {
        const double swap = itemNumber(member(state, "swapped"), 0) >= 0 && number(state, "redeals") == 0 ? 2.2 : 0;
        if (elapsed < swap) {
            callout = "Swap";
            duration = swap;
        } else if (elapsed < swap + 1.15) {
            drawStarter(ImGui::GetWindowDrawList(), boardAt, boardSize, scale, float((elapsed - swap) / 1.15),
                        number(state, "turn"));
            return;
        } else {
            callout = number(state, "turn") == 0 ? "You start" : label(state, "opponent") + " starts";
            tone = "first";
            start = swap + 1.15;
            duration = 1.1;
        }
    } else {
        for (int i = 0; i < int(arrayMember(state, "stages").size()); ++i) {
            const auto& s = state["stages"][i];
            if (elapsed >= presentation.stageStarts[i] && elapsed < presentation.stageStarts[i] + .76 &&
                !label(s, "label").empty()) {
                callout = label(s, "label");
                start = presentation.stageStarts[i];
                tone = callout == "Combo" ? "combo"
                       : callout.find("Legion") != std::string::npos || callout.find("Decimation") != std::string::npos
                           ? "group"
                           : "rule";
            }
        }
        const double stagesEnd = presentation.duration - (flag(state, "suddenDeath") ? 1.6 : 0) -
                                 (label(state, "phase") == "result" ? 1.8 : 0);
        if (flag(state, "suddenDeath") && elapsed >= stagesEnd) {
            callout = "Sudden Death";
            start = stagesEnd;
            duration = 1.6;
        }
        if (label(state, "phase") == "result" && elapsed >= presentation.duration - 1.8) {
            const int yours = itemNumber(member(state, "score"), 0), theirs = itemNumber(member(state, "score"), 1);
            callout = yours == theirs ? "Draw" : yours > theirs ? "You Win!" : "You Lose";
            tone = yours == theirs ? "draw" : yours > theirs ? "win" : "lose";
            start = presentation.duration - 1.8;
            duration = 1.8;
            result = true;
        }
    }
    drawAnnouncement(ImGui::GetWindowDrawList(), font(), boardAt, boardSize, scale, callout,
                     float((elapsed - start) / duration), tone, result);
}
void GameApp::ruleSounds(const Json& data, Presentation& clock) {
    const double elapsed = ImGui::GetTime() - clock.start;
    for (int i = 0; i < int(clock.stageStarts.size()); ++i)
        if (!clock.stageSounds[i] && elapsed >= clock.stageStarts[i]) {
            const auto name = lower(label(data["stages"][i], "label"));
            for (const auto& pair : std::array<std::pair<const char*, const char*>, 6>{{{"same wall", "samewall"},
                                                                                        {"same", "same"},
                                                                                        {"plus", "plus"},
                                                                                        {"combo", "combo"},
                                                                                        {"legion", "legion"},
                                                                                        {"decimation", "decimation"}}})
                if (name.find(pair.first) != std::string::npos) {
                    send("rule", pair.second, false);
                    if (name == "same wall") break;
                }
            clock.stageSounds[i] = true;
        }
}
void GameApp::tickPresentation() {
    if (!sourceReady || !presentation.pending(state)) return;
    const double elapsed = ImGui::GetTime() - presentation.start;
    const double swap =
        flag(state, "opening") && itemNumber(member(state, "swapped"), 0) >= 0 && number(state, "redeals") == 0 ? 2.2
                                                                                                                : 0;
    if (flag(state, "opening") && !presentation.openingSound && elapsed >= swap + 1.15) {
        send("opening", {}, false);
        presentation.openingSound = true;
    }
    ruleSounds(state, presentation);
    if (label(state, "phase") == "result" && !presentation.resultSound && elapsed >= presentation.duration - 1.8) {
        send("result", {}, false);
        presentation.resultSound = true;
    }
    if (elapsed >= presentation.duration) {
        presentation.acknowledged = true;
        send("settled", {}, false);
    }
}
}
