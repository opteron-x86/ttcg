#!/usr/bin/env python3
"""Build the offline catalog audit. Never changes gameplay data or runtime files."""
import argparse
from collections import Counter, defaultdict
import hashlib
import importlib.util
import json
from pathlib import Path

from card_rank_policy import POLICY, valid_profile, weighted_cost, rank_checks

ROOT = Path(__file__).resolve().parents[1]
RARITIES = ("Common", "Rare", "Epic", "Legendary")
SOURCE_FILES = (
    "data/cards.json", "data/opponents.json", "data/opponent-collections.json",
    "data/rank-policy.json", "data/workshop-roster.json", "include/Album.h", "include/Tournaments.h",
    "include/Match.h", "include/Campaign.h", "include/Shop.h",
)
# This offline report audits the official roster only. Runtime expansions are
# opt-in additions to the same tier/rarity buckets; they are not authoring data.
# These two recipes are described by this report. Fail visibly when their
# implementation changes so rebuilding cannot silently bless outdated formulas.
REVIEWED_PACKS = (
    ("include/Album.h", "inline std::vector<CardID> packCards(",
     "9ec31744012ed3935b4cf07967516722b997d5ab9f0eaad82bc456b0bc56974f"),
    ("include/Tournaments.h", "inline std::vector<CardID> tournamentPack(",
     "66c640f0b1b51effed8fd55c03cbf7e74c68dc2e1d44261038e7b7948ae3eeb1"),
)


def verify_pack_recipes():
    trophy_source = (ROOT / "include/Tournaments.h").read_text()
    trophy_guard = 'if((circuit!=1&&circuit!=2)||c.pending||!c.unique||std::string_view(c.rarity)!="Legendary")return false;'
    trophy_condition = 'return officialCard(c)&&!c.available&&(circuit==1?c.tier==8:c.tier==9||c.tier==10);'
    if trophy_guard not in trophy_source or trophy_condition not in trophy_source:
        raise ValueError("Tournament trophy eligibility changed. Review routes_for before rebuilding the audit.")
    for path, signature, expected in REVIEWED_PACKS:
        source = (ROOT / path).read_text()
        start = source.index(signature)
        body = source.index("{", start)
        depth = 0
        for end in range(body, len(source)):
            depth += (source[end] == "{") - (source[end] == "}")
            if not depth:
                break
        actual = hashlib.sha256(source[start:end + 1].encode()).hexdigest()
        if actual != expected:
            raise ValueError(f"{path}: pack implementation changed. Review routes_for, "
                             "bucket_report and the report explanation before updating "
                             "the reviewed recipe fingerprint.")


def stronger_layout(candidate, reference):
    """Fixed orientation, printed sides only. This is not overall card dominance."""
    return all(a >= b for a, b in zip(candidate, reference)) and any(
        a > b for a, b in zip(candidate, reference))


def routes_for(card, holders):
    a, tier = card["acquisition"], card["tier"]
    if card.get("roster_status") == "pending":
        return []
    routes = []
    if a.get("starter"):
        routes.append("starter")
    if holders:
        routes.append("opponent")
    if a["available"] and not card["unique"]:
        if 1 <= tier <= 7 and card["rarity"] in RARITIES[:3]:
            routes.append("merchant")
        if 1 <= tier <= 6 and card["rarity"] in RARITIES[:3]:
            routes.append("standard")
        if 3 <= tier <= 7 and card["rarity"] in RARITIES[:3]:
            routes.append("tournament")
    if not a["available"] and card["unique"] and card["rarity"] == "Legendary" and 8 <= tier <= 10:
        routes.append("trophy")
    return routes


def edge_comparisons(sides, peers):
    """Every other identity in the tier, each of four actual opposing directions."""
    counts = Counter()
    for peer in peers:
        for side, rank in enumerate(sides):
            facing = peer[(side + 2) % 4]
            counts["basic" if rank > facing else "reverse" if rank < facing else "equal"] += 1
    total = sum(counts.values())
    return {k: round(counts[k] * 100 / total, 2) if total else None
            for k in ("basic", "reverse", "equal")} | {"comparisons": total}


