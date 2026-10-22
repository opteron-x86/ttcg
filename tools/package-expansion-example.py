#!/usr/bin/env python3
"""Package the optional example mod and author guide separately from releases."""
import importlib.util
import json
from pathlib import Path

from runtime_files import ROOT, hashes

spec = importlib.util.spec_from_file_location('release_package', ROOT / 'tools/package.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


def main():
    editor_spec = importlib.util.spec_from_file_location('expansion_editor', ROOT / 'tools/build-expansion-editor.py')
    editor = importlib.util.module_from_spec(editor_spec)
    editor_spec.loader.exec_module(editor)
    editor_path = ROOT / 'dist/expansions/Tessera-Expansion-Workshop.html'
    editor_path.parent.mkdir(parents=True, exist_ok=True)
    editor_path.write_text(editor.build_page())
    output = ROOT / 'dist/expansions'
    sample = ROOT / 'examples/my-cool-cards'
    files = {p.relative_to(sample).as_posix(): p for p in sample.rglob('*') if p.is_file()}
    opponent_sample = ROOT / 'examples/opponent-integration'
    opponents = {p.relative_to(opponent_sample).as_posix(): p for p in opponent_sample.rglob('*') if p.is_file()}
    guide = {'README.md': ROOT / 'docs/expansion-authoring-readme.md',
             'card-expansions.md': ROOT / 'docs/card-expansions.md',
             'card-art-replacers.md': ROOT / 'docs/card-art-replacers.md',
             'expansion-editor.html': editor_path,
             'Expansion-Workshop-README.txt': ROOT / 'tools/expansion-editor/README.txt',
             'opponent-integrations.md': ROOT / 'docs/opponent-integrations.md',
             'Scripts/Source/TTCG_Opponents.psc': ROOT / 'Scripts/Source/TTCG_Opponents.psc',
             'Scripts/Source/TTCG_Native.psc': ROOT / 'Scripts/Source/TTCG_Native.psc',
             'Scripts/Source/TTCG_CardExpansion.psc': ROOT / 'Scripts/Source/TTCG_CardExpansion.psc'}
    guide.update({'example/' + name: path for name, path in files.items()})
    guide.update({'opponent-example/' + name: path for name, path in opponents.items()})
    guide.update({'opponent-templates/' + p.name: p for p in (ROOT / 'examples/opponent-authoring').iterdir() if p.is_file()})
    manifests = {}
    for name, entries in [('Tessera-Example-Expansion.zip', files), ('Tessera-Example-Opponents.zip', opponents), ('Tessera-Expansion-Authoring.zip', guide)]:
        expected = hashes(entries)
        package.write_archive(entries, expected, output / name)
        manifests[name] = expected
        print(f'Verified {output / name} ({len(entries)} files)')
    report = ROOT / 'build/expansions/example-manifest.json'
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(manifests, indent=2) + '\n')


if __name__ == '__main__':
    main()
