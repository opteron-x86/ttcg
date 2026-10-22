#!/usr/bin/env python3
"""Create a local, readable report from the rule table's simulation results."""
import argparse
import csv
import html
import json
from pathlib import Path
import statistics

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
ARCHETYPES = ['affinity', 'group', 'construct', 'mixed']
LABELS = ['One affinity', 'One group', 'Neutral Constructs', 'Mixed']
CORE = [('off', False), ('off', True), ('ascension', False), ('ascension', True), ('descension', False), ('descension', True)]
CORE_LABELS = ['Standard', 'Dominion', 'Ascension', 'Ascension\n+ Dominion', 'Descension', 'Descension\n+ Dominion']


def find_rule(data, affinity='off', dominion=False, reverse=False, same=False, plus=False):
    target = (affinity, dominion, reverse, same, plus)
    return next(row for row in data['summary'] if tuple(row[k] for k in ('affinity', 'dominion', 'reverse', 'same', 'plus')) == target)


def pair_result(data, rule_id, first_kind, second_kind):
    rows = [g for g in data['games'] if g['rule'] == rule_id and set(data['cases'][g['case']]['archetypes']) == {first_kind, second_kind}]
    wins = draws = 0
    for game in rows:
        owner = data['cases'][game['case']]['archetypes'].index(first_kind)
        wins += game['scores'][owner] > game['scores'][1 - owner]
        draws += game['scores'][0] == game['scores'][1]
    return {'games': len(rows), 'wins': wins, 'draws': draws, 'losses': len(rows) - wins - draws, 'points': (wins + draws / 2) / len(rows)}


def aggregate_metrics(data):
    domination = [g for g in data['games'] if data['rules'][g['rule']]['dominion']]
    triggered = [g for g in domination if g['dominionFirstMove']]
    same_plus = []
    for same, plus in [(False, False), (True, False), (False, True), (True, True)]:
        rows = [r for r in data['summary'] if r['same'] == same and r['plus'] == plus]
        same_plus.append({'name': 'Same + Plus' if same and plus else 'Same' if same else 'Plus' if plus else 'Neither',
                          **{k: statistics.mean(r[k] for r in rows) for k in ('drawRate', 'captures', 'comboCaptures', 'sameTriggers', 'plusTriggers', 'starterPoints')}})
    paired = []
    by_match = {(g['rule'], g['case'], g['first']): g for g in data['games']}
    sign = lambda n: (n > 0) - (n < 0)
    for game in domination:
        rule = data['rules'][game['rule']]
        other = find_rule(data, rule['affinity'], False, rule['reverse'], rule['same'], rule['plus'])
        baseline = by_match[other['id'], game['case'], game['first']]
        paired.append(sign(game['scores'][0] - game['scores'][1]) != sign(baseline['scores'][0] - baseline['scores'][1]))
    return {'dominionGames': len(domination), 'dominionActivated': len(triggered),
            'dominionRate': len(triggered) / len(domination),
            'dominionFirstMoveMedian': statistics.median(g['dominionFirstMove'] for g in triggered),
            'dominionFinalCards': statistics.mean(g['finalDominionCards'] for g in domination),
            'dominionCaptureChangeGames': sum(g['dominionCaptureChanges'] > 0 for g in domination) / len(domination),
            'dominionCaptureChanges': statistics.mean(g['dominionCaptureChanges'] for g in domination),
            'dominionPairedOutcomeChanges': statistics.mean(paired),
            'samePlus': same_plus,
            'starterMin': min(r['starterPoints'] for r in data['summary']), 'starterMax': max(r['starterPoints'] for r in data['summary']),
            'minValue': min(g['minValue'] for g in data['games']), 'maxValue': max(g['maxValue'] for g in data['games'])}


