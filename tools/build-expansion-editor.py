#!/usr/bin/env python3
"""Build the portable, offline expansion workshop. No runtime files are changed."""
import argparse
import base64
import io
import json
from pathlib import Path
import zipfile

from PIL import Image
from card_rank_policy import POLICY, rank_checks

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'tools/expansion-editor'


def build_page():
    profiles = json.loads((ROOT / 'tools/catalog-audit/roster-profiles.json').read_text())
    groups = sorted(g['name'] for g in json.loads((ROOT / 'data/creature-groups.json').read_text())['groups'])
    # Match the workshop's Dragon exception, from the same proposal source.
    grouping = json.loads((ROOT / 'tools/catalog-audit/grouping-proposal.json').read_text())
    stream = io.BytesIO()
    with Image.open(ROOT / 'TesseraUI/art/backs/mosaic.webp') as back:
        back.thumbnail((300, 400), Image.Resampling.LANCZOS)
        back.save(stream, 'WEBP', quality=90)
    data = {'policy': POLICY, 'rankChecks': rank_checks(), 'workshopPolicy': profiles,
            'groups': groups, 'dragonCurve': grouping['dragon_curve'],
            'cardBack': 'data:image/webp;base64,' + base64.b64encode(stream.getvalue()).decode('ascii')}
    payload = json.dumps(data, ensure_ascii=False, separators=(',', ':')).replace('<', '\\u003c').replace('&', '\\u0026')
    page = (SOURCE / 'index.html').read_text()
    # Use the same local SVG artwork as the shared in-game card renderer.
    app = (ROOT / 'TesseraUI/app.js').read_text()
    page = page.replace('/* AFFINITY_JS */', app[app.index('const affinityNames='):app.index('const tierMetrics=')])
    pieces = {'EDITOR_CSS': SOURCE / 'editor.css', 'ZIP_JS': SOURCE / 'vendor/fflate-0.8.3.js',
              'RANK_JS': ROOT / 'tools/catalog-audit/rank-tools.js', 'CORE_JS': SOURCE / 'core.js',
              'FILES_JS': SOURCE / 'files.js', 'APP_JS': SOURCE / 'app.js'}
    for marker, path in pieces.items():
        content = path.read_text()
        if marker == 'ZIP_JS':
            content = '/* fflate 0.8.3 — https://github.com/101arrowz/fflate\n' + (SOURCE / 'vendor/fflate-LICENSE.txt').read_text() + '\n*/\n' + content
        page = page.replace('/* ' + marker + ' */', content)
    return page.replace('<!-- EDITOR_DATA -->', payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    page = build_page()
    targets = [ROOT / 'docs/expansion-editor.html', ROOT / 'dist/expansions/Tessera-Expansion-Workshop.html']
    for path in targets:
        if args.check:
            if not path.exists() or path.read_text() != page:
                raise SystemExit(f'Out of date: {path}. Run python3 tools/build-expansion-editor.py')
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(page)
            print('Built', path.relative_to(ROOT))
    if not args.check:
        archive = ROOT / 'dist/expansions/Tessera-Expansion-Workshop.zip'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as out:
            out.writestr('Tessera-Expansion-Workshop.html', page)
            out.write(SOURCE / 'README.txt', 'README.txt')
        with zipfile.ZipFile(archive) as check:
            assert check.testzip() is None
            assert check.read('Tessera-Expansion-Workshop.html').decode() == page
        print('Verified', archive.relative_to(ROOT))


if __name__ == '__main__':
    main()
