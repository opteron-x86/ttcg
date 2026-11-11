#!/usr/bin/env python3
"""Bake a UI mod's artwork into a Tessera skin.

Skyrim UI mods keep their panels inside SWF files, as vector shapes or as
bitmaps. This reads an installed copy and writes the pieces named in
tools/skin/recipes.json to TesseraUI/skins/<id>. A shape is rendered at three
times Skyrim's 720p stage; a bitmap (shape "bitmap": the largest in the file)
is copied at its own size, and its skin.json entry carries "scale": 3. A
fourth recipe field may hold {"mirror_top": N}, which replaces the top N rows
with the bottom N mirrored, to drop an ornament that overhangs the top edge.
The skin's skin.json, which describes how each texture is drawn, is written by
hand and left untouched. Artwork is used with its authors' permission; see
docs/licenses/Skin-credits.txt.

    python3 tools/build-skin.py oathvein --mods /path/to/mods

Requires rsvg-convert and ImageMagick.
"""
import argparse, contextlib, io, json, pathlib, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/skin'))
import swf_bitmaps  # noqa: E402
import swf_shapes  # noqa: E402

SCALE = 3


def resolve(base, relative):
    """Mods disagree about the case of Interface and its subfolders."""
    current = base
    for part in pathlib.PurePosixPath(relative).parts:
        matches = [p for p in current.iterdir() if p.name.lower() == part.lower()]
        if not matches:
            raise FileNotFoundError(f'{relative} is missing from {base}')
        current = matches[0]
    return current


def bake(skin, recipe, mod, out):
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as work:
        rendered = {}
        for name, (source, shape, crop, *extra) in recipe['art'].items():
            key = (source, shape)
            if key not in rendered:
                folder = pathlib.Path(work) / source.replace('/', '_')
                path = resolve(mod, 'Interface/' + source)
                if shape == 'bitmap':
                    with contextlib.redirect_stdout(io.StringIO()):
                        swf_bitmaps.main(str(path), str(folder))
                    rendered[key] = max(folder.glob('*.png'), key=lambda p: p.stat().st_size)
                else:
                    if not folder.exists():
                        swf_shapes.main(str(path), str(folder), SCALE)
                    png = folder / f'shape{shape}.png'
                    subprocess.run(['rsvg-convert', str(folder / f'shape{shape}.svg'), '-o', str(png)], check=True)
                    rendered[key] = png
            command = ['magick', str(rendered[key])]
            if crop == 'trim':
                command += ['-trim', '+repage']
            elif crop:
                command += ['-crop', crop, '+repage']
            rows = (extra[0] if extra else {}).get('mirror_top')
            if rows:
                command += ['(', '+clone', '-flip', '-crop', f'x{rows}+0+0', '+repage', ')', '-gravity', 'north',
                            '-compose', 'copy', '-composite']
            subprocess.run(command + ['-strip', f'PNG32:{out / (name + ".png")}'], check=True)
            size = subprocess.check_output(['magick', 'identify', '-format', '%wx%h', str(out / (name + '.png'))], text=True)
            print(f'{skin}/{name}.png {size}')


def main():
    recipes = json.loads((ROOT / 'tools/skin/recipes.json').read_text())
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('skins', nargs='*', help='skin ids; all when omitted')
    parser.add_argument('--mods', required=True, type=pathlib.Path, help='folder containing the installed UI mods')
    parser.add_argument('--out', type=pathlib.Path, default=ROOT / 'TesseraUI/skins')
    args = parser.parse_args()
    for skin in args.skins or recipes:
        recipe = recipes[skin]
        bake(skin, recipe, args.mods / recipe['mod'], args.out / skin)


if __name__ == '__main__':
    main()
