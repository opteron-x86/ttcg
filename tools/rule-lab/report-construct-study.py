#!/usr/bin/env python3
"""Compare Construct matchups with smaller and later affinity modifiers."""
import csv
import html
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/rule-lab/construct-study'
VERSIONS = [('Original: cap 3, first card', ROOT / 'build/rule-lab/simulations'),
            ('Cap 2, first card', OUT / 'cap2-first'),
            ('Cap 3, second card', OUT / 'cap3-second'),
            ('Cap 2, second card', OUT / 'cap2-second')]
KEYS = ('affinity', 'reverse', 'dominion', 'same', 'plus')
GROUPS = [('affinity', 'One affinity'), ('group', 'One group'), ('mixed', 'Mixed')]


def targeted(rule):
    return (rule['affinity'] == 'descension' and not rule['reverse']) or (rule['affinity'] == 'ascension' and rule['reverse'])


def results(data, rule, opponent=None):
    games = [g for g in data['games'] if g['rule'] == rule['id'] and 'construct' in data['cases'][g['case']]['archetypes']
             and (opponent is None or opponent in data['cases'][g['case']]['archetypes'])]
    wins = draws = 0
    for game in games:
        side = data['cases'][game['case']]['archetypes'].index('construct')
        wins += game['scores'][side] > game['scores'][1 - side]
        draws += game['scores'][0] == game['scores'][1]
    return dict(games=len(games), wins=wins, draws=draws, losses=len(games) - wins - draws,
                points=(wins + draws / 2) / len(games))


def main():
    data = [json.loads((path / 'results.json').read_text()) for _, path in VERSIONS]
    original = data[0]
    for variant in data[1:]:
        assert variant['metadata']['rosterSha256'] == original['metadata']['rosterSha256']
        assert variant['metadata']['budget'] == original['metadata']['budget']
        assert len(variant['games']) == variant['metadata']['games'] == 1440
        for case in variant['cases']:
            previous = original['cases'][case['originalCase']]
            assert case['hands'] == previous['hands'] and case['archetypes'] == previous['archetypes']

    rules = sorted([r for r in original['rules'] if targeted(r)],
                   key=lambda r: (r['affinity'] != 'descension', r['dominion'], r['same'], r['plus']))
    rows = []
    for rule in rules:
        row = {'name': rule['name'], **{k: rule[k] for k in KEYS}, 'versions': []}
        for (label, _), suite in zip(VERSIONS, data):
            match = next(r for r in suite['rules'] if all(r[k] == rule[k] for k in KEYS))
            record = {'label': label, 'all': results(suite, match),
                      'opponents': {key: results(suite, match, key) for key, _ in GROUPS}}
            assert record['all']['games'] == 90
            assert all(p['games'] == 30 for p in record['opponents'].values())
            row['versions'].append(record)
        rows.append(row)
    (OUT / 'comparison.json').write_text(json.dumps(rows, indent=2))
    with (OUT / 'comparison.csv').open('w') as f:
        writer = csv.writer(f)
        writer.writerow(['rules', 'version', 'opponent', 'games', 'wins', 'draws', 'losses', 'match_points'])
        for row in rows:
            for version in row['versions']:
                for opponent, record in [('all', version['all']), *version['opponents'].items()]:
                    writer.writerow([row['name'], version['label'], opponent, *[record[k] for k in ('games', 'wins', 'draws', 'losses', 'points')]])

    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 11, 'text.color': '#e8dec8',
                         'xtick.color': '#c3b69a', 'ytick.color': '#e8dec8', 'axes.edgecolor': '#716349',
                         'figure.facecolor': '#14231d', 'axes.facecolor': '#14231d'})
    fig, axes = plt.subplots(1, 2, figsize=(15, 7), layout='constrained')
    fig.suptitle('Tessera · how much do Constructs benefit?', color='#d8b77a', fontsize=22)
    for ax, affinity in zip(axes, ['descension', 'ascension']):
        selected = [row for row in rows if row['affinity'] == affinity]
        values = np.array([[v['all']['points'] * 100 for v in row['versions']] for row in selected])
        im = ax.imshow(values, cmap='RdBu_r', vmin=0, vmax=100, aspect='auto')
        ax.set_title('Descension' if affinity == 'descension' else 'Ascension + Reverse', color='#d8b77a', pad=15)
        ax.set_xticks(range(4), ['Cap 3\nfirst card', 'Cap 2\nfirst card', 'Cap 3\nsecond card', 'Cap 2\nsecond card'], fontsize=10)
        ax.set_yticks(range(8), [' + '.join(name for key, name in [('dominion', 'Dominion'), ('same', 'Same'), ('plus', 'Plus')] if row[key]) or 'No extra rules' for row in selected], fontsize=10)
        ax.tick_params(length=0, pad=10)
        for y in range(8):
            for x in range(4):
                ax.text(x, y, f'{values[y, x]:.0f}%', ha='center', va='center', fontsize=13,
                        color='#16211e' if 23 <= values[y, x] <= 77 else 'white')
    bar = fig.colorbar(im, ax=axes, shrink=.75, fraction=.02, pad=.025)
    bar.set_label('Construct match points', color='#c3b69a', labelpad=10)
    fig.supxlabel('Wins + half of draws · same 90 games per cell · both starting orders\nConstructs face one-affinity, one-group and mixed decks equally often.', fontsize=11, color='#c3b69a')
    fig.savefig(OUT / 'comparison.png', dpi=150)
    fig.savefig(OUT / 'comparison.svg')
    plt.close(fig)

    def cell(record):
        return f'<td title="{record["wins"]} wins, {record["draws"]} draws, {record["losses"]} losses">{record["points"] * 100:.1f}%</td>'

    overview = ''.join(f'<tr><th>{html.escape(row["name"])}</th>' + ''.join(cell(v['all']) for v in row['versions']) + '</tr>' for row in rows)
    details = ''
    for row in rows:
        for key, label in GROUPS:
            details += f'<tr><th>{html.escape(row["name"])}</th><td>{label}</td>' + ''.join(cell(v['opponents'][key]) for v in row['versions']) + '</tr>'
    labels = ''.join(f'<th>{label}</th>' for label, _ in VERSIONS)
    total = sum(len(d['games']) for d in data[1:])
    text = f'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Tessera · Construct study</title><style>