def bucket_report(cards, kind):
    report = []
    for tier in range(1, 11):
        for rarity in RARITIES:
            members = [c["id"] for c in cards if kind in c["routes"] and
                       c["tier"] == tier and c["rarity"] == rarity]
            if kind == "standard":
                used = tier <= 6 and rarity != "Legendary"
                demand = (4 if tier <= 3 else 1) if rarity == "Common" else 1
                # One randomly chosen slot is upgraded in 2% of packs.
                tier_weight = ({1: .45, 2: .35, 3: .20}[tier] * .996 if tier <= 3
                               else {4: .75, 5: .20, 6: .05}.get(tier, 0) * .004)
                expected = tier_weight * (4 if rarity == "Common" else
                                          {"Rare": .965, "Epic": .035, "Legendary": 0}[rarity])
            else:
                used = 3 <= tier <= 7 and rarity in ("Common", "Rare", "Epic")
                demand = 5
                expected = 5 * {3: .15, 4: .25, 5: .30, 6: .20, 7: .10}.get(tier, 0) * {
                    "Common": .30, "Rare": .45, "Epic": .25, "Legendary": 0}[rarity]
            if used:
                report.append({"tier": tier, "rarity": rarity, "count": len(members),
                               "maxDraws": demand, "risk": len(members) < demand,
                               "expectedDraws": round(expected, 6), "ids": members})
    return report


def grouping_proposal(cards):
    """Editorial overlay only. Preserve current tiers, ranks and acquisition data."""
    path = ROOT / "tools/catalog-audit/grouping-proposal.json"
    proposal = json.loads(path.read_text())
    roster_path = ROOT / "tools/catalog-audit/roster-profiles.json"
    roster = json.loads(roster_path.read_text())
    groups = {g["name"] for g in proposal["groups"]}
    if len(groups) != 18 or len(proposal["groups"]) != 18:
        raise ValueError("The grouping proposal must define eighteen distinct groups.")
    ids = {c["id"] for c in cards}
    rows = []
    for c in cards:
        notes = [proposal["group_notes"][g] for g in c["groups"] if g in proposal["group_notes"]]
        if len(c["groups"]) == 1 and c["groups"][0] in groups:
            group, action = c["groups"][0], "retain"
            notes = ["Superior roster is unfinished."] if c.get("pending") else []
        elif c["id"] in proposal["overrides"]:
            override = proposal["overrides"][c["id"]]
            group, action = override["group"], override["action"]
            notes.append(override["note"])
        else:
            special = {proposal["subtype_map"][s] for s in c["subtypes"] if s in proposal["subtype_map"]}
            if special:
                candidates = special
            elif "Undead" in c["groups"]:
                candidates = {"Undead"}
                if len(c["groups"]) > 1:
                    notes.append("Undead is the sole gameplay group; retain ancestry and species as descriptive tags.")
            else:
                # Direct indexing intentionally fails on unreviewed new source groups.
                candidates = {proposal["group_map"][g] for g in c["groups"]}
            group = next(iter(candidates)) if len(candidates) == 1 else None
            action = "review" if group is None else "retain" if c["groups"] == [group] else "regroup"
        if group is not None and group not in groups:
            raise ValueError(f"Unknown proposed group for {c['name']}: {group}")
        requirements = []
        if group == "God":
            if c["tier"] not in (9, 10) or c["rarity"] != "Legendary" or not c["unique"]:
                requirements.append("God requires tier IX–X, Legendary and unique.")
        elif group == "Dragon":
            if not 6 <= c["tier"] <= 9:
                requirements.append("Review for tier VI–IX, or replace the subject; rebuild ranks when retiering.")
            if c["rarity"] == "Common":
                requirements.append("Dragon uses Rare / Epic, plus Legendary uniques.")
        elif group is not None and c["tier"] == 10:
            requirements.append("Reserve tier X for God; review this card within tier I–IX.")
        if group != "God" and c["rarity"] == "Legendary" and (
                not c["unique"] or not 8 <= c["tier"] <= 9):
            requirements.append("Propose Rare / Epic for generic subjects, or a specific Legendary unique at tier VIII–IX.")
        if group not in (None, "God", "Dragon") and c["tier"] in (8, 9) and c["rarity"] != "Legendary":
            requirements.append("Ordinary groups reserve tiers VIII–IX for Legendary uniques; review this subject in I–VII.")
        if requirements and action in ("retain", "regroup"):
            action = "rework"
        rows.append({"id": c["id"], "group": group, "action": action,
                     "note": " ".join(notes), "requirements": requirements})
    return {"version": proposal["version"], "status": proposal["status"],
            "fingerprint": hashlib.sha256(path.read_bytes() + roster_path.read_bytes()).hexdigest(),
            "groups": proposal["groups"], "rows": rows,
            "ordinaryCurve": roster["ordinaryCurve"], "dragonCurve": proposal["dragon_curve"]}


