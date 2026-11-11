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
// Frame metals: the colour the baked metal is multiplied by, and the gem's pigment.
struct Metal {
    unsigned tint, gem;
};
Metal metal(std::string_view rarity) {
    return rarity == "Rare"        ? Metal{0x6699b3, 0x56dfff}
           : rarity == "Epic"      ? Metal{0x8f70b3, 0xd684fa}
           : rarity == "Legendary" ? Metal{0xd6a857, 0xe6bc65}
                                   : Metal{0x9ea39e, 0xb7b89e};
}
}
// Baked frame atlas, in card units; tools/bake-card-frame.py writes the same layout.
namespace atlas {
constexpr float width = 102, height = 114, margin = 1;
constexpr float coin = 44, patch = 14, edge = 32;
constexpr ImVec2 coinBase(0, 0), coinLight(46, 0);
}
ImVec2 App::valueAnchor(int side, ImVec2 size) const {
    const float unit = size.x / 180.f;
    if (face.classic) {
        constexpr ImVec2 cluster[]{ImVec2(33, 20), ImVec2(49, 39), ImVec2(33, 58), ImVec2(17, 39)};
        return (cluster[side] + (face.legacy() ? ImVec2(0, 0) : ImVec2(4, 5))) * unit;
    }
    if (face.legacy()) {
        const float middle = size.y * (face.names ? .42f : .5f);
        const ImVec2 legacy[]{ImVec2(size.x * .5f, 20 * unit), ImVec2(size.x - 15 * unit, middle),
                              ImVec2(size.x * .5f, size.y * (face.names    ? .65f
                                                             : face.groups ? .82f
                                                                           : .92f)),
                              ImVec2(15 * unit, middle)};
        return legacy[side];
    }
    // Medallions sit just inside the edge, so touching values face each other across the gap.
    const float inset = (face.printed() ? 17.5f : 16.4f) * unit, foot = (face.printed() ? 18.f : 19.6f) * unit;
    const ImVec2 edge[]{ImVec2(size.x * .5f, inset), ImVec2(size.x - inset, size.y * .46f),
                        ImVec2(size.x * .5f, size.y - foot), ImVec2(inset, size.y * .46f)};
    return edge[side];
}
void App::framedCard(const CardView& c, Texture texture, ImVec2 at, ImVec2 size, bool chosen, int owner) {
    auto* d = ImGui::GetWindowDrawList();
    const bool printed = face.printed();
    const float unit = size.x / 180.f, bezel = (printed ? 3.4f : 4.2f) * unit;
    const auto end = at + size;
    const auto rare = metal(c.rarity);
    // In a match the frame belongs to whoever controls the card; rarity stays on the gem.
    const ImU32 tint = owner >= 0 ? mix(controlColour(owner), rgb(0x1c1c1c), .2f) : rgb(rare.tint);
    const ImU32 light = mix(tint, rgb(0xffffff), .3f);
    const ImU32 base = owner >= 0 ? mix(controlColour(owner), rgb(0x0a0a0a), printed ? .82f : .87f) : rgb(0x0c0c0d);
    const float corner = 3 * unit;
    for (int i = 3; i > 0; --i)
        d->AddRectFilled(at + ImVec2(2 - i, 3 - i) * unit, end + ImVec2(2 + i, 3 + i) * unit, rgb(0x000000, 26),
                         corner + i * unit);
    if (printed) d->AddRectFilled(at, end, rgb(0x050505), corner);
    const ImVec2 inner = at + ImVec2(bezel, bezel), innerEnd = end - ImVec2(bezel, bezel);
    if (!printed) d->AddRectFilled(inner, innerEnd, rgb(0x121212));
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
        artImage(texture, inner, innerEnd, uv0, uv1);
    }
    // Footer row: tier gem, bottom value and affinity.
    const float footer = 27 * unit, footerTop = innerEnd.y - footer;
    auto* nameFont = cardNameFont ? cardNameFont : serif;
    std::vector<NameLine> lines, groupLines;
    float pixels = 0, groupPixels = 0, lineHeight = 0;
    // Below nine real pixels a name is noise; very small cards carry values only.
    if (size.x >= 84 && (face.names || face.groups)) {
        pixels = std::clamp(size.x * .069f, std::max(9.f, 9.f * scale), std::max(9.f, 28.f * scale));
        if (face.names) lines = nameLines(nameFont, c.name, pixels, size.x * .84f, face.lettering);
        if (lines.size() > 2) {
            pixels *= .8f;
            lines = nameLines(nameFont, c.name, pixels, size.x * .84f, face.lettering);
        }
        lineHeight = pixels * 1.16f;
        groupPixels = std::clamp(size.x * .052f, 8.f * scale, 18.f * scale);
        if (face.groups) {
            std::string list;
            for (unsigned i = 0; i < ttcg::creatureGroups.size(); ++i)
                if (c.groups & (1ull << i)) list += (list.empty() ? "" : " / ") + std::string(ttcg::creatureGroups[i]);
            groupLines = nameLines(nameFont, list, groupPixels, size.x * .84f, 1);
            if (groupLines.size() > 2) groupLines.resize(2);
        }
    }
    const bool lettered = !lines.empty() || !groupLines.empty();
    const float text = lines.size() * lineHeight + groupLines.size() * groupPixels * 1.16f;
    const float top = footerTop - (lettered ? text + (printed ? 11 : 14) * unit : 0);
    if (printed) {
        // No strip: one gradient from clear to dark carries the name and the footer row.
        const float from = top - 24 * unit, to = footerTop + 14 * unit;
        constexpr int bands = 6;
        const auto shade = [&](float t) { return alpha(base, std::pow(std::clamp(t, 0.f, 1.f), 1.25f) * .93f); };
        for (int i = 0; i < bands; ++i) {
            const float a = float(i) / bands, b = float(i + 1) / bands;
            d->AddRectFilledMultiColor(ImVec2(inner.x, from + (to - from) * a),
                                       ImVec2(innerEnd.x, from + (to - from) * b), shade(a), shade(a), shade(b),
                                       shade(b));
        }
        d->AddRectFilled(ImVec2(inner.x, to), innerEnd, shade(1));
    } else {
        if (lettered) {
            const auto clear = alpha(base, 0), solid = alpha(base, .88f);
            d->AddRectFilledMultiColor(ImVec2(inner.x, top - 18 * unit), ImVec2(innerEnd.x, top), clear, clear, solid,
                                       solid);
            d->AddRectFilled(ImVec2(inner.x, top), ImVec2(innerEnd.x, footerTop), solid);
        }
        d->AddRectFilled(ImVec2(inner.x, footerTop), innerEnd, base);
        const float rule = std::max(1.f, unit);
        d->AddRectFilled(ImVec2(inner.x, footerTop), ImVec2(innerEnd.x, footerTop + rule), alpha(light, .7f));
        d->AddRectFilled(ImVec2(inner.x, footerTop + rule), ImVec2(innerEnd.x, footerTop + 2 * rule),
                         rgb(0x000000, 130));
    }
    if (lettered) {
        float y = top + (printed ? 2 : 4) * unit;
        if (!lines.empty()) {
            drawNameLines(d, nameFont, inner, innerEnd - inner, lines, y, pixels, scale, rgb(0xf5eedd));
            y += lines.size() * lineHeight;
        }
        if (!groupLines.empty())
            drawNameLines(d, nameFont, inner, innerEnd - inner, groupLines, y, groupPixels, scale, rgb(0xd9d2bf, 210));
    }
    // The baked metal: a grey base multiplied by the frame's colour, then its highlights and shadows.
    const int level = unit * 1.3f <= 1 ? 1 : unit * 1.3f <= 2 ? 2 : unit * 1.3f <= 4 ? 4 : 8;
    const auto sheet = printed ? Texture{} : assets.optional("../assets/card/frame-" + std::to_string(level) + ".png");
    const auto piece = [&](ImVec2 source, ImVec2 extent, ImVec2 a, ImVec2 b, ImU32 colour) {
        // Clipped to the card by hand: a clip rectangle would not follow the inspector's tilt.
        const ImVec2 from(std::max(a.x, at.x), std::max(a.y, at.y)), to(std::min(b.x, end.x), std::min(b.y, end.y));
        if (to.x <= from.x || to.y <= from.y) return;
        const auto u = [&](float x) {
            return (source.x + atlas::margin + extent.x * (x - a.x) / (b.x - a.x)) / atlas::width;
        };
        const auto v = [&](float y) {
            return (source.y + atlas::margin + extent.y * (y - a.y) / (b.y - a.y)) / atlas::height;
        };
        d->AddImage(sheet.id, from, to, ImVec2(u(from.x), v(from.y)), ImVec2(u(to.x), v(to.y)), colour);
    };
    if (printed) {
        // Two fine lines, as on a printed card: one at the edge, one around the picture.
        const float line = std::max(1.f, 1.5f * unit);
        d->AddRect(at + ImVec2(line, line) * .5f, end - ImVec2(line, line) * .5f, mix(tint, rgb(0x000000), .25f),
                   corner - line * .5f, 0, line);
        d->AddRect(inner, innerEnd, rgb(0x000000, 150), 1.4f * unit, 0, std::max(1.f, 2.6f * unit));
        d->AddRect(inner, innerEnd, light, 1.4f * unit, 0, std::max(1.f, 1.3f * unit));
    } else if (sheet.id) {
        const float p = atlas::patch * unit;
        const ImVec2 patch(atlas::patch, atlas::patch), flat(atlas::edge, atlas::patch),
            tall(atlas::patch, atlas::edge);
        for (int pass = 0; pass < 2; ++pass) {
            const ImU32 colour = pass ? IM_COL32_WHITE : tint;
            const float cornerRow = pass ? 62.f : 46.f, edgeRow = pass ? 94.f : 78.f, sideRow = pass ? 80.f : 46.f;
            piece(ImVec2(0, cornerRow), patch, at, at + ImVec2(p, p), colour);
            piece(ImVec2(16, cornerRow), patch, ImVec2(end.x - p, at.y), ImVec2(end.x, at.y + p), colour);
            piece(ImVec2(32, cornerRow), patch, ImVec2(at.x, end.y - p), ImVec2(at.x + p, end.y), colour);
            piece(ImVec2(48, cornerRow), patch, end - ImVec2(p, p), end, colour);
            piece(ImVec2(0, edgeRow), flat, ImVec2(at.x + p, at.y), ImVec2(end.x - p, at.y + p), colour);
            piece(ImVec2(34, edgeRow), flat, ImVec2(at.x + p, end.y - p), ImVec2(end.x - p, end.y), colour);
            piece(ImVec2(68, sideRow), tall, ImVec2(at.x, at.y + p), ImVec2(at.x + p, end.y - p), colour);
            piece(ImVec2(84, sideRow), tall, ImVec2(end.x - p, at.y + p), ImVec2(end.x, end.y - p), colour);
        }
    } else
        d->AddRect(at, end, tint, corner, 0, bezel);
    // Values.
    auto* valueFont = cardValueFont ? cardValueFont : bold;
    const bool geometric = valueFont == jost;
    if (geometric) valueFont = jostBold;
    const float digit = (printed ? (geometric ? 34.f : 30.f) : (geometric ? 31.f : 26.f)) * unit;
    const ImU32 ink = rgb(0xf8f3e6);
    const auto value = [&](int side) {
        const std::string text = c.sides[side] == 10 ? "A" : std::to_string(c.sides[side]);
        const auto extent = valueFont->CalcTextSizeA(digit, 1e5f, 0, text.c_str());
        // Jost's tall line box leaves its numerals above the middle of it.
        const ImVec2 centre = at + valueAnchor(side, size),
                     pos = centre - extent * .5f + ImVec2(0, geometric ? digit * .06f : 0);
        if (printed) {
            // A soft dark ground, then a close halo that follows the numeral.
            for (int i = 6; i > 0; --i) d->AddCircleFilled(centre, 17 * unit * i / 6, rgb(0x000000, 16), 28);
            for (int i = 0; i < 12; ++i) {
                const float angle = i * 3.14159265f / 6;
                d->AddText(valueFont, digit, pos + ImVec2(std::cos(angle), std::sin(angle)) * 2.2f * unit,
                           rgb(0x000000, 34), text.c_str());
            }
            for (int i = 0; i < 8; ++i) {
                const float angle = i * 3.14159265f / 4;
                d->AddText(valueFont, digit, pos + ImVec2(std::cos(angle), std::sin(angle)) * unit, rgb(0x000000, 96),
                           text.c_str());
            }
        }
        d->AddText(valueFont, digit, pos + ImVec2(.5f, 1.1f) * unit, rgb(0x000000, 215), text.c_str());
        if (valueFont == lato)
            for (float shift : {-.5f, .5f})
                d->AddText(valueFont, digit, pos + ImVec2(shift * unit, 0), ink, text.c_str());
        d->AddText(valueFont, digit, pos, ink, text.c_str());
    };
    if (face.classic) {
        // The classic cluster keeps its corner, on one diamond plaque.
        const ImVec2 centre = at + ImVec2(37, 44) * unit;
        const float r = 37 * unit;
        const ImVec2 diamond[]{centre + ImVec2(0, -r), centre + ImVec2(r, 0), centre + ImVec2(0, r),
                               centre + ImVec2(-r, 0)};
        d->AddConvexPolyFilled(diamond, 4, alpha(base, printed ? .78f : .92f));
        d->AddPolyline(diamond, 4, light, ImDrawFlags_Closed, std::max(1.f, 1.3f * unit));
    } else if (sheet.id) {
        const ImVec2 half = ImVec2(atlas::coin, atlas::coin) * unit * .5f, sprite(atlas::coin, atlas::coin);
        for (int pass = 0; pass < 2; ++pass)
            for (int side = 0; side < 4; ++side) {
                const ImVec2 centre = at + valueAnchor(side, size);
                piece(pass ? atlas::coinLight : atlas::coinBase, sprite, centre - half, centre + half,
                      pass ? IM_COL32_WHITE : tint);
            }
    }
    for (int side = 0; side < 4; ++side) value(side);
    // Tier gem, bottom left: four facets under the frame's light.
    const ImVec2 gem(inner.x + 17 * unit, footerTop + footer * .5f + unit);
    const float g = 10.5f * unit;
    const ImU32 pigment = rgb(rare.gem);
    const auto diamond = [&](float r, ImU32 colour, float line = 0) {
        const ImVec2 points[]{gem + ImVec2(0, -r), gem + ImVec2(r, 0), gem + ImVec2(0, r), gem + ImVec2(-r, 0)};
        if (line > 0)
            d->AddPolyline(points, 4, colour, ImDrawFlags_Closed, line);
        else
            d->AddConvexPolyFilled(points, 4, colour);
    };
    diamond(g + 1.4f * unit, rgb(0x000000, 190));
    diamond(g, mix(pigment, rgb(0x000000), .7f));
    const ImVec2 north = gem + ImVec2(0, -g), east = gem + ImVec2(g, 0), south = gem + ImVec2(0, g),
                 west = gem + ImVec2(-g, 0);
    d->AddTriangleFilled(gem, west, north, mix(pigment, rgb(0x000000), .36f));
    d->AddTriangleFilled(gem, north, east, mix(pigment, rgb(0x000000), .58f));
    d->AddTriangleFilled(gem, south, west, mix(pigment, rgb(0x000000), .74f));
    d->AddTriangleFilled(gem, east, south, mix(pigment, rgb(0x000000), .84f));
    diamond(g * .62f, mix(pigment, rgb(0x000000), .84f));
    diamond(g * .62f, alpha(mix(pigment, rgb(0xffffff), .1f), .8f), std::max(1.f, .8f * unit));
    auto* tierFont = font();
    const float tierSize = (c.tier == 8 ? 8.f : 10.f) * unit;
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
    if (chosen) chosenOutline(at, size);
}
}