*{{box-sizing:border-box}}body{{margin:0;background:#0c1712;color:#e6ddc9;font:16px/1.65 Georgia,serif}}main{{max-width:1440px;margin:30px auto;padding:32px 40px;background:radial-gradient(ellipse at 12% 0%,#2d3b30,#16261e 70%);border:1px solid #8e79564d}}.brand{{letter-spacing:.3em;color:#d7b77d}}h1,h2{{font-weight:400;color:#d7b77d}}h1{{font-size:34px}}h2{{margin-top:36px}}p{{max-width:1080px}}a{{color:#d7b77d}}.muted{{color:#b9ad94}}figure{{margin:24px 0}}img{{width:100%;display:block}}figcaption{{font-size:14px}}.scroll{{overflow:auto}}table{{width:100%;border-collapse:collapse;font-size:15px;font-variant-numeric:tabular-nums}}th,td{{padding:11px 12px;border-bottom:1px solid #bfa57025;text-align:right}}th{{font-weight:400;color:#d7b77d}}th:first-child{{text-align:left}}tbody tr:hover{{background:#bfa5700b}}details{{margin:25px 0}}summary{{cursor:pointer;color:#d7b77d}}@media(max-width:800px){{main{{margin:10px;padding:20px}}h1{{font-size:28px}}table{{min-width:800px}}}}
</style></head><body><main><span class="brand">TESSERA</span><h1>Constructs, smaller caps &amp; later activation</h1>
<p class="muted">{total:,} additional AI games · 16 rule combinations · three alternatives · tiers III, V and VII</p>
<p>These runs reuse the exact Construct matchups from the original study, including both starting orders. Each column contains 90 games per combination: 30 against a one-affinity deck, 30 against a one-group deck and 30 against a mixed deck. The original column reuses earlier results.</p>
<p><strong>Percentages are match points: wins plus half of draws.</strong> Hover over a table result to see its wins, draws and losses. A high percentage means a strong Construct advantage in these samples.</p>
<figure><a href="comparison.png"><img src="comparison.png" alt="Construct match points under four cap and activation choices, with Descension and Ascension Reverse"></a><figcaption><a href="comparison.svg">SVG chart</a> · <a href="comparison.csv">Full comparison CSV</a></figcaption></figure>
<h2>All tested combinations</h2><div class="scroll"><table><thead><tr><th>Rules</th>{labels}</tr></thead><tbody>{overview}</tbody></table></div>
<details><summary>Break down the results by opposing deck</summary><p>Each result below contains 30 games, using 15 deals with both starting orders.</p><div class="scroll"><table><thead><tr><th>Rules</th><th>Opponent</th>{labels}</tr></thead><tbody>{details}</tbody></table></div></details>
<h2>What “second card” means</h2><p>With one card of an affinity on the board, its modifier is zero. With two, it becomes ±1; with three, ±2; with four, ±3 if that cap is allowed. The newly placed card counts before captures. Counts include both players, and Neutral stays unaffected.</p>
<p>Against a fully Neutral deck, five different affinities never reach the second-card threshold. A mixed deck therefore avoids the affinity penalty in that matchup. Against an opponent with matching affinities, both players still contribute to the count.</p>
<h2>Limits</h2><p>The same finite hands are reused across alternatives. The AI sees both hands and uses the same limited search as the first study. These comparisons show how the rules affect these decks; they do not establish a guaranteed winning strategy against human players or optimized counter-decks. Dominion is still −1 and does not stack. Other rules, including Legion and Decimation, were not tested here.</p>
<p class="muted">The browser table keeps its original defaults, and the Skyrim mod is unchanged.</p><p><a href="comparison.json">Detailed comparison data</a> · <a href="../simulations/report.html">Original 48-combination study</a> · <a href="/tools/rule-lab/">Play the sample</a></p></main></body></html>'''
    (OUT / 'report.html').write_text(text)
    for row in rows:
        print(row['name'], ' | '.join(f'{v["all"]["points"] * 100:.1f}%' for v in row['versions']))
    print(OUT / 'report.html')


if __name__ == '__main__':
    main()