def card_proposals(cards, fingerprint):
    by_id = {c["id"]: c for c in cards}
    result = []
    for path in sorted((ROOT / "docs/catalog-reviews").glob("*-draft.json")):
        draft = json.loads(path.read_text())
        if draft["kind"] != "tessera-catalog-review" or draft["schema"] != 1 or draft["fingerprint"] != fingerprint:
            raise ValueError(f"Rebase {path.name} on the current catalog before rebuilding.")
        source_path = path.parent / draft["sourceReview"]
        if source_path.parent != path.parent or hashlib.sha256(source_path.read_bytes()).hexdigest() != draft["sourceSHA256"]:
            raise ValueError(f"The original review for {path.name} changed.")
        source = json.loads(source_path.read_text())
        seen = set()
        for entry in draft["entries"] + source["entries"]:
            c = by_id[entry["id"]]
            original = {k: c[k] for k in ("tier", "rarity", "groups", "sides")}
            if entry["name"] != c["name"] or entry["before"] != original:
                raise ValueError(f"Original values changed for {entry['name']}")
        for entry in draft["entries"]:
            if entry["id"] in seen or entry["decision"] not in ("keep", "adjust", "retire", "review"):
                raise ValueError(f"Invalid proposal entry: {entry['name']}")
            seen.add(entry["id"])
            after = entry["after"]
            if entry["decision"] != "retire" and not valid_profile(after["sides"], after["tier"], after["rarity"]):
                raise ValueError(f"Proposed ranks need review: {entry['name']}")
        result.append({"id": draft["proposalId"], "title": draft["title"], "group": draft["group"],
                       "summary": draft["summary"], "revision": hashlib.sha256(path.read_bytes()).hexdigest(),
                       "file": "catalog-reviews/" + path.name,
                       "sourceFile": "catalog-reviews/" + source_path.name,
                       "sourceEntries": source["entries"], "entries": draft["entries"]})
    if len({p["id"] for p in result}) != len(result):
        raise ValueError("Proposal IDs must be unique.")
    return result


