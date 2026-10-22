"""Shared validator for the current weighted card catalog."""
import json
import base64
import itertools
from pathlib import Path

POLICY = json.loads((Path(__file__).resolve().parent.parent / 'data/rank-policy.json').read_text())
LIMITS = {t['tier']: t for t in POLICY['tiers']}
COSTS = POLICY['rank_costs']

def rank_checks():
    """Compact browser tables; keep both roster editors on this validator."""
    result = {}
    for tier in range(1, 11):
        result[str(tier)] = {}
        for label, rarity in (("Common", "Common"), ("Other", "Rare")):
            bits = bytearray(1250)
            for index, sides in enumerate(itertools.product(range(1, 11), repeat=4)):
                if valid_profile(sides, tier, rarity):
                    bits[index // 8] |= 1 << (index % 8)
            result[str(tier)][label] = base64.b64encode(bits).decode("ascii")
    return result

def weighted_cost(sides):
    return sum(COSTS[n] for n in sides)

def valid_profile(sides, tier, rarity):
    lim = LIMITS[tier]
    if len(sides) != 4 or any(type(n) is not int or not 1 <= n <= lim['side_max'] for n in sides):
        return False
    if not lim['weighted_min'] <= weighted_cost(sides) <= lim['weighted_max']:
        return False
    if tier >= 7:
        if min(sides)>5:return False
        if tier==7:return sides.count(9)<=1 and (9 not in sides or all(n<=6 for n in sides if n!=9))
        if tier==8:
            if sides.count(9)>2:return False
            if sides.count(9)==2:
                pos=[i for i,n in enumerate(sides) if n==9]
                return (pos[0]-pos[1])%2==0 and all(n<=4 for n in sides if n!=9)
            return 9 not in sides or sides.count(8)<=1
        if tier==9:return sides.count(10)<=1 and (10 not in sides or sum(n>6 for n in sides if n!=10)<=1)
        if sides.count(10)>2 or sum(n>=9 for n in sides)>2:return False
        if sides.count(10)==2:
            pos=[i for i,n in enumerate(sides) if n==10]
            return (pos[0]-pos[1])%2==0 and all(n<=4 for n in sides if n!=10)
        return True
    peak = max(sides)
    if tier == 1 and peak == 6:
        return sides.count(6) == 1 and all(n <= 2 for n in sides if n != 6)
    if tier == 2 and peak == 7:
        return sides.count(7) == 1 and all(n <= 3 for n in sides if n != 7)
    if tier == 3 and peak == 7:
        if sides.count(7) != 1:
            return False
        other = sorted(n for n in sides if n != 7)
        return other[-1] <= 4 or (rarity in ('Rare', 'Epic', 'Legendary') and
                                 other[-1] == 5 and other[-2] <= 3)
    if (tier, peak) in ((4, 8), (5, 8), (6, 9)):
        if sides.count(peak) != 1:
            return False
        i = sides.index(peak)
        opposite, adjacent = (4, 3) if tier == 4 else (5, 4)
        return sides[(i + 2) % 4] <= opposite and all(sides[(i + d) % 4] <= adjacent for d in (1, 3))
    return True