def chart(roster, control, out):
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 11, 'text.color': '#e8dec8', 'axes.labelcolor': '#e8dec8',
                         'xtick.color': '#c3b69a', 'ytick.color': '#e8dec8', 'axes.edgecolor': '#716349', 'figure.facecolor': '#14231d', 'axes.facecolor': '#14231d'})
    fig, axes = plt.subplots(2, 2, figsize=(16, 9), layout='constrained')
    fig.suptitle('Tessera · rule effects by deck type', fontsize=24, color='#d8b77a')
    for row, (data, title) in enumerate([(roster, 'Real roster cards'), (control, 'Identical printed values')]):
        for col, reverse in enumerate([False, True]):
            ax = axes[row, col]
            values = np.array([[find_rule(data, affinity, dominion, reverse)['performance'][a]['points'] * 100 for affinity, dominion in CORE] for a in ARCHETYPES])
            im = ax.imshow(values, vmin=0, vmax=100, cmap='RdBu', aspect='auto')
            ax.set_title(title + (' · Reverse' if reverse else ' · higher values win'), color='#d8b77a', pad=14)
            ax.set_xticks(range(6), CORE_LABELS, fontsize=9)
            ax.set_yticks(range(4), LABELS, fontsize=10)
            ax.tick_params(length=0, pad=10)
            for y in range(4):
                for x in range(6):
                    ax.text(x, y, f'{values[y, x]:.0f}%', ha='center', va='center', fontsize=13, color='#16211e' if 23 <= values[y, x] <= 77 else 'white')
    cbar = fig.colorbar(im, ax=axes, shrink=.72, pad=.025, fraction=.025)
    cbar.set_label('Match points · wins + ½ draws', labelpad=14)
    fig.supxlabel('Same and Plus are off here. Every deck faces the other three types; both starting orders are included.\n50% is even. These are AI playtest results, not estimates of human win rates.', fontsize=10, color='#c3b69a')
    fig.savefig(out / 'comparison.png', dpi=150)
    fig.savefig(out / 'comparison.svg')
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--roster', default='build/rule-lab/simulations')
    parser.add_argument('--control', default='build/rule-lab/simulations-control')
    args = parser.parse_args()
    out = ROOT / args.roster
    roster = json.loads((out / 'results.json').read_text())
    control = json.loads((ROOT / args.control / 'results.json').read_text())
    for data in (roster, control):
        assert len(data['games']) == data['metadata']['games']
        assert len(data['summary']) == 48
        for rule in data['summary']:
            assert abs(sum(p['points'] for p in rule['performance'].values()) - 2) < 1e-8
    for row in control['summary']:
        if row['affinity'] == 'off' and not row['dominion']:
            assert all(p['points'] == .5 for p in row['performance'].values()), 'Equal-value control must be symmetric without the new rules'
    metrics = {'roster': aggregate_metrics(roster), 'control': aggregate_metrics(control)}
    chart(roster, control, out)
    with (out / 'deck-results.csv').open('w') as f:
        writer = csv.writer(f)
        writer.writerow(['suite', 'rule', 'deck', 'games', 'wins', 'draws', 'losses', 'match_points'])
        for suite, data in [('roster', roster), ('control', control)]:
            for rule in data['summary']:
                for kind, performance in rule['performance'].items():
                    writer.writerow([suite, rule['name'], kind, *[performance[k] for k in ('games', 'wins', 'draws', 'losses', 'points')]])

    pct = lambda n: f'{n * 100:.1f}%'
    esc = html.escape
    table_rows = []
    for rule in roster['summary']:
        table_rows.append(f'<tr data-affinity="{rule["affinity"]}" data-reverse="{str(rule["reverse"]).lower()}"><td>{esc(rule["name"])}</td>' +
                          ''.join(f'<td>{pct(rule["performance"][a]["points"])}</td>' for a in ARCHETYPES) +
                          f'<td>{pct(rule["drawRate"])}</td><td>{pct(rule["starterPoints"])}</td><td>{rule["captures"]:.2f}</td><td>{pct(rule["dominionRate"]) if rule["dominion"] else "—"}</td></tr>')
    neutral_rows = []
    for reverse in [False, True]:
        for affinity, dominion in CORE:
            rule = find_rule(roster, affinity, dominion, reverse)
            other = find_rule(control, affinity, dominion, reverse)
            real_pair = pair_result(roster, rule['id'], 'construct', 'affinity')
            control_pair = pair_result(control, other['id'], 'construct', 'affinity')
            neutral_rows.append(f'<tr><td>{esc(rule["name"])}</td><td>{real_pair["wins"]} / {real_pair["draws"]} / {real_pair["losses"]}</td><td>{pct(real_pair["points"])}</td><td>{pct(control_pair["points"])}</td></tr>')
    special_rows = ''.join(f'<tr><td>{r["name"]}</td><td>{pct(r["drawRate"])}</td><td>{r["captures"]:.2f}</td><td>{r["comboCaptures"]:.2f}</td><td>{pct(r["starterPoints"])}</td></tr>' for r in metrics['roster']['samePlus'])
    metrics['core'] = []
    for reverse in [False, True]:
        for affinity, dominion in CORE:
            rule = find_rule(roster, affinity, dominion, reverse)
            metrics['core'].append({'name': rule['name'], 'points': {a: rule['performance'][a]['points'] for a in ARCHETYPES},
                                    'constructVsAffinity': pair_result(roster, rule['id'], 'construct', 'affinity'),
                                    'controlPoints': {a: find_rule(control, affinity, dominion, reverse)['performance'][a]['points'] for a in ARCHETYPES}})
    (out / 'analysis.json').write_text(json.dumps(metrics, indent=2))
    m = metrics['roster']
    total = len(roster['games']) + len(control['games'])
    text = f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Tessera · rule simulations</title>