def build_templates(data):
    """Equal ordinary-group budgets; editable workshop slots, never runtime edits."""
    spec = json.loads((ROOT / "tools/catalog-audit/roster-profiles.json").read_text())
    if spec["sideOrder"] != POLICY["side_order"]:
        raise ValueError("Workshop side order differs from the catalog.")
    for tier, rarities in spec["profiles"].items():
        for rarity, profiles in rarities.items():
            for sides in profiles:
                if not valid_profile(sides, int(tier), rarity):
                    raise ValueError(f"Invalid workshop profile: {tier} {rarity} {sides}")
    group_rows = {r["id"]: r for r in data["grouping"]["rows"]}
    result = []
    for group_index, group in enumerate(data["grouping"]["groups"]):
        name = group["name"]
        slug = name.lower().replace(" ", "-")
        mode = "god" if name == "God" else "dragon" if name == "Dragon" else "ordinary"
        curve = (data["grouping"]["dragonCurve"] if mode == "dragon" else
                 [{"tier": 9, "Legendary": 3}, {"tier": 10, "Legendary": 2}] if mode == "god" else
                 spec["ordinaryCurve"])
        presets, slots, allowed = [], [], {}
        for row in curve:
            tier = row["tier"]
            allowed[str(tier)] = (["Legendary"] if mode == "god" or (mode == "ordinary" and tier >= 8) else
                                 ["Rare", "Epic", "Legendary"] if mode == "dragon" and tier >= 8 else
                                 ["Rare", "Epic"] if mode == "dragon" else ["Common", "Rare", "Epic"])
            for rarity in allowed[str(tier)]:
                for variant, sides in enumerate(spec["profiles"][str(tier)][rarity]):
                    for rotation in range(4):
                        presets.append({"id": f"{tier}-{rarity.lower()}-{variant}-{rotation}",
                                        "tier": tier, "rarity": rarity,
                                        "sides": sides[rotation:] + sides[:rotation],
                                        "affinities": [spec["affinities"][(group_index + tier + variant + rotation) % 5]],
                                        "unique": rarity == "Legendary"})
            ordinal = 0
            for rarity in RARITIES:
                count = row.get(rarity, 0)
                variants = spec["profiles"][str(tier)].get(rarity, [])
                for index in range(count):
                    ordinal += 1
                    variant = index % len(variants)
                    rotation = (group_index + index // len(variants)) % 4
                    preset = next(p for p in presets if p["id"] == f"{tier}-{rarity.lower()}-{variant}-{rotation}")
                    used = {tuple(s["sides"]) for s in slots if s["tier"] == tier}
                    if tuple(preset["sides"]) in used:
                        preset = next(p for p in presets if p["tier"] == tier and p["rarity"] == rarity and tuple(p["sides"]) not in used)
                    slots.append({"id": f"{slug}-{tier}-{ordinal}", "preset": preset["id"],
                                  **{k: preset[k] for k in ("tier", "rarity", "sides", "unique")},
                                  "affinities": [spec["affinities"][(group_index + (sum(s["unique"] for s in slots) if rarity == "Legendary" else tier + ordinal - 1)) % 5]]})
        template = {"id": slug + "-v2", "slug": slug, "label": name, "group": name, "mode": mode,
                    "tiers": [r["tier"] for r in curve], "allowedRarities": allowed,
                    "pool": [c["id"] for c in data["cards"] if group_rows[c["id"]]["group"] == name],
                    "slots": slots, "presets": presets}
        template["revision"] = hashlib.sha256(json.dumps(template, sort_keys=True).encode()).hexdigest()
        result.append(template)
    return result


def build_data():
    verify_pack_recipes()
    catalog = json.loads((ROOT / "data/cards.json").read_text())
    assert catalog["side_order"] == ["top", "right", "bottom", "left"]
    profiles = {p["name"]: p for p in json.loads((ROOT / "data/opponents.json").read_text())}
    holders = defaultdict(list)
    for row in json.loads((ROOT / "data/opponent-collections.json").read_text())["opponents"]:
        p = profiles[row["name"]]
        for form in set(row["cards"]):
            holders[form].append({"name": p["name"], "hold": p["region"], "skill": p["skillName"]})
    # Reuse the existing offline library's format preference and URL escaping.
    spec = importlib.util.spec_from_file_location("card_library", ROOT / "tools/build-card-library.py")
    library = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(library)
    faces = library.images("cards", library.ART_FORMATS)
    thumbs = library.images("thumbs", library.ART_FORMATS)
    cards = []
    for c in sorted(catalog["cards"], key=lambda c: (c["tier"], c["name"])):
        key = c["name"].lower()
        if key not in faces:
            raise ValueError("Missing artwork: " + c["name"])
        owners = sorted(holders[c["form_id"]], key=lambda p: p["name"])
        routes = routes_for(c, owners)
        cards.append({k: c[k] for k in ("id", "name", "tier", "rarity", "sides", "groups",
                                      "subtypes", "affinities", "unique")} | {
            "form": c["form_id"], "available": c["acquisition"]["available"],
            "pending": c.get("roster_status") == "pending",
            "routes": routes, "holders": owners, "art": faces[key],
            "thumbnail": thumbs.get(key, faces[key]), "focus": c.get("focus", "50% 40%"),
            "role": c.get("profile_role", "Unassigned").lower(),
            "sourceID": c.get("source_id", ""), "sourceSet": c.get("set", ""),
            "acquisitionNote": c["acquisition"].get("route", c["acquisition"].get("distribution", "")),
            "total": sum(c["sides"]), "cost": weighted_cost(c["sides"]),
            "valid": valid_profile(c["sides"], c["tier"], c["rarity"]),
            "matches": [], "stronger": [], "flags": [],
        })
    groups = sorted({g for c in cards for g in c["groups"]})
    group_sizes = Counter(g for c in cards for g in c["groups"])
    for c in cards:
        peers = [p for p in cards if p["id"] != c["id"] and p["tier"] == c["tier"]]
        c["edges"] = edge_comparisons(c["sides"], [p["sides"] for p in peers])
        c["matches"] = [p["id"] for p in peers if p["sides"] == c["sides"]]
        c["stronger"] = [p["id"] for p in peers if stronger_layout(p["sides"], c["sides"])]
        if not c["routes"] and not c["pending"]:
            c["flags"].append("no-route")
        if any(group_sizes[g] <= 5 for g in c["groups"]):
            c["flags"].append("small-group")
        if c["matches"]:
            c["flags"].append("shared-layout")
        if c["stronger"]:
            c["flags"].append("stronger-layout")
        if any(p["sides"] == c["sides"] and p["rarity"] != c["rarity"] for p in peers):
            c["flags"].append("rarity-overlap")
        if not c["valid"]:
            c["flags"].append("rank-policy")
    buckets = {kind: bucket_report(cards, kind) for kind in ("standard", "tournament")}
    risky = {id for rows in buckets.values() for row in rows if row["risk"] for id in row["ids"]}
    for c in cards:
        if c["id"] in risky:
            c["flags"].append("pack-shortfall")
    sources = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in SOURCE_FILES}
    fingerprint = hashlib.sha256(json.dumps(sources, sort_keys=True).encode()).hexdigest()
    return {"schema": 1, "fingerprint": fingerprint, "sources": sources, "cards": cards,
            "grouping": grouping_proposal(cards),
            "proposals": card_proposals(cards, fingerprint), "rankChecks": rank_checks(),
            "groups": groups, "rarities": RARITIES, "policy": POLICY, "buckets": buckets,
            "summary": {"cards": len(cards), "groups": len(groups),
                        "released": sum(c["available"] for c in cards),
                        "trophies": sum("trophy" in c["routes"] for c in cards),
                        "noRoute": sum(not c["routes"] and not c["pending"] for c in cards),
                        "pending": sum(c["pending"] for c in cards),
                        "smallGroups": sum(n <= 5 for n in group_sizes.values()),
                        "unique": sum(c["unique"] for c in cards),
                        "rankViolations": sum(not c["valid"] for c in cards)},
            "briefs": [
                {"group": "Dragon", "tiers": [6, 7, 8, 9], "title": "Finish the Dragon roster",
                 "role": "Rare and Epic cards, with Legendary uniques in VIII–IX.",
                 "route": "Candidates are preserved for authoring, outside normal acquisition.",
                 "decision": "Choose subjects and rebuild their side values in the workshop."},
                {"group": "God", "tiers": [9, 10], "title": "God trophies",
                 "role": "The Nine Divines and 17 Daedric Princes are Tier X Legendary uniques.",
                 "route": "Completed Gods are Masters tournament trophies; nine still use placeholder art.",
                 "decision": "The Tribunal, Dagoth Ur and lesser gods remain unfinished Tier IX candidates."},
            ]}


