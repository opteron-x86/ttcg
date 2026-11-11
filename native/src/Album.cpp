#include "App.h"
#include <cstdio>

namespace preview {
void App::refreshFilter() {
    filter.text = query;
    visible = filter.results(collection.cards, collection.connected);
}
void App::cardGrid(ImVec2 at, ImVec2 size, bool changed) {
    ImGui::SetCursorScreenPos(at - ImVec2(5, 0) * scale);
    ImGui::BeginChild("catalog", ImVec2(size.x + 10 * scale, size.y), ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoScrollWithMouse);
    const float gap = 18 * scale, content = ImGui::GetContentRegionAvail().x - 24 * scale;
    const bool editing = screen == 3;
    const float target =
        editing ? std::array<float, 3>{150, 112, 84}[deckDensity] : std::array<float, 3>{185, 140, 104}[cardDensity];
    const int columns = std::clamp(int((content + gap) / (target * scale + gap)), 2, 10);
    const float w = (content - gap * (columns - 1)) / columns, h = w / .75f, rowH = h + gap;
    const auto edges = gridScroll(rowH, changed);
    const int count = int(visible.size());
    ImGuiListClipper clip;
    clip.Begin((count + columns - 1) / columns, rowH);
    int material = MaterialSlot::grid;
    while (clip.Step())
        for (int row = clip.DisplayStart; row < clip.DisplayEnd; ++row) {
            const auto origin = ImGui::GetCursorScreenPos() + ImVec2(6, 6) * scale;
            for (int col = 0; col < columns; ++col) {
                const int item = row * columns + col;
                if (item >= count) break;
                const ImVec2 p = origin + ImVec2(col * (w + gap), 0);
                const int display = visible[item], id = display % int(collection.cards.size());
                const bool foil = display >= int(collection.cards.size());
                const auto& c = collection.cards[id];
                const bool known = !collection.connected || c.known[foil ? 1 : 0] || c.owned[foil ? 1 : 0] > 0;
                ImGui::PushID(display);
                ImGui::SetCursorScreenPos(p);
                if (ImGui::InvisibleButton("card", ImVec2(w, h), ImGuiButtonFlags_EnableNav)) {
                    selected = id;
                    selectedFoil = foil;
                    selectedPack = -1;
                }
                const bool twice = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0);
                const bool held = ImGui::IsItemFocused();
                // In the deck editor a double click, Enter or X puts the card straight into the deck.
                if (editing && known && c.owned[foil ? 1 : 0] > 0 &&
                    (twice || (held && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                        ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false))))) {
                    selected = id;
                    selectedFoil = foil;
                    addToDeck(c.id | (foil ? ttcg::foilFlag : 0));
                }
                if (known &&
                    ((ImGui::IsItemHovered() && ((twice && !editing) || ImGui::IsMouseClicked(1))) ||
                     (held && (ImGui::IsKeyPressed(ImGuiKey_I) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp)))))
                    inspectCard(display, false);
                if (known && collection.connected && collection.writable && !collection.busy &&
                    c.owned[foil ? 1 : 0] > 0 && ImGui::BeginDragDropSource()) {
                    const auto key = c.id | (foil ? ttcg::foilFlag : 0);
                    ImGui::SetDragDropPayload("TESSERA_CARD", &key, sizeof(key));
                    dragCard(id, foil);
                    ImGui::EndDragDropSource();
                }
                const bool chosen = (id == selected && foil == selectedFoil && (screen != 0 || selectedPack < 0)) ||
                                    (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible);
                if (known)
                    card(id, p, ImVec2(w, h), chosen, foil && face.foil, -1, material++);
                else {
                    auto texture = assets.get(customBack);
                    ImGui::GetWindowDrawList()->AddImage(texture.id, p, p + ImVec2(w, h));
                    if (chosen)
                        ImGui::GetWindowDrawList()->AddRect(p - ImVec2(3, 3), p + ImVec2(w + 3, h + 3),
                                                            palette().accent);
                }
                if (editing && known) {
                    // A folded corner marks cards already in the deck.
                    const auto key = c.id | (foil ? ttcg::foilFlag : 0);
                    if (std::find(collection.deck.begin(), collection.deck.end(), key) != collection.deck.end()) {
                        const float u = w / 180.f, fold = 40 * u;
                        auto* draw = ImGui::GetWindowDrawList();
                        draw->AddTriangleFilled(p, p + ImVec2(fold, 0), p + ImVec2(0, fold), skin().value);
                        const ImVec2 tick[]{p + ImVec2(7, 15) * u, p + ImVec2(12, 20) * u, p + ImVec2(21, 8) * u};
                        draw->AddPolyline(tick, 3, IM_COL32(12, 12, 12, 255), 0, std::max(1.5f, 2.6f * u));
                    }
                }
                if (collection.connected && c.owned[foil ? 1 : 0] > 0) {
                    const auto label = "×" + std::to_string(c.owned[foil ? 1 : 0]);
                    const float unit = w / 180.f, pixels = 14 * unit;
                    const auto measure = font()->CalcTextSizeA(pixels, 1e5f, 0, label.c_str());
                    const auto extent = measure + ImVec2(10, 6) * unit;
                    // On the framed face the count sits in the top corner, clear of the plates and footer.
                    const auto badge = !face.legacy() ? p + ImVec2(w - 9 * unit - extent.x, 9 * unit)
                                                      : p + ImVec2(w - 5 * unit - extent.x, h * .75f - extent.y);
                    auto* draw = ImGui::GetWindowDrawList();
                    draw->AddRectFilled(badge, badge + extent, IM_COL32(12, 12, 12, 200), unit);
                    draw->AddRect(badge, badge + extent, (palette().accent & 0xffffff) | 0x46000000, unit);
                    draw->AddText(font(), pixels, badge + ImVec2(5, 3) * unit, IM_COL32(239, 228, 204, 255),
                                  label.c_str());
                }
                ImGui::PopID();
            }
            ImGui::SetCursorScreenPos(
                ImVec2(origin.x - 6 * scale, origin.y - 6 * scale + rowH - ImGui::GetStyle().ItemSpacing.y));
            ImGui::Dummy(ImVec2(0, 0));
        }
    if (count == 0)
        text(ImGui::GetCursorScreenPos() + ImVec2(10, 30) * scale, "No matching cards", 24, palette().muted);
    gridFade(edges);
    ImGui::EndChild();
}
void App::dragCard(int index, bool foil, int owner, CardContext context) {
    // This is the drag tooltip's own draw list, so the preview stays above the
    // destination and is not clipped by the source grid's scrolling region.
    const auto at = ImGui::GetCursorScreenPos() + ImVec2(6, 6) * scale;
    const ImVec2 size = ImVec2(138, 184) * scale;
    ImGui::Dummy(size + ImVec2(12, 12) * scale);
    card(index, at, size, false, foil && face.foil, owner, MaterialSlot::drag, false, context);
    dragPreviewDrawn = true;
}
bool App::addToDeck(ttcg::CardID id) {
    if (!collection.ready || !collection.writable || collection.busy) return false;
    // A chosen slot is replaced; otherwise the card fills the first empty one.
    int slot = deckSlot;
    for (int i = 0; slot < 0 && i < 5; ++i)
        if (!collection.deck[i]) slot = i;
    if (slot < 0) {
        collection.notice = "The deck is full. Choose the slot to replace first.";
        return false;
    }
    if (!collection.canUse(id, slot)) {
        collection.notice = "That card cannot fill this slot. Check your owned copies and the deck limit.";
        return false;
    }
    auto hand = collection.deck;
    hand[slot] = id;
    deckSlot = -1;
    request({{"action", "deck"}, {"cards", hand}});
    return true;
}
void App::decks(ImVec2 at, ImVec2 size) {
    // Three columns: the deck itself, the cards to choose from, and the card being considered.
    const bool editable = collection.ready && collection.writable && !collection.busy;
    const float side = AlbumLayout::deck * scale, right = AlbumLayout::preview * scale, gap = AlbumLayout::gap * scale;
    const ImVec2 mid = at + ImVec2(side + gap, 0);
    const float width = size.x - side - right - 2 * gap;
    auto* d = ImGui::GetWindowDrawList();
    const auto divider = [&](float x) { skinRule(ImVec2(x, at.y), ImVec2(x, at.y + size.y), .45f); };
    divider(at.x + side + gap * .5f);
    auto active = std::find_if(collection.decks.begin(), collection.decks.end(),
                               [&](const auto& deck) { return deck.id == collection.activeDeck; });
    if (renamingDeck && renamingDeckID != collection.activeDeck) renamingDeck = false;
    // Which deck, and what to do with it.
    if (renamingDeck) {
        ImGui::SetCursorScreenPos(at);
        ImGui::SetNextItemWidth(side);
        if (focusDeckName) {
            ImGui::SetKeyboardFocusHere();
            focusDeckName = false;
        }
        const bool enter = ImGui::InputText("##deck-title", deckName, sizeof(deckName),
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        const bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false),
                   blur = ImGui::IsItemDeactivated();
        if (cancel)
            renamingDeck = false;
        else if (enter || blur) {
            if (editable && active != collection.decks.end() && ttcg::validDeckName(deckName) &&
                active->name != deckName)
                request({{"action", "rename"}, {"deckID", renamingDeckID}, {"name", deckName}});
            renamingDeck = false;
        }
    } else {
        std::vector<std::string> names;
        int chosen = 0;
        for (const auto& deck : collection.decks) {
            if (deck.id == collection.activeDeck) chosen = int(names.size());
            const auto count = std::count_if(deck.cards.begin(), deck.cards.end(), [](auto id) { return id != 0; });
            names.push_back(deck.name + (count < 5 ? "  (" + std::to_string(count) + "/5)" : ""));
        }
        if (names.empty()) names.push_back("Your deck");
        ImGui::SetCursorScreenPos(at);
        ImGui::BeginDisabled(!editable || collection.decks.size() < 2);
        if (select("##deck-pick", chosen, names, side - 48 * scale) && chosen < int(collection.decks.size()))
            request({{"action", "select"}, {"deckID", collection.decks[chosen].id}});
        ImGui::EndDisabled();
        if (button("new-deck", "+", at + ImVec2(side - 40 * scale, 0), ImVec2(40, 38) * scale, false,
                   editable && collection.decks.size() < collection.maxDecks)) {
            deckDialog = "new";
            std::snprintf(deckName, sizeof(deckName), "Deck %zu", collection.decks.size() + 1);
        }
        itemTooltip("New deck");
    }
    {
        const float each = (side - 16 * scale) / 3;
        const ImVec2 row = at + ImVec2(0, 46 * scale);
        if (button("rename", "Rename", row, ImVec2(each, 32 * scale), false,
                   editable && active != collection.decks.end()) &&
            !renamingDeck) {
            renamingDeck = true;
            focusDeckName = true;
            renamingDeckID = collection.activeDeck;
            std::snprintf(deckName, sizeof(deckName), "%s", active->name.c_str());
        }
        int n = 1;
        for (const auto& item :
             std::array<std::pair<const char*, const char*>, 2>{{{"copy", "Copy"}, {"delete", "Delete"}}}) {
            const bool enabled =
                editable && (std::string_view(item.first) != "delete" || collection.decks.size() > 1) &&
                (std::string_view(item.first) != "copy" || collection.decks.size() < collection.maxDecks);
            if (button(item.first, item.second, row + ImVec2(n * (each + 8 * scale), 0), ImVec2(each, 32 * scale),
                       false, enabled)) {
                deckDialog = item.first;
                std::snprintf(deckName, sizeof(deckName), "%s",
                              active == collection.decks.end() ? "Deck" : active->name.c_str());
                if (deckDialog == "copy")
                    std::snprintf(deckName, sizeof(deckName), "Copy of %s",
                                  active == collection.decks.end() ? "Deck" : active->name.c_str());
            }
            ++n;
        }
    }
    // The five slots, in the order they are held.
    const char* tiers[]{"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
    const float slotsTop = at.y + 92 * scale, strip = 40 * scale, profile = 158 * scale;
    const float rowH = std::clamp((at.y + size.y - slotsTop - strip - profile) / 5, 70 * scale, 128 * scale);
    int next = deckSlot;
    for (int i = 0; next < 0 && i < 5; ++i)
        if (!collection.deck[i]) next = i;
    for (int slot = 0; slot < 5; ++slot) {
        const ImVec2 pos(at.x, slotsTop + slot * rowH), extent(side, rowH - 6 * scale);
        const int index = collection.index(collection.deck[slot]);
        const bool foil = ttcg::isFoil(collection.deck[slot]);
        ImGui::PushID(slot);
        ImGui::SetCursorScreenPos(pos);
        if (ImGui::InvisibleButton("slot", extent, ImGuiButtonFlags_EnableNav)) deckSlot = deckSlot == slot ? -1 : slot;
        const bool hover = ImGui::IsItemHovered(), focus = ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
        if (index >= 0 && ((hover && (ImGui::IsMouseDoubleClicked(0) || ImGui::IsMouseClicked(1))) ||
                           (ImGui::IsItemFocused() &&
                            (ImGui::IsKeyPressed(ImGuiKey_I) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp)))))
            inspectCard(index + (foil ? collection.cards.size() : 0), false);
        if (index >= 0 && editable && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("TESSERA_SLOT", &slot, sizeof(slot));
            dragCard(index, foil);
            ImGui::EndDragDropSource();
        }
        bool receiving = false;
        if (ImGui::BeginDragDropTarget()) {
            receiving = true;
            if (const auto* payload = ImGui::AcceptDragDropPayload("TESSERA_CARD"))
                if (payload->DataSize == sizeof(ttcg::CardID)) {
                    const auto id = *static_cast<const ttcg::CardID*>(payload->Data);
                    if (collection.canUse(id, slot)) {
                        auto hand = collection.deck;
                        hand[slot] = id;
                        deckSlot = -1;
                        request({{"action", "deck"}, {"cards", hand}});
                    } else
                        collection.notice =
                            "That card cannot fill this slot. Check your owned copies and the deck limit.";
                }
            if (const auto* payload = ImGui::AcceptDragDropPayload("TESSERA_SLOT"))
                if (payload->DataSize == sizeof(int) && editable) {
                    const int source = *static_cast<const int*>(payload->Data);
                    if (source >= 0 && source < 5 && source != slot) {
                        auto hand = collection.deck;
                        std::swap(hand[source], hand[slot]);
                        deckSlot = slot;
                        request({{"action", "deck"}, {"cards", hand}});
                    }
                }
            ImGui::EndDragDropTarget();
        }
        const bool chosen = deckSlot == slot;
        if (chosen || hover || focus || receiving) {
            const ImU32 wash = (palette().selection & 0xffffff) | (chosen ? 0x90000000 : 0x48000000);
            d->AddRectFilledMultiColor(pos, pos + extent, wash, wash & 0xffffff, wash & 0xffffff, wash);
        }
        d->AddRectFilled(pos, pos + ImVec2(std::max(2.f, 2.5f * scale), extent.y),
                         chosen                         ? palette().accent
                         : slot == next && deckSlot < 0 ? palette().muted
                                                        : palette().line);
        const float ch = extent.y - 10 * scale, cw = ch * .75f;
        const ImVec2 art = pos + ImVec2(12 * scale, 5 * scale);
        const float textX = art.x + cw + 14 * scale, cross = 70 * scale, textW = pos.x + side - cross - textX;
        const std::string place = "Slot " + std::to_string(slot + 1);
        if (index >= 0) {
            const auto& c = collection.cards[index];
            card(index, art, ImVec2(cw, ch), false, foil && face.foil, -1, 1 + slot);
            d->PushClipRect(ImVec2(textX, pos.y), ImVec2(textX + textW, pos.y + extent.y), true);
            const float block = 68 * scale, ty = pos.y + (extent.y - block) * .5f;
            text(ImVec2(textX, ty), c.name, 18, palette().ink);
            text(ImVec2(textX, ty + 27 * scale),
                 place + " · Tier " + tiers[std::clamp(c.tier, 0, 10)] + (foil ? " · Foil" : ""), 14, palette().muted);
            float ix = textX;
            for (int a = 0; a < 5; ++a)
                if (c.affinityMask & (1u << a)) {
                    const auto icon = assets.get("../icons/affinity-" + std::to_string(a) + ".png");
                    d->AddImage(icon.id, ImVec2(ix, ty + 49 * scale), ImVec2(ix + 17 * scale, ty + 66 * scale));
                    ix += 21 * scale;
                }
            std::string group;
            for (unsigned g = 0; g < ttcg::creatureGroups.size(); ++g)
                if (c.groups & (1ull << g)) group += (group.empty() ? "" : ", ") + std::string(ttcg::creatureGroups[g]);
            text(ImVec2(ix + (ix > textX ? 4 * scale : 0), ty + 49 * scale), group, 14, palette().muted);
            d->PopClipRect();
            // The four ranks again, at a size that reads without leaning in.
            const ImVec2 centre(pos.x + side - cross * .5f - 2 * scale, pos.y + extent.y * .5f);
            const float reach = 19 * scale;
            const ImVec2 offsets[]{ImVec2(0, -reach), ImVec2(reach, 0), ImVec2(0, reach), ImVec2(-reach, 0)};
            for (int n = 0; n < 4; ++n) {
                const std::string rank = c.sides[n] == 10 ? "A" : std::to_string(c.sides[n]);
                const auto measure = font()->CalcTextSizeA(19 * scale, 1e5f, 0, rank.c_str());
                text(centre + offsets[n] - measure * .5f, rank, 19, palette().ink);
            }
        } else {
            d->AddRect(art, art + ImVec2(cw, ch), slot == next ? palette().muted : palette().line, 2 * scale);
            text(art + ImVec2(cw * .5f, ch * .5f - 15 * scale), "+", 24, palette().muted, 0, true);
            text(ImVec2(textX, pos.y + extent.y * .5f - 24 * scale), collection.deck[slot] ? "Unavailable" : "Empty",
                 18, palette().muted);
            text(ImVec2(textX, pos.y + extent.y * .5f + 3 * scale), place, 14, palette().muted);
        }
        ImGui::PopID();
    }
    // One reserved strip: choosing a slot never moves the rest of the panel.
    const float y = slotsTop + 5 * rowH + 2 * scale;
    if (deckSlot >= 0) {
        text(ImVec2(at.x, y + 6 * scale), "Slot " + std::to_string(deckSlot + 1), 16, palette().muted);
        const auto move = [&](const char* id, const char* label, float x, int to) {
            if (button(id, label, ImVec2(at.x + side - x * scale, y), ImVec2(38, 32) * scale, false,
                       editable && to >= 0 && to < 5)) {
                auto hand = collection.deck;
                std::swap(hand[deckSlot], hand[to]);
                deckSlot = to;
                request({{"action", "deck"}, {"cards", hand}});
            }
        };
        move("slot-left", "↑", 194, deckSlot - 1);
        itemTooltip("Move up");
        move("slot-right", "↓", 150, deckSlot + 1);
        itemTooltip("Move down");
        if (button("slot-remove", "Remove", ImVec2(at.x + side - 104 * scale, y), ImVec2(104, 32) * scale, false,
                   editable && collection.deck[deckSlot] != 0)) {
            auto hand = collection.deck;
            hand[deckSlot] = 0;
            request({{"action", "deck"}, {"cards", hand}});
        }
    } else
        text(ImVec2(at.x, y + 6 * scale),
             next >= 0 ? "Choose a card to fill slot " + std::to_string(next + 1) : "Choose a slot to change it", 15,
             palette().muted);
    // What the deck adds up to.
    std::array<int, ttcg::creatureGroups.size()> groups{};
    std::array<int, 5> affinities{};
    std::array<int, 4> totals{};
    int held = 0;
    for (auto id : collection.deck) {
        const auto i = collection.index(id);
        if (i < 0) continue;
        const auto& c = collection.cards[i];
        ++held;
        for (int n = 0; n < 4; ++n) totals[n] += c.sides[n];
        for (unsigned g = 0; g < groups.size(); ++g)
            if (c.groups & (1ull << g)) ++groups[g];
        for (int a = 0; a < 5; ++a)
            if (c.affinityMask & (1u << a)) ++affinities[a];
    }
    const ImVec2 summary(at.x, y + strip + 4 * scale);
    skinRule(summary, summary + ImVec2(side, 0), .5f);
    textCaps(summary + ImVec2(0, 14 * scale), "Average ranks", 12.5f, palette().muted, 1.2f);
    {
        const ImVec2 centre = summary + ImVec2(52 * scale, 88 * scale);
        const float reach = 30 * scale;
        const ImVec2 offsets[]{ImVec2(0, -reach), ImVec2(reach, 0), ImVec2(0, reach), ImVec2(-reach, 0)};
        const ImVec2 diamond[]{centre + ImVec2(0, -9 * scale), centre + ImVec2(9 * scale, 0),
                               centre + ImVec2(0, 9 * scale), centre + ImVec2(-9 * scale, 0)};
        d->AddPolyline(diamond, 4, palette().line, ImDrawFlags_Closed, std::max(1.f, scale));
        for (int n = 0; n < 4; ++n) {
            char value[16];
            std::snprintf(value, sizeof(value), held ? "%.1f" : "–", held ? totals[n] / float(held) : 0.f);
            const auto measure = font()->CalcTextSizeA(18 * scale, 1e5f, 0, value);
            text(centre + offsets[n] - measure * .5f, value, 18, held ? palette().ink : palette().muted);
        }
    }
    const float factsX = summary.x + 118 * scale, factsW = side - 118 * scale;
    float ax = factsX;
    for (int a = 0; a < 5; ++a)
        if (affinities[a]) {
            const ImVec2 pos(ax, summary.y + 40 * scale);
            ImGui::SetCursorScreenPos(pos);
            ImGui::InvisibleButton(("deck-affinity-" + std::to_string(a)).c_str(), ImVec2(35, 22) * scale);
            itemTooltip(std::string(ttcg::attributeNames[a]) + ": " + std::to_string(affinities[a]));
            auto icon = assets.get("../icons/affinity-" + std::to_string(a) + ".png");
            d->AddImage(icon.id, pos, pos + ImVec2(20, 20) * scale);
            text(pos + ImVec2(23, 2) * scale, std::to_string(affinities[a]), 15, palette().muted);
            ax += 39 * scale;
        }
    std::string groupSummary;
    for (unsigned g = 0; g < groups.size(); ++g)
        if (groups[g])
            groupSummary += (groupSummary.empty() ? "" : " · ") + std::string(ttcg::creatureGroups[g]) + " " +
                            std::to_string(groups[g]);
    d->PushClipRect(ImVec2(factsX, summary.y), ImVec2(factsX + factsW, at.y + size.y), true);
    text(ImVec2(factsX, summary.y + (ax > factsX ? 70 : 40) * scale), groupSummary, 14, palette().muted, factsW);
    d->PopClipRect();
    pickerFilters(mid, width);
    const auto grid = mid + ImVec2(0, 86 * scale);
    cardGrid(grid, ImVec2(width, at.y + size.y - grid.y));
    divider(at.x + size.x - right - gap * .5f);
    drawDetail(at + ImVec2(size.x - right, 0), ImVec2(right, size.y));
}
void App::pickerFilters(ImVec2 at, float width) {
    ImGui::SetCursorScreenPos(at);
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputTextWithHint("##deck-search", "Search cards", query, sizeof(query))) refreshFilter();
}
void App::deckModal() {
    if (!deckDialog.empty() && !ImGui::IsPopupOpen("Deck")) ImGui::OpenPopup("Deck");
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * .5f, ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(ImVec2(470, 210) * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24) * scale);
    if (ImGui::BeginPopupModal("Deck", nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | popupFlags())) {
        popupSurface();
        const bool remove = deckDialog == "delete";
        ImGui::TextUnformatted(remove                 ? "Delete this saved deck?"
                               : deckDialog == "new"  ? "New deck"
                               : deckDialog == "copy" ? "Copy deck"
                                                      : "Rename deck");
        ImGui::Dummy(ImVec2(1, 14 * scale));
        bool enter = false;
        if (remove)
            ImGui::TextUnformatted("Your cards stay in the album.");
        else {
            ImGui::SetNextItemWidth(-1);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            enter = ImGui::InputText("##deck-name", deckName, sizeof(deckName), ImGuiInputTextFlags_EnterReturnsTrue);
        }
        ImGui::Dummy(ImVec2(1, 12 * scale));
        const auto pos = ImGui::GetCursorScreenPos();
        if (button("cancel", "Cancel", pos, ImVec2(180, 39) * scale) || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
            deckDialog.clear();
            ImGui::CloseCurrentPopup();
        } else if (button("save-deck", remove ? "Delete" : "Save", pos + ImVec2(215, 0) * scale,
                          ImVec2(180, 39) * scale, true,
                          !collection.busy && (remove || ttcg::validDeckName(deckName))) ||
                   enter) {
            if (remove || ttcg::validDeckName(deckName)) {
                request({{"action", deckDialog}, {"name", deckName}});
                deckDialog.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}
}