<style>
*{{box-sizing:border-box}}body{{margin:0;background:#0c1712;color:#e6ddc9;font:16px/1.6 Georgia,serif}}main{{max-width:1420px;margin:30px auto;padding:30px 42px;background:radial-gradient(ellipse at 12% 0%,#2d3b30,#16261e 70%);border:1px solid #8e79564d}}header{{border-bottom:1px solid #bfa57038;padding-bottom:22px;margin-bottom:28px}}.brand{{letter-spacing:.3em;color:#d7b77d;font-size:16px}}h1{{font-weight:400;font-size:36px;margin:12px 0}}h2{{font-weight:400;font-size:25px;color:#d7b77d;margin:36px 0 12px}}p{{max-width:1050px}}.muted{{color:#b9ad94}}a{{color:#d7b77d}}.facts{{display:flex;gap:40px;flex-wrap:wrap;color:#b9ad94}}.facts b{{display:block;font-size:25px;font-weight:400;color:#e6ddc9}}figure{{margin:24px 0}}figure img{{width:100%;display:block}}figcaption{{font-size:14px;color:#b9ad94}}.scroll{{overflow:auto}}table{{width:100%;border-collapse:collapse;font-size:15px;font-variant-numeric:tabular-nums}}th,td{{padding:10px 12px;border-bottom:1px solid #bfa57025;text-align:right;white-space:nowrap}}th:first-child,td:first-child{{text-align:left}}th{{color:#d7b77d;font-weight:400}}tbody tr:hover{{background:#bfa5700b}}.filters{{display:flex;gap:18px;margin:12px 0 20px}}select{{background:#142019;color:#e6ddc9;border:1px solid #bfa57055;padding:8px 12px;font:inherit}}.notes{{border-top:1px solid #bfa57038;margin-top:32px;padding-top:10px;color:#b9ad94}}li{{margin:8px 0}}@media(max-width:800px){{main{{margin:10px;padding:20px}}.facts{{gap:20px}}h1{{font-size:28px}}}}
</style></head><body><main>
<header><span class="brand">TESSERA</span><h1>Ascension, Descension &amp; Dominion</h1><p class="muted">AI playtest · {roster['metadata']['created'][:10]}</p><div class="facts"><span><b>{total:,}</b>completed games</span><span><b>48</b>rule combinations</span><span><b>III · V · VII</b>card tiers</span><span><b>Both starters</b>for every deal</span></div></header>
<p>This tests the rules in the browser sample: Ascension or Descension, Dominion, Reverse, Same and Plus. Open hands are used throughout. Ascension and Descension cannot run together. The Skyrim mod is unchanged.</p>
<h2>Deck results</h2><p>The percentages below are <strong>match points: wins plus half of draws</strong>, divided by games played. Each deck type faces the other three equally often. The upper panels use real roster cards. The lower panels give both sides identical printed values and change only their groups and affinities.</p>
<figure><a href="comparison.png"><img src="comparison.png" alt="Deck match points for six core rule sets, with and without Reverse, using real cards and equal printed values"></a><figcaption><a href="comparison.svg">Download chart as SVG</a> · <a href="deck-results.csv">Deck results CSV</a></figcaption></figure>
<h2>What Dominion did</h2><p>Dominion activated in <strong>{pct(m['dominionRate'])}</strong> of games where it was enabled. Its first activation was typically on placement <strong>{m['dominionFirstMoveMedian']:g}</strong>. An average of <strong>{m['dominionFinalCards']:.2f}</strong> of the nine placed cards were weakened by the end.</p>
<p>It changed an immediate capture in <strong>{pct(m['dominionCaptureChangeGames'])}</strong> of those games when the same move was recalculated without Dominion. Replaying matched deals with Dominion enabled changed the win/draw/loss outcome in <strong>{pct(m['dominionPairedOutcomeChanges'])}</strong> of games. That second comparison includes the AI choosing different moves.</p>
<h2>Constructs against one-affinity decks</h2><p>Results are from the Construct side. Same and Plus are off in this table. Each roster row contains {roster['metadata']['sampleBatches'] * 2} games; each equal-value row contains {control['metadata']['sampleBatches'] * 2} games.</p>
<div class="scroll"><table><thead><tr><th>Rules</th><th>Wins / draws / losses</th><th>Roster points</th><th>Equal-value points</th></tr></thead><tbody>{''.join(neutral_rows)}</tbody></table></div>
<h2>Same and Plus</h2><p>Averages across the twelve combinations of affinity rule, Dominion and Reverse. Captures include cards flipped more than once during a game. Combo captures are the subsequent flips following Same or Plus.</p>
<div class="scroll"><table><thead><tr><th>Extra rules</th><th>Draws</th><th>Captures/game</th><th>Combo captures/game</th><th>Starter points</th></tr></thead><tbody>{special_rows}</tbody></table></div>
<h2>Starting advantage</h2><p>The starting player scored between {pct(m['starterMin'])} and {pct(m['starterMax'])} across the real-card combinations. Reverse + Plus showed a much stronger starting advantage with identical printed hands: {pct(find_rule(control, reverse=True, plus=True)['starterPoints'])}, compared with {pct(find_rule(roster, reverse=True, plus=True)['starterPoints'])} in the real-card deals. That makes symmetric hands worth a closer playtest; the limited AI search may also contribute.</p>
<h2>All 48 combinations · real roster</h2><div class="filters"><select id="affinity" aria-label="Filter affinity rule"><option value="all">All affinity rules</option><option value="off">No affinity rule</option><option value="ascension">Ascension</option><option value="descension">Descension</option></select><select id="reverse" aria-label="Filter Reverse"><option value="all">With and without Reverse</option><option value="false">Higher values win</option><option value="true">Reverse</option></select></div>
<div class="scroll"><table id="all-rules"><thead><tr><th>Rules</th>{''.join(f'<th>{label}</th>' for label in LABELS)}<th>Draws</th><th>Starter points</th><th>Captures/game</th><th>Dominion active</th></tr></thead><tbody>{''.join(table_rows)}</tbody></table></div>
<h2>How the games were played</h2><ul>
<li>{len(roster['games']):,} games with unmodified roster cards: {len(roster['cases'])} deals, replayed with all 48 combinations and both starting orders. Every individual match uses one tier.</li>
<li>{len(control['games']):,} games with identical printed values on both sides. These artificial decks isolate the group and affinity effects. Without the new rules, every type scored exactly 50% in this control.</li>
<li>One-affinity decks use five cards sharing an affinity but belonging to distinct groups. One-group decks share a group; their affinities come from the roster. Construct decks are all Neutral. Mixed decks use distinct groups and distinct affinities, with Neutral sometimes included.</li>
<li>Both players use the same rule-aware browser AI. It looks up to three moves ahead early, then searches toward the endgame, with a limit of {roster['metadata']['budget']:,} search nodes per move.</li>
<li>Ascension and Descension affect matching cards in both hands and on the board, capped at ±3. Neutral is unaffected. Dominion gives −1 when two adjacent cards share a group the target does not belong to; ownership does not matter and the penalty does not stack.</li>
<li>Modifiers update before captures. Same and Plus use printed values; ordinary and Combo captures use effective values. Values are not clamped to 1–10. Observed effective values ranged from {m['minValue']} to {m['maxValue']}.</li>
<li>Every game checked that all ten cards were retained and the total score remained ten.</li></ul>
<div class="notes"><p>These results identify large advantages in this playtest. They do not prove balance for human play, other tiers, custom expansions or carefully optimized decks. The same finite deals are reused across combinations, so these are matched comparisons rather than thousands of independent deck samples. Other Tessera rules, including Legion, Decimation and board-tile Affinity, are outside this test.</p>
<p><a href="summary.csv">Rule summary CSV</a> · <a href="results.json">Full roster results and move records</a> · <a href="../{esc(Path(args.control).name)}/results.json">Full equal-value results</a> · <a href="/tools/rule-lab/">Play the browser sample</a></p></div>
</main><script>for(const id of ['affinity','reverse'])document.getElementById(id).onchange=()=>{{for(const row of document.querySelectorAll('#all-rules tbody tr'))row.hidden=['affinity','reverse'].some(key=>document.getElementById(key).value!=='all'&&row.dataset[key]!==document.getElementById(key).value)}};</script></body></html>'''
    (out / 'report.html').write_text(text)
    print(json.dumps(metrics, indent=2))
    print(out / 'report.html')


if __name__ == '__main__':
    main()
