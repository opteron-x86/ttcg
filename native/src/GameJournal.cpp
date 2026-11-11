#include "GameApp.h"
#include "Announcements.h"
namespace preview {
void GameApp::journal(ImVec2 at, ImVec2 size) {
    const auto& c = member(state, "collection");
    const auto& regions = arrayMember(c, "regions");
    if (journalHold < 0) journalHold = number(c, "holdID");
    const float nav = AlbumLayout::navigation * scale, gap = AlbumLayout::gap * scale;
    line(at + ImVec2(nav + gap / 2, 0), at + ImVec2(nav + gap / 2, size.y));
    ImGui::SetCursorScreenPos(at);
    ImGui::BeginChild("holds", ImVec2(nav, size.y), ImGuiChildFlags_NavFlattened);
    const float navWidth = nav - 20 * scale;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 6 * scale));
    if (chapter("all", "All holds", ImGui::GetCursorScreenPos(), ImVec2(navWidth, 43 * scale), journalHold == 0)) {
        journalHold = 0;
        journalPlayer = -1;
        journalQuery[0] = 0;
    }
    for (const auto& hold : regions) {
        int id = number(hold, "id");
        if (chapter("hold" + std::to_string(id), label(hold, "name"), ImGui::GetCursorScreenPos(),
                    ImVec2(navWidth, 43 * scale), journalHold == id)) {
            journalHold = id;
            journalPlayer = -1;
            journalQuery[0] = 0;
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    auto mid = at + ImVec2(nav + gap, 0);
    float width = size.x - nav - gap;
    const Json* chosen = nullptr;
    for (const auto& h : regions)
        if (number(h, "id") == journalHold) chosen = &h;
    text(mid, chosen ? label(*chosen, "name") : journalHold == 11 ? "Travellers" : "All holds", 30, palette().accent);
    float y = mid.y + 53 * scale;
    if (chosen && !flag(*chosen, "travellers") && !flag(*chosen, "unaffiliated")) {
        text(ImVec2(mid.x, y),
             label(*chosen, "reputationTitle") + " · Wins: " + std::to_string(number(*chosen, "wins")), 21,
             palette().muted);
        y += 38 * scale;
        const auto& changes = arrayMember(*chosen, "ruleChanges");
        y += ruleChips(ImVec2(mid.x, y), width, number(*chosen, "rules"), number(*chosen, "trade"), false, true,
                       &changes) +
             10 * scale;
    }
    line(ImVec2(mid.x, y), ImVec2(mid.x + width, y));
    y += 15 * scale;
    ImGui::SetCursorScreenPos(ImVec2(mid.x, y));
    ImGui::SetNextItemWidth(width - 205 * scale);
    ImGui::InputTextWithHint("##players", "Find a player", journalQuery, sizeof(journalQuery));
    ImGui::SetCursorScreenPos(ImVec2(mid.x + width - 190 * scale, y));
    select("##player-filter", journalFilter, {"All players", "Played", "Unplayed"}, 190 * scale);
    y += 60 * scale;
    std::vector<int> rows;
    const auto& roster = arrayMember(c, "roster");
    for (int i = 0; i < int(roster.size()); ++i) {
        const auto& p = roster[i];
        if (journalHold == 11 ? !flag(p, "traveller") : journalHold && number(p, "holdID") != journalHold) continue;
        const bool played =
            !flag(p, "friendlyOnly") && number(p, "wins") + number(p, "losses") + number(p, "draws") > 0;
        if (journalFilter == 1 && !played || journalFilter == 2 && played) continue;
        if (journalQuery[0] &&
            lower(label(p, "name") + " " + label(p, "hold") + " " + label(p, "skill")).find(lower(journalQuery)) ==
                std::string::npos)
            continue;
        rows.push_back(i);
    }
    std::sort(rows.begin(), rows.end(),
              [&](int a, int b) { return label(roster[a], "name") < label(roster[b], "name"); });
    if (std::find(rows.begin(), rows.end(), journalPlayer) == rows.end())
        journalPlayer = rows.empty() ? -1 : rows.front();
    const float detailWidth = journalPlayer >= 0 ? 260 * scale : 0,
                listWidth = width - (detailWidth ? detailWidth + gap : 0);
    textRight(ImVec2(mid.x + width - 12 * scale, y - 7 * scale),
              std::to_string(rows.size()) + (rows.size() == 1 ? " player" : " players"), 15, palette().muted);
    y += 24 * scale;
    ImGui::SetCursorScreenPos(ImVec2(mid.x, y));
    ImGui::BeginChild("players", ImVec2(listWidth, at.y + size.y - y), ImGuiChildFlags_NavFlattened);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8 * scale));
    const float rowHeight = 68 * scale, rowWidth = ImGui::GetContentRegionAvail().x - 12 * scale;
    ImGuiListClipper clip;
    clip.Begin(int(rows.size()), rowHeight + 8 * scale);
    while (clip.Step())
        for (int row = clip.DisplayStart; row < clip.DisplayEnd; ++row) {
            const int i = rows[row];
            const auto& p = roster[i];
            const auto pos = ImGui::GetCursorScreenPos();
            const bool friendly = flag(p, "friendlyOnly");
            if (chapter("player" + std::to_string(i), label(p, "name"), pos, ImVec2(rowWidth, rowHeight),
                        journalPlayer == i, label(p, "skill"), 180 * scale))
                journalPlayer = i;
            std::string record = friendly               ? "Practice"
                                 : !flag(p, "unlocked") ? "Unavailable"
                                 : number(p, "wins") + number(p, "losses") + number(p, "draws")
                                     ? std::to_string(number(p, "wins")) + " W · " +
                                           std::to_string(number(p, "losses")) + " L · " +
                                           std::to_string(number(p, "draws")) + " D"
                                     : "Not played";
            text(pos + ImVec2(rowWidth - 180 * scale, 14 * scale), record, 16, palette().muted, 168 * scale);
        }
    if (rows.empty())
        text(ImGui::GetCursorScreenPos() + ImVec2(listWidth / 2, 48 * scale), "No known players", 23, palette().muted,
             0, true);
    ImGui::PopStyleVar();
    ImGui::EndChild();
    if (journalPlayer >= 0 && journalPlayer < int(roster.size())) {
        const auto& p = roster[journalPlayer];
        const auto detail = ImVec2(mid.x + width - detailWidth, y);
        line(detail - ImVec2(gap / 2, 0), detail + ImVec2(-gap / 2, at.y + size.y - y));
        // Give flowing text a real column origin and clip rectangle. A cursor
        // offset in the root window is lost at the next ImGui layout item.
        ImGui::SetCursorScreenPos(detail);
        ImGui::BeginChild("player-details", ImVec2(detailWidth, at.y + size.y - y), ImGuiChildFlags_NavFlattened);
        const auto top = ImGui::GetCursorScreenPos();
        text(top, label(p, "name"), 25, palette().accent, detailWidth);
        float ry =
            top.y + font()->CalcTextSizeA(25 * scale, 1e5f, detailWidth, label(p, "name").c_str()).y + 12 * scale;
        skinRule(ImVec2(top.x, ry), ImVec2(top.x + detailWidth, ry), .7f);
        ry += 8 * scale;
        // The same labelled rows the card details use.
        const int wins = number(p, "wins"), losses = number(p, "losses"), draws = number(p, "draws"),
                  games = wins + losses + draws;
        const bool friendly = flag(p, "friendlyOnly");
        std::vector<std::pair<const char*, std::string>> facts{
            {"Found at", label(p, "location")},
            {"Hold", flag(p, "traveller") ? "Traveller" : label(p, "hold")},
            {"Skill", label(p, "skill")},
            {"Games", friendly              ? "Practice only"
                      : flag(p, "unlocked") ? "Available"
                                            : "Not ready"}};
        if (!friendly) {
            facts.push_back({"Played", std::to_string(games)});
            facts.push_back({"Won", std::to_string(wins)});
            facts.push_back({"Lost", std::to_string(losses)});
            facts.push_back({"Drawn", std::to_string(draws)});
            if (games) facts.push_back({"Win rate", std::to_string(wins * 100 / games) + "%"});
        }
        for (const auto& [name, value] : facts) {
            if (value.empty()) continue;
            textCaps(ImVec2(top.x, ry + 9 * scale), name, 13.5f, palette().muted, 1.2f);
            const float room = detailWidth - capsWidth(name, 13.5f, 1.2f) - 14 * scale,
                        natural = font()->CalcTextSizeA(17 * scale, 1e5f, 0, value.c_str()).x;
            textRight(ImVec2(top.x + detailWidth - 2 * scale, ry + 5 * scale), value,
                      std::clamp(17.f * room / std::max(1.f, natural), 15.f, 17.f), palette().ink);
            ry += 31 * scale;
            ImGui::GetWindowDrawList()->AddLine(ImVec2(top.x, ry - 1), ImVec2(top.x + detailWidth, ry - 1),
                                                fade(palette().line, .55f));
        }
        ImGui::SetCursorScreenPos(ImVec2(top.x, ry));
        ImGui::Dummy(ImVec2(1, 1));
        ImGui::EndChild();
    }
}
}
