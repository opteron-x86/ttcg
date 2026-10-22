"""Audit evidence: fixed orientation, acquisition semantics, coverage and offline artifacts."""
import importlib.util
import base64
from collections import Counter
import itertools
import json
from pathlib import Path
import re
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location("catalog_audit", ROOT / "tools/build-catalog-audit.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

assert audit.stronger_layout([5, 4, 2, 1], [4, 4, 2, 1])
assert not audit.stronger_layout([4, 4, 2, 1], [4, 4, 2, 1])
assert not audit.stronger_layout([1, 5, 4, 2], [4, 4, 2, 1]), "Never rotate cards for dominance"
facing = audit.edge_comparisons([5, 7, 2, 3], [[2, 3, 5, 7]])
assert facing == {"basic": 0.0, "reverse": 0.0, "equal": 100.0, "comparisons": 4}
assert audit.edge_comparisons([1, 1, 1, 1], [[10, 10, 10, 10]])["reverse"] == 100
assert audit.edge_comparisons([1, 1, 1, 1], [])["basic"] is None

def card(tier=1, rarity="Common", unique=False, available=True):
    return {"tier": tier, "rarity": rarity, "unique": unique,
            "acquisition": {"starter": False, "available": available}}

assert audit.routes_for(card(), []) == ["merchant", "standard"]
assert audit.routes_for(card(7, "Epic"), []) == ["merchant", "tournament"]
assert audit.routes_for(card(7, "Common"), []) == ["merchant", "tournament"]
assert audit.routes_for(card(7, "Legendary", True), []) == []
assert audit.routes_for(card(10, "Legendary", True, False), []) == ["trophy"]
assert audit.routes_for(card(7, "Legendary", True), ["Some NPC"]) == ["opponent"]

data = audit.build_data()
by_name = {c["name"]: c for c in data["cards"]}
assert data["summary"]["cards"] == 703 and data["summary"]["groups"] == 18
assert data["summary"]["pending"] == 29
assert data["summary"]["noRoute"] == 0
assert data["summary"]["rankViolations"] == 0
assert all(not c["routes"] for c in data["cards"] if c["pending"])
assert all("trophy" in c["routes"] for c in data["cards"] if not c["pending"] and c["unique"] and not c["available"])
proposal = data["grouping"]
assignments = {r["id"]: r for r in proposal["rows"]}
assert len(proposal["groups"]) == 18
assert len(assignments) == len(data["cards"]) == len(proposal["rows"])
assert all(assignments[c["id"]]["group"] == c["groups"][0] for c in data["cards"])
assert by_name["Azura"]["groups"] == ["God"] and not by_name["Azura"]["pending"]
assert by_name["Azura"]["tier"] == 10 and by_name["Azura"]["routes"] == ["trophy"]
assert by_name["Skeletal Dragon"]["groups"] == ["Dragon"]
assert assignments[by_name["Fallen Dragon"]["id"]]["requirements"]
assert sum(sum(r[k] for k in audit.RARITIES) for r in proposal["ordinaryCurve"]) == 40
assert sum(sum(r[k] for k in audit.RARITIES) for r in proposal["dragonCurve"]) == 18
for kind, rows in data["buckets"].items():
    assert abs(sum(r["expectedDraws"] for r in rows) - 5) < 0.00001
    for row in rows:
        assert row["count"] == len(row["ids"])
        assert row["risk"] == (row["count"] < row["maxDraws"])
assert not any(r["risk"] for r in data["buckets"]["standard"])
assert not any(r["risk"] for r in data["buckets"]["tournament"])
for c in data["cards"]:
    for key in ("art", "thumbnail"):
        assert (ROOT / "docs" / unquote(c[key])).is_file(), (c["name"], key)
    assert len(c["matches"]) == len(set(c["matches"]))
    assert c["id"] not in c["matches"] + c["stronger"]
    assert abs(sum(c["edges"][k] for k in ("basic", "reverse", "equal")) - 100) <= 0.02
    if "trophy" in c["routes"]:
        assert c["unique"] and not c["available"]
outputs = audit.build_outputs()
templates = audit.build_templates(data)
assert len(templates) == 18
assert {t['group'] for t in templates} == {g['name'] for g in proposal['groups']}
for t in templates:
    assert len({s['id'] for s in t['slots']}) == len(t['slots'])
    assert len({(s['tier'], tuple(s['sides'])) for s in t['slots']}) == len(t['slots'])
    assert t['pool'] == [c['id'] for c in data['cards'] if assignments[c['id']]['group'] == t['group']]
    for s in t['slots'] + t['presets']:
        assert audit.valid_profile(s['sides'], s['tier'], s['rarity']), (t['group'], s)
        assert s['rarity'] in t['allowedRarities'][str(s['tier'])]
        assert s['unique'] == (s['rarity'] == 'Legendary')
    if t['mode'] == 'ordinary':
        assert len(t['slots']) == 40
        assert set(Counter(a for s in t['slots'] for a in s['affinities']).values()) == {8}
        for tier in range(1, 8):
            members = [s for s in t['slots'] if s['tier'] == tier]
            assert Counter(s['rarity'] for s in members) == {'Common': 3, 'Rare': 1, 'Epic': 1}
            assert len({s['affinities'][0] for s in members}) == 5
            commons = [s for s in members if s['rarity'] == 'Common']
            for s in members:
                if s['rarity'] != 'Common':
                    assert audit.weighted_cost(s['sides']) > max(audit.weighted_cost(c['sides']) for c in commons)
            epic = next(s for s in members if s['rarity'] == 'Epic')
            assert max(epic['sides']) > max(max(s['sides']) for s in members if s['rarity'] != 'Epic')
        assert all(s['rarity'] == 'Legendary' for s in t['slots'] if s['tier'] >= 8)
    elif t['mode'] == 'dragon':
        assert len(t['slots']) == 18 and t['tiers'] == [6, 7, 8, 9]
        assert not any(s['rarity'] == 'Common' for s in t['slots'])
    else:
        assert t['tiers'] == [9, 10]
        assert all(s['rarity'] == 'Legendary' and s['unique'] for s in t['slots'])
assert not data['proposals']
# Every bitmap bit must agree with the native-source Python policy, including
# oriented peak/flank restrictions; budget-only validation is insufficient.
for tier in range(1, 11):
    for label, rarity in [('Common', 'Common'), ('Other', 'Rare')]:
        bits = base64.b64decode(data['rankChecks'][str(tier)][label])
        assert len(bits) == 1250
        for index, sides in enumerate(itertools.product(range(1, 11), repeat=4)):
            assert bool(bits[index // 8] & (1 << (index % 8))) == audit.valid_profile(sides, tier, rarity)
builder = outputs[ROOT / 'docs/catalog-template-builder.html']
builder_data = json.loads(re.search(r'<script type="application/json" id="builder-data">(.*?)</script>', builder, re.S)[1])
assert builder_data['templates'] == templates
assert '/* BUILDER_' not in builder
assert not re.search(r'<(?:script|link)[^>]+(?:src|href)="https?://', builder)
html = outputs[ROOT / "docs/catalog-audit.html"]
embedded = json.loads(re.search(r'<script type="application/json" id="audit-data">(.*?)</script>', html, re.S)[1])
assert embedded == json.loads(outputs[ROOT / "docs/catalog-audit-data.json"])
assert "/* AUDIT_" not in html and "<!-- AUDIT_" not in html
assert not re.search(r'<(?:script|link)[^>]+(?:src|href)="https?://', html)
for path, content in outputs.items():
    assert path.read_text() == content, f"Rebuild {path.name}"
print("Catalog audit: acquisition routes, fixed side orientation, pack shortfalls, rank evidence, artwork and generated artifacts passed.")