def build_outputs():
    data = build_data()
    data["templates"] = build_templates(data)
    data["workshopPolicy"] = json.loads((ROOT / "tools/catalog-audit/roster-profiles.json").read_text())
    roster = json.loads((ROOT / "data/workshop-roster.json").read_text())
    templates = {t["id"]: t for t in data["templates"]}
    data["initialWorkshop"] = {"kind": "tessera-roster-workshop", "schema": 2,
        "catalogFingerprint": data["fingerprint"], "sideOrder": POLICY["side_order"],
        "currentGroup": roster["currentGroup"], "groups": [
            {"kind": "tessera-catalog-template", "schema": 2,
             "catalogFingerprint": data["fingerprint"], "sideOrder": POLICY["side_order"],
             "template": {k: templates[g["id"]][k] for k in ("id", "group", "label", "revision")},
             "slots": g["slots"]} for g in roster["groups"]]}
    data["cardBack"] = "../TesseraUI/art/backs/mosaic.webp"
    if not (ROOT / "docs" / data["cardBack"]).is_file():
        raise ValueError("The Tessera card back is missing.")
    payload = json.dumps(data, ensure_ascii=False, separators=(",", ":")).replace("<", "\\u003c").replace("&", "\\u0026")
    folder = ROOT / "tools/catalog-audit"
    page = (folder / "index.html").read_text()
    page = page.replace("/* AUDIT_CSS */", (folder / "audit.css").read_text())
    page = page.replace("/* AUDIT_JS */", (folder / "audit.js").read_text())
    page = page.replace("<!-- AUDIT_DATA -->", payload)
    builder = (folder / "template-builder.html").read_text()
    builder = builder.replace("/* AUDIT_CSS */", (folder / "audit.css").read_text())
    builder = builder.replace("/* BUILDER_CSS */", (folder / "template-builder.css").read_text())
    builder = builder.replace("/* WORKSHOP_SYNC_JS */", (folder / "workshop-sync.js").read_text())
    builder = builder.replace("/* BUILDER_JS */", (folder / "rank-tools.js").read_text() + "\n" + (folder / "template-builder.js").read_text())
    builder = builder.replace("<!-- AUDIT_DATA -->", payload)
    return {ROOT / "docs/catalog-audit.html": page,
            ROOT / "docs/catalog-template-builder.html": builder,
            ROOT / "docs/catalog-audit-data.json": json.dumps(data, ensure_ascii=False, indent=2) + "\n"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for path, content in build_outputs().items():
        if args.check:
            if not path.exists() or path.read_text() != content:
                raise SystemExit("Catalog audit is out of date. Run python3 tools/build-catalog-audit.py")
        else:
            path.write_text(content)
            print("Built", path.relative_to(ROOT))
    if args.check:
        print("Catalog audit matches current data, acquisition sources, rank policy and artwork.")


if __name__ == "__main__":
    main()
