#!/usr/bin/env python3
"""Render the candidate shapes of an installed UI mod onto one contact sheet, to pick recipe entries.
usage: python3 -I tools/skin/survey.py MOD_DIR OUT_DIR   (writes OUT_DIR/sheet.png)"""
import sys, os, glob, subprocess, io, contextlib, pathlib
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import swf_shapes
mod, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
interface = next(p for p in mod.iterdir() if p.name.lower() == 'interface')
def find(rel):
    cur = interface
    for part in rel.split('/'):
        match = [p for p in cur.iterdir() if p.name.lower() == part.lower()]
        if not match: return None
        cur = match[0]
    return cur
fixed = ['deardiary_dm/messagebox/messagebox_BG.swf', 'deardiary_dm/Inventory/inventorylists_BG.swf',
         'deardiary_dm/Inventory/itemcard_1_BG.swf', 'deardiary_dm/Inventory/inventorylists_categoryCenter.swf',
         'deardiary_dm/configpanel/configpanel_window_BG.swf', 'deardiary_dm/Inventory/inventorylists_selector.swf',
         'deardiary_dm/quest_journal/quest_journal_BG.swf', 'deardiary_dm/Inventory/bottombar_one.swf',
         'deardiary_dm/levelupmenu/levelupmenu_BG.swf', 'deardiary_dm/sleepwaitmenu/sleepwaitmenu_BG.swf']
scan = ['skyui/inventorylists.swf', 'quest_journal.swf', 'tweenmenu.swf', 'skyui/itemcard.swf', 'skyui/bottombar.swf']
rows = []
for rel in fixed + scan:
    path = find(rel)
    if not path: print('missing', rel); continue
    folder = out / rel.replace('/', '_')[:-4]
    with contextlib.redirect_stdout(io.StringIO()):
        try: shapes = swf_shapes.main(str(path), str(folder), 3)
        except Exception as e: print('fail', rel, e, file=sys.stderr); continue
    for cid, w, h, nf, nl in shapes:
        if rel in scan and (nf + nl < 150 or w * h < 3000): continue
        if nf + nl == 0: continue
        svg = folder / f'shape{cid}.svg'
        png = folder / f'shape{cid}.png'
        subprocess.run(['rsvg-convert', str(svg), '-o', str(png)], capture_output=True)
        if png.exists(): rows.append((rel, cid, int(w), int(h), nf + nl, str(png)))
for r in rows: print(*r[:5])
labels = []
args = ['magick', 'montage']
for rel, cid, w, h, n, png in rows:
    args += ['-label', f'{pathlib.Path(rel).stem}#{cid} {w}x{h}', png]
args += ['-background', '#5b6f86', '-fill', 'white', '-pointsize', '13', '-geometry', '380x260>+8+8', '-tile', '6x', str(out / 'sheet.png')]
subprocess.run(args, check=True)
