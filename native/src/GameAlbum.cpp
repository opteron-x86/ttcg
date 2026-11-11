#include "GameApp.h"
#include "Announcements.h"
#include <bit>
namespace preview {
void GameApp::nativeFilters(ImVec2 at, float width, bool editing) {
    const int page = editing ? 1 : 0;
    if (filterPage != page) {
        if (filterPage == 0)
            cardFilter = filter;
        else if (filterPage == 1)
            deckFilter = filter;
        filter = editing ? deckFilter : cardFilter;
        std::snprintf(query, sizeof(query), "%s", filter.text.c_str());
        filterPage = page;
        filterDirty = true;
        refreshFilter();
    }
    const std::vector<std::string> ownership{"Owned", "Discovered", "Missing", "Extras"},
        tiers{"Tier", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"},
        sorts{"Low to high", "High to low", "Name", "Quantity", "Rarity"};
    // The deck editor only offers cards you own, so it has no ownership choice.
    if (editing) filter.ownership = 0;
    const float ownW = editing ? 0 : choiceWidth(ownership) + 10 * scale, tierW = choiceWidth(tiers),
                sortW = choiceWidth(sorts), tools = 80 * scale + 10 * scale + 118 * scale,
                searchW = width - ownW - tierW - sortW - 30 * scale - tools;
    ImGui::SetCursorScreenPos(at);
    ImGui::SetNextItemWidth(searchW);
    ImGui::InputTextWithHint("##search", "Search cards", query, sizeof(query));
    float x = searchW + 10 * scale;
    if (!editing) {
        ImGui::SetCursorScreenPos(at + ImVec2(x, 0));
        select("##ownership", filter.ownership, ownership, ownW - 10 * scale);
        x += ownW;
    }
    ImGui::SetCursorScreenPos(at + ImVec2(x, 0));
    select("##tier", filter.tier, tiers, tierW);
    x += tierW + 10 * scale;
    ImGui::SetCursorScreenPos(at + ImVec2(x, 0));
    select("##sort", filter.sort, sorts, sortW);
    if (button("filters", "Filters", at + ImVec2(width - 208 * scale, 0), ImVec2(80, 38) * scale))
        ImGui::OpenPopup("Card filters");
    ImGui::SetNextWindowSize(ImVec2(370 * scale, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20) * scale);
    if (ImGui::BeginPopup("Card filters")) {
        std::vector<std::string> groups{"All groups"};
        for (auto name : ttcg::creatureGroups) groups.emplace_back(name);
        int group = filter.group + 1;
        if (combo("##group", "Group", group, groups, 330 * scale)) filter.group = group - 1;
        combo("##rarity", "Rarity", filter.rarity, {"All rarities", "Common", "Rare", "Epic", "Legendary"},
              330 * scale);
        combo("##finish", "Finish", filter.finish, {"All finishes", "Standard", "Foil"}, 330 * scale);
        int affinity = filter.affinity + 1;
        if (combo("##affinity", "Affinity", affinity,
                  {"All affinities", "Neutral", "Strength", "Intelligence", "Willpower", "Agility", "Endurance"},
                  330 * scale))
            filter.affinity = affinity - 1;
        std::vector<std::string> sets{"All sets", "Tessera"}, keys{"", "tessera"};
        for (const auto& c : collection.cards)
            if (!c.setKey.empty() && std::find(keys.begin(), keys.end(), c.setKey) == keys.end()) {
                keys.push_back(c.setKey);
                sets.push_back(c.set);
            }
        int set = int(std::find(keys.begin(), keys.end(), filter.set) - keys.begin());
        if (set >= int(keys.size())) set = 0;
        if (combo("##set", "Set", set, sets, 330 * scale)) filter.set = keys[set];
        if (ImGui::Button("Reset filters")) {
            filter = Filter{};
            query[0] = 0;
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    filter.text = query;
    {
        // Card size, on both the album and the deck editor.
        int& density = editing ? deckDensity : cardDensity;
        const char* ids[]{"view-large", "view-medium", "view-small"};
        const char* labels[]{"Large cards", "Medium cards", "Small cards"};
        for (int i = 0; i < 3; ++i) {
            const auto pos = at + ImVec2(width - (118 - i * 40.f) * scale, 3 * scale);
            if (button(ids[i], "", pos, ImVec2(34, 32) * scale, density == i)) density = i;
            itemTooltip(labels[i]);
            const int columns = i + 1;
            const float gap = 2 * scale, unit = (18 * scale - gap * i) / columns;
            const auto origin = pos + ImVec2(8, 7) * scale;
            for (int row = 0; row < columns; ++row)
                for (int col = 0; col < columns; ++col) {
                    const auto cell = origin + ImVec2(col * (unit + gap), row * (unit + gap));
                    ImGui::GetWindowDrawList()->AddRectFilled(cell, cell + ImVec2(unit, unit),
                                                              density == i ? palette().accent : palette().muted,
                                                              scale * .5f);
                }
        }
    }
    {
        // Filters set in the popup stay visible as chips; a click clears one.
        static const char* rarities[]{"", "Common", "Rare", "Epic", "Legendary"};
        static const char* finishes[]{"", "Standard", "Foil"};
        struct Chip {
            std::string name;
            int kind;
        };
        std::vector<Chip> chips;
        if (filter.group >= 0 && filter.group < int(ttcg::creatureGroups.size()))
            chips.push_back({std::string(ttcg::creatureGroups[filter.group]), 0});
        if (filter.rarity > 0 && filter.rarity < 5) chips.push_back({rarities[filter.rarity], 1});
        if (filter.finish > 0 && filter.finish < 3) chips.push_back({finishes[filter.finish], 2});
        if (filter.affinity == 0) chips.push_back({"Neutral", 3});
        if (filter.affinity > 0 && filter.affinity <= 5)
            chips.push_back({ttcg::attributeNames[filter.affinity - 1], 3});
        if (!filter.set.empty()) {
            std::string name = filter.set == "tessera" ? "Tessera" : filter.set;
            for (const auto& c : collection.cards)
                if (c.setKey == filter.set) name = c.set;
            chips.push_back({name, 4});
        }
        float cx = at.x;
        for (const auto& chip : chips) {
            const auto shown = chip.name + "  ×";
            const float w = font()->CalcTextSizeA(15 * scale, 1e5f, 0, shown.c_str()).x + 20 * scale;
            const ImVec2 pos(cx, at.y + 49 * scale);
            ImGui::PushID(chip.kind);
            ImGui::SetCursorScreenPos(pos);
            const bool hit = ImGui::InvisibleButton("filter-chip", ImVec2(w, 28 * scale), ImGuiButtonFlags_EnableNav);
            const bool lit = ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible);
            auto* d = ImGui::GetWindowDrawList();
            d->AddRectFilled(pos, pos + ImVec2(w, 28 * scale), fade(palette().selection, lit ? .8f : .45f), 14 * scale);
            d->AddRect(pos, pos + ImVec2(w, 28 * scale), fade(palette().accent, lit ? .7f : .3f), 14 * scale);
            text(pos + ImVec2(10, 4) * scale, shown, 15, palette().ink);
            ImGui::PopID();
            if (hit) {
                if (chip.kind == 0) filter.group = -1;
                if (chip.kind == 1) filter.rarity = 0;
                if (chip.kind == 2) filter.finish = 0;
                if (chip.kind == 3) filter.affinity = -1;
                if (chip.kind == 4) filter.set.clear();
            }
            cx += w + 8 * scale;
        }
    }
    const auto countLabel = [&] {
        std::string total = std::to_string(visible.size()) + " cards";
        if (!editing) {
            int known = 0;
            for (const auto& c : collection.cards) known += c.known[0] || c.known[1];
            total += " · " + std::to_string(known) + " of " + std::to_string(collection.cards.size()) + " discovered";
        }
        textRight(at + ImVec2(width - 14 * scale, 54 * scale), total, 16, palette().muted);
    };
    if (!filterDirty && filter == appliedFilter) {
        countLabel();
        return;
    }
    appliedFilter = filter;
    filterDirty = false;
    visible.clear();
    const int count = int(collection.cards.size());
    for (int id = 0; id < count * 2; ++id) {
        const auto& c = collection.cards[id % count];
        const bool foil = id >= count;
        const int n = c.owned[foil];
        if (!c.known[foil] && n <= 0) continue;
        Filter f = filter;
        f.ownership = 1;
        f.text.clear();
        if (!f.matches(c, foil, true)) continue;
        if (filter.ownership == 0 && !n || filter.ownership == 2 && n) continue;
        if (filter.ownership == 3 &&
            (foil || n <= std::max(1, itemNumber(member(member(state, "collection"), "deckReserved"), id, 0))))
            continue;
        if (filter.affinity == 0 && c.affinityMask ||
            filter.affinity > 0 && !(c.affinityMask & (1u << (filter.affinity - 1))))
            continue;
        if (!filter.set.empty() && (c.setKey.empty() ? "tessera" : c.setKey) != filter.set) continue;
        if (query[0]) {
            const auto& raw = state["cards"][id];
            const auto hay = lower(c.name + " " + c.rarity + " " + join(member(raw, "groups")) + " " +
                                   join(member(raw, "subtypes")) + " " + join(member(raw, "affinities")));
            if (hay.find(lower(query)) == std::string::npos) continue;
        }
        visible.push_back(id);
    }
    std::stable_sort(visible.begin(), visible.end(), [&](int a, int b) {
        const auto& x = collection.cards[a % count];
        const auto& y = collection.cards[b % count];
        int order = 0;
        if (filter.sort <= 1)
            order = x.tier - y.tier;
        else if (filter.sort == 3)
            order = x.owned[a / count] - y.owned[b / count];
        else if (filter.sort == 4) {
            const std::vector<std::string> rarity{"Common", "Rare", "Epic", "Legendary"};
            order = int(std::find(rarity.begin(), rarity.end(), x.rarity) -
                        std::find(rarity.begin(), rarity.end(), y.rarity));
        }
        if (order) return filter.sort == 1 || filter.sort == 3 ? order > 0 : order < 0;
        return x.name != y.name ? x.name < y.name : a < b;
    });
    countLabel();
}
void GameApp::nativeAlbum(ImVec2 at, ImVec2 size) {
    const float right = AlbumLayout::preview * scale, gap = AlbumLayout::gap * scale, width = size.x - right - gap;
    nativeFilters(at, width);
    const float top = 86 * scale + packStrip(at + ImVec2(0, 86 * scale), width);
    cardGrid(at + ImVec2(0, top), ImVec2(width, size.y - top));
    skinRule(at + ImVec2(width + gap * .5f, 0), at + ImVec2(width + gap * .5f, size.y), .45f);
    drawDetail(at + ImVec2(width + gap, 0), ImVec2(right, size.y));
}
void GameApp::nativeDecks(ImVec2 at, ImVec2 size) {
    if (!member(state, "interruptedMatch").is_null()) {
        text(at, "Finish your current game to edit decks.", 22, palette().muted);
        return;
    }
    decks(at, size);
}
void GameApp::drawDetail(ImVec2 at, ImVec2 size) {
    if (collection.cards.empty() || selected < 0) return;
    const auto& c = collection.cards[selected];
    const bool known = c.known[selectedFoil] || c.owned[selectedFoil];
    const float facts = factRows(c, {}, size.x, false);
    const float w = std::min(size.x - 12 * scale, (size.y - facts - 186 * scale) * .75f), h = w / .75f;
    const auto p = at + ImVec2((size.x - w) / 2, 8 * scale);
    if (known && skinPiece("detail"))
        skinPanel(ImVec2(at.x - 8 * scale, p.y + h + 10 * scale), ImVec2(size.x + 16 * scale, facts + 66 * scale), .7f,
                  IM_COL32_WHITE, "detail");
    ImGui::SetCursorScreenPos(p);
    if (ImGui::InvisibleButton("selected-art", ImVec2(w, h), ImGuiButtonFlags_EnableNav) && known)
        inspectCard(selected + (selectedFoil ? collection.cards.size() : 0), false);
    if (known) {
        softShadow(p, ImVec2(w, h), 1, 10);
        card(selected, p, ImVec2(w, h), false, selectedFoil, -1, 0, true);
    } else
        displayCard(-2, p, ImVec2(w, h));
    if (!known) return;
    const float mid = at.x + size.x / 2;
    float y = p.y + h + 14 * scale;
    // An item card: a title band, then labelled facts.
    const bool inset = skinPiece("detail");
    if (!skinBand("band", ImVec2(at.x, y), ImVec2(size.x, 40 * scale)))
        skinBand("selector", ImVec2(at.x, y), ImVec2(size.x, 40 * scale));
    const float fit = std::min(19.f, 19.f * (size.x - 24 * scale) / std::max(1.f, capsWidth(c.name, 19, 1)));
    textCaps(ImVec2(mid, y + (40 - fit * 1.25f) * .5f * scale), c.name, fit, palette().ink, 1, true);
    y += 44 * scale;
    y += factRows(c, ImVec2(at.x + (inset ? 10 : 2) * scale, y), size.x - (inset ? 20 : 4) * scale) + 16 * scale;
    if (tab == 1) {
        int slot = deckSlot;
        for (int i = 0; slot < 0 && i < 5; ++i)
            if (!collection.deck[i]) slot = i;
        const auto id = c.id | (selectedFoil ? ttcg::foilFlag : 0);
        // Against the card it would replace: what each rank gains or gives up.
        const int held = slot >= 0 ? collection.index(collection.deck[slot]) : -1;
        if (held >= 0 && collection.deck[slot] != id) {
            const auto& other = collection.cards[held];
            textCaps(ImVec2(at.x + 2 * scale, y + 5 * scale), "Vs slot " + std::to_string(slot + 1), 12.5f,
                     palette().muted, 1.2f);
            const char* arrows[]{"↑", "→", "↓", "←"};
            float x = at.x + size.x - 2 * scale;
            for (int n = 3; n >= 0; --n) {
                const int change = c.sides[n] - other.sides[n];
                const auto value = std::string(arrows[n]) +
                                   (change > 0   ? "+"
                                    : change < 0 ? "−"
                                                 : "") +
                                   std::to_string(std::abs(change));
                x -= font()->CalcTextSizeA(16 * scale, 1e5f, 0, value.c_str()).x;
                text(ImVec2(x, y + 2 * scale), value, 16,
                     change > 0   ? IM_COL32(150, 214, 128, 255)
                     : change < 0 ? IM_COL32(236, 132, 112, 255)
                                  : palette().muted);
                x -= 9 * scale;
            }
        }
        y += 34 * scale;
        const auto title = slot < 0                ? std::string("Choose a slot")
                           : collection.deck[slot] ? "Replace slot " + std::to_string(slot + 1)
                                                   : "Add to slot " + std::to_string(slot + 1);
        if (button("add", title.c_str(), ImVec2(at.x, y), ImVec2(size.x, 40 * scale), true,
                   slot >= 0 && collection.canUse(id, slot) && !pendingCommand))
            addToDeck(id);
    } else if (button("deck-card", "Use in deck", ImVec2(at.x, y + 34 * scale), ImVec2(size.x, 40 * scale), false,
                      collection.writable && c.owned[selectedFoil] > 0)) {
        tab = 1;
        screen = 3;
        selectedPack = -1;
    }
}
}
