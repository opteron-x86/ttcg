#include "App.h"
#include "CardName.h"
#include <algorithm>
#include <cmath>

namespace preview {
namespace {
constexpr ImU32 rgb(unsigned n, unsigned alpha = 255) { return IM_COL32(n >> 16, (n >> 8) & 255, n & 255, alpha); }
ImU32 alpha(ImU32 colour, float amount) {
    return (colour & 0xffffff) | (unsigned(std::clamp(amount, 0.f, 1.f) * 255) << 24);
}
ImU32 mix(ImU32 a, ImU32 b, float t) {
    const auto channel = [&](int shift) {
        const float x = float((a >> shift) & 255), y = float((b >> shift) & 255);
        return unsigned(std::clamp(x + (y - x) * t, 0.f, 255.f)) << shift;
    };
    return channel(0) | channel(8) | channel(16) | 0xff000000;
}
const char* romans[]{"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
// Bezel metals: light edge, body and the gem's pigment.
struct Metal {
    unsigned light, body, gem;
};
Metal metal(std::string_view rarity) {
    return rarity == "Rare"        ? Metal{0xa9d6e6, 0x3f7f96, 0x56dfff}
           : rarity == "Epic"      ? Metal{0xd3b3ee, 0x6d4692, 0xd684fa}
           : rarity == "Legendary" ? Metal{0xf3dc9a, 0x9a7426, 0xe6bc65}
                                   : Metal{0xb9bdb6, 0x595e59, 0xb7b89e};
}
}
ImVec2 App::valueAnchor(int side, ImVec2 size) const {
    const float unit = size.x / 180.f;
    if (face.classic) {
        constexpr ImVec2 cluster[]{ImVec2(33, 20), ImVec2(49, 39), ImVec2(33, 58), ImVec2(17, 39)};
        return (cluster[side] + (face.frame == 0 ? ImVec2(4, 5) : ImVec2(0, 0))) * unit;
    }
    if (face.frame != 0) {
        const float middle = size.y * (face.names ? .42f : .5f);
        const ImVec2 legacy[]{ImVec2(size.x * .5f, 20 * unit), ImVec2(size.x - 15 * unit, middle),
                              ImVec2(size.x * .5f, size.y * (face.names    ? .65f
                                                             : face.groups ? .82f
                                                                           : .92f)),
                              ImVec2(15 * unit, middle)};
        return legacy[side];
    }
    // Plates sit flush with their edges, so touching values meet across the gap.
    const ImVec2 framed[]{ImVec2(size.x * .5f, 16 * unit), ImVec2(size.x - 15.5f * unit, size.y * .46f),
                          ImVec2(size.x * .5f, size.y - 19.5f * unit), ImVec2(15.5f * unit, size.y * .46f)};
    return framed[side];
}
void App::framedCard(const CardView& c, Texture texture, ImVec2 at, ImVec2 size, bool chosen, int owner) {
    auto* d = ImGui::GetWindowDrawList();
    const float unit = size.x / 180.f, bezel = 5 * unit;
    const auto end = at + size;
    const auto rare = metal(c.rarity);
    // In a match the frame belongs to whoever controls the card; rarity stays on the gem.
    const ImU32 body = owner >= 0 ? mix(controlColour(owner), rgb(0x101010), .28f) : rgb(rare.body);
    const ImU32 light = owner >= 0 ? mix(controlColour(owner), rgb(0xffffff), .25f) : rgb(rare.light);
    const ImU32 dark = mix(body, rgb(0x000000), .55f);
    const ImU32 base = owner >= 0 ? mix(controlColour(owner), rgb(0x0b0b0b), .84f) : rgb(0x0f0f0f);
    d->AddRectFilled(at + ImVec2(3, 5) * unit, end + ImVec2(3, 5) * unit, rgb(0x000000, 70), 2 * unit);
    d->AddRectFilledMultiColor(at, end, light, body, dark, body);
    d->AddRect(at, end, rgb(0x000000, 200), 0, 0, std::max(1.f, unit));
    const ImVec2 inner = at + ImVec2(bezel, bezel), innerEnd = end - ImVec2(bezel, bezel);
    d->AddRectFilled(inner, innerEnd, rgb(0x121212));
    if (texture.id) {
        const float ratio = float(texture.width) / texture.height, target = (size.x - 2 * bezel) / (size.y - 2 * bezel);
        ImVec2 uv0(0, 0), uv1(1, 1);
        if (ratio > target) {
            const float crop = target / ratio;
            uv0.x = (1 - crop) * .5f;
            uv1.x = uv0.x + crop;
        } else {
            const float crop = ratio / target;
            uv0.y = (1 - crop) * .4f;
            uv1.y = uv0.y + crop;
        }
        d->AddImage(texture.id, inner, innerEnd, uv0, uv1);
    }
    d->AddRect(inner, innerEnd, rgb(0x000000, 170), 0, 0, std::max(1.f, unit));
    // Footer: tier gem, bottom value and affinity share one opaque strip.
    const float footer = 27 * unit, footerTop = innerEnd.y - footer;
    auto* nameFont = cardNameFont ? cardNameFont : serif;
    if (face.names || face.groups) {
        const float pixels = std::clamp(size.x * .069f, 9.f * scale, 28.f * scale);
        auto lines =
            face.names ? nameLines(nameFont, c.name, pixels, size.x * .84f, face.lettering) : std::vector<NameLine>{};
        if (lines.size() > 2) lines = nameLines(nameFont, c.name, pixels * .8f, size.x * .84f, face.lettering);
        const float lineHeight = (lines.size() > 2 ? pixels * .8f : pixels) * 1.16f;
        std::vector<NameLine> groupLines;
        const float groupPixels = std::clamp(size.x * .052f, 8.f * scale, 18.f * scale);
        if (face.groups) {
            std::string list;
            for (unsigned i = 0; i < ttcg::creatureGroups.size(); ++i)
                if (c.groups & (1ull << i)) list += (list.empty() ? "" : " / ") + std::string(ttcg::creatureGroups[i]);
            groupLines = nameLines(nameFont, list, groupPixels, size.x * .84f, 1);
            if (groupLines.size() > 2) groupLines.resize(2);
        }
        const float text = lines.size() * lineHeight + groupLines.size() * groupPixels * 1.16f;
        const float band = text + 14 * unit, top = footerTop - band;
        const auto clear = alpha(base, 0), solid = alpha(base, .86f);
        d->AddRectFilledMultiColor(ImVec2(inner.x, top - 16 * unit), ImVec2(innerEnd.x, top), clear, clear, solid,
                                   solid);
        d->AddRectFilled(ImVec2(inner.x, top), ImVec2(innerEnd.x, footerTop), solid);
        float y = top + 4 * unit;
        if (!lines.empty()) {
            drawNameLines(d, nameFont, inner, innerEnd - inner, lines, y, lines.size() > 2 ? pixels * .8f : pixels,
                          scale, rgb(0xf5eedd));
            y += lines.size() * lineHeight;
        }
        if (!groupLines.empty())
            drawNameLines(d, nameFont, inner, innerEnd - inner, groupLines, y, groupPixels, scale, rgb(0xd9d2bf, 210));
    }
    d->AddRectFilled(ImVec2(inner.x, footerTop), innerEnd, base);
    d->AddRectFilled(ImVec2(inner.x, footerTop), ImVec2(innerEnd.x, footerTop + std::max(1.f, 1.2f * unit)),
                     alpha(light, .8f));
    // Value plates: tabs that grow out of the bezel on each edge.
    auto* valueFont = cardValueFont ? cardValueFont : bold;
    const float digit = 25 * unit, rim = std::max(1.f, 1.3f * unit);
    const auto value = [&](int side) {
        const std::string text = c.sides[side] == 10 ? "A" : std::to_string(c.sides[side]);
        const auto extent = valueFont->CalcTextSizeA(digit, 1e5f, 0, text.c_str());
        const auto pos = at + valueAnchor(side, size) - extent * .5f;
        if (valueFont == jost || valueFont == lato)
            for (float shift : {-.5f, .5f})
                d->AddText(valueFont, digit, pos + ImVec2(shift * unit, 0), rgb(0xf6f1e2), text.c_str());
        d->AddText(valueFont, digit, pos, rgb(0xf6f1e2), text.c_str());
    };
    const auto plate = [&](const ImVec2* points, int count) {
        d->AddConvexPolyFilled(points, count, base);
        d->AddPolyline(points, count, mix(light, body, .3f), ImDrawFlags_None, rim);
    };
    if (face.classic) {
        // The classic cluster keeps its corner, on one diamond plaque.
        const ImVec2 centre = at + ImVec2(37, 44) * unit;
        const float r = 37 * unit;
        const ImVec2 diamond[]{centre + ImVec2(0, -r), centre + ImVec2(r, 0), centre + ImVec2(0, r),
                               centre + ImVec2(-r, 0)};
        d->AddConvexPolyFilled(diamond, 4, alpha(base, .92f));
        d->AddPolyline(diamond, 4, light, ImDrawFlags_Closed, rim);
    } else {
        const float cx = at.x + size.x * .5f, cy = at.y + size.y * .46f, w = 15 * unit, cut = 7 * unit;
        const ImVec2 top[]{ImVec2(cx - w, at.y + bezel * .5f),     ImVec2(cx - w, at.y + 23 * unit),
                           ImVec2(cx - w + cut, at.y + 30 * unit), ImVec2(cx + w - cut, at.y + 30 * unit),
                           ImVec2(cx + w, at.y + 23 * unit),       ImVec2(cx + w, at.y + bezel * .5f)};
        const ImVec2 bottom[]{ImVec2(cx - w, end.y - bezel * .5f),     ImVec2(cx - w, end.y - 31 * unit),
                              ImVec2(cx - w + cut, end.y - 38 * unit), ImVec2(cx + w - cut, end.y - 38 * unit),
                              ImVec2(cx + w, end.y - 31 * unit),       ImVec2(cx + w, end.y - bezel * .5f)};
        const ImVec2 left[]{ImVec2(at.x + bezel * .5f, cy - w),     ImVec2(at.x + 22 * unit, cy - w),
                            ImVec2(at.x + 29 * unit, cy - w + cut), ImVec2(at.x + 29 * unit, cy + w - cut),
                            ImVec2(at.x + 22 * unit, cy + w),       ImVec2(at.x + bezel * .5f, cy + w)};
        const ImVec2 right[]{ImVec2(end.x - bezel * .5f, cy - w),     ImVec2(end.x - 22 * unit, cy - w),
                             ImVec2(end.x - 29 * unit, cy - w + cut), ImVec2(end.x - 29 * unit, cy + w - cut),
                             ImVec2(end.x - 22 * unit, cy + w),       ImVec2(end.x - bezel * .5f, cy + w)};
        plate(top, 6);
        plate(bottom, 6);
        plate(left, 6);
        plate(right, 6);
    }
    for (int side = 0; side < 4; ++side) value(side);
    // Tier gem, bottom left.
    const ImVec2 gem(inner.x + 17 * unit, footerTop + footer * .5f + unit);
    const float g = 11 * unit;
    const ImVec2 facets[]{gem + ImVec2(0, -g), gem + ImVec2(g, 0), gem + ImVec2(0, g), gem + ImVec2(-g, 0)};
    d->AddConvexPolyFilled(facets, 4, mix(rgb(rare.gem), rgb(0x000000), .72f));
    d->AddPolyline(facets, 4, rgb(rare.gem), ImDrawFlags_Closed, std::max(1.f, 1.1f * unit));
    auto* tierFont = skinned() ? font() : serif;
    const float tierSize = (c.tier == 8 ? 8.5f : 10.5f) * unit;
    const auto tier = tierFont->CalcTextSizeA(tierSize, 1e5f, 0, romans[c.tier]);
    d->AddText(tierFont, tierSize, gem - tier * .5f, rgb(0xf6f1e2), romans[c.tier]);
    // Affinities, bottom right.
    int offset = 0;
    for (int i = 0; i < 5; ++i)
        if (c.affinityMask & (1u << i)) {
            const ImVec2 p(innerEnd.x - (22 + offset * 17) * unit, footerTop + (footer - 16 * unit) * .5f + unit);
            const auto icon = assets.get("../icons/affinity-" + std::to_string(i) + ".png");
            d->AddImage(icon.id, p, p + ImVec2(16, 16) * unit);
            ++offset;
        }
    if (owner >= 0 && matchAppearance.marker) controlFrame(at, size, owner);
    if (chosen) {
        if (skinned()) {
            d->AddRect(at - ImVec2(6, 6) * scale, end + ImVec2(6, 6) * scale, alpha(palette().selection, .55f), 0, 0,
                       4 * scale);
            d->AddRect(at - ImVec2(3, 3) * scale, end + ImVec2(3, 3) * scale, palette().ink, 0, 0, 1.5f * scale);
        } else
            d->AddRect(at - ImVec2(5, 5) * scale, end + ImVec2(5, 5) * scale, palette().accent, 5, 0, 2 * scale);
    }
}
}
