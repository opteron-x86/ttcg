#!/usr/bin/env python3
"""Generate the game board materials in TesseraUI/assets/boards.

Each board is three textures drawn by native/src/GameMatch.cpp:

  surface.webp the playing field, stretched across the board
  frame.png    the rail around it, a nine-slice with 96 px corners and a
               64 px rail; the remaining 32 px is the rail's shadow on the field
  cell.png     shared by every board: the recess of one empty square

The field of each material is painted source art from tools/board-art where a
file exists, and a procedural SVG filter otherwise; rails, shadows and the
recess are always drawn here. Rendering uses rsvg-convert and ImageMagick.

    python3 tools/build-board-textures.py
"""
import argparse, base64, pathlib, subprocess, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'TesseraUI/assets/boards'
W, H = 768, 1024
F = 512  # frame texture


def noise(ident, frequency, octaves, seed, matrix, kind='fractalNoise'):
    return (f'<filter id="{ident}" x="0" y="0" width="100%" height="100%" color-interpolation-filters="sRGB">'
            f'<feTurbulence type="{kind}" baseFrequency="{frequency}" numOctaves="{octaves}" seed="{seed}"/>'
            f'<feColorMatrix type="matrix" values="{matrix}"/></filter>')


def relief(ident, frequency, octaves, seed, scale, azimuth=225, elevation=58):
    return (f'<filter id="{ident}" x="0" y="0" width="100%" height="100%">'
            f'<feTurbulence type="fractalNoise" baseFrequency="{frequency}" numOctaves="{octaves}" seed="{seed}"/>'
            f'<feDiffuseLighting surfaceScale="{scale}" diffuseConstant="1.05" lighting-color="#fff">'
            f'<feDistantLight azimuth="{azimuth}" elevation="{elevation}"/></feDiffuseLighting></filter>')


def tint(r, g, b, gain, offset, channel=0):
    """Colour matrix that paints a flat colour whose alpha follows one noise channel."""
    alpha = ['0', '0', '0']
    alpha[channel] = str(gain)
    return f'0 0 0 0 {r}  0 0 0 0 {g}  0 0 0 0 {b}  {" ".join(alpha)} 0 {offset}'


VIGNETTE = ('<radialGradient id="v" cx="50%" cy="46%" r="78%"><stop offset="0.5" stop-color="#000" stop-opacity="0"/>'
            '<stop offset="1" stop-color="#000" stop-opacity="{}"/></radialGradient>')


def layers(base, defs, body, vignette=.45, w=W, h=H):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}"><defs>'
            f'{defs}{VIGNETTE.format(vignette)}</defs><rect width="{w}" height="{h}" fill="{base}"/>{body}'
            f'<rect width="{w}" height="{h}" fill="url(#v)"/></svg>')


def full(ident, opacity=1, blend=None, w=W, h=H):
    style = f' style="mix-blend-mode:{blend}"' if blend else ''
    return f'<rect width="{w}" height="{h}" filter="url(#{ident})" opacity="{opacity}"{style}/>'


def scratches(seed, count, colour, w=W, h=H):
    """Deterministic fine scratches for the dark slab."""
    state = seed
    out = []
    for _ in range(count):
        values = []
        for _ in range(5):
            state = (state * 1103515245 + 12345) & 0x7fffffff
            values.append(state / 0x7fffffff)
        x, y, angle, length, strength = values
        dx, dy = (angle - .5) * 2, 1 if angle > .5 else -1
        out.append(f'<line x1="{x * w:.1f}" y1="{y * h:.1f}" x2="{x * w + dx * length * 220:.1f}" '
                   f'y2="{y * h + dy * length * 220:.1f}" stroke="{colour}" stroke-width="{.5 + strength:.2f}" '
                   f'stroke-opacity="{.03 + strength * .07:.3f}"/>')
    return ''.join(out)


# name: (surface svg, rail colours light/body/dark, rail overlay svg fragment)
def materials():
    wood = lambda w=W, h=H: (
        noise('streak', '0.004 0.085', 5, 7, tint(.17, .09, .045, 2.1, -.78)) +
        noise('pore', '0.02 0.9', 3, 3, tint(.08, .04, .02, 1.5, -.6, 1)) +
        noise('glow', '0.003 0.02', 3, 11, tint(.62, .40, .22, 1.3, -.5)) + relief('rel', '0.006 0.12', 4, 7, 2.2),
        full('glow', .5, w=w, h=h) + full('streak', 1, w=w, h=h) + full('pore', 1, w=w, h=h) +
        full('rel', .16, 'multiply', w, h))
    stone = lambda w=W, h=H: (
        noise('mottle', '0.012 0.016', 5, 5, tint(.60, .66, .70, 1.5, -.62)) +
        noise('dark', '0.006 0.007', 4, 9, tint(.03, .04, .05, 1.8, -.7)) +
        noise('speck', '0.9', 2, 2, tint(.9, .93, .95, 1.2, -.92)) + relief('rel', '0.035', 5, 5, 3.2),
        full('mottle', .55, w=w, h=h) + full('dark', .8, w=w, h=h) + full('rel', .30, 'multiply', w, h) +
        full('speck', .35, w=w, h=h))
    felt = lambda w=W, h=H: (
        noise('nap', '0.85', 3, 4, tint(.02, .08, .05, 1.6, -.62)) +
        noise('lint', '0.9', 2, 8, tint(.55, .78, .66, 1.3, -.94)) +
        noise('wear', '0.006 0.008', 3, 6, tint(.40, .64, .52, 1.2, -.5)),
        full('wear', .22, w=w, h=h) + full('nap', .8, w=w, h=h) + full('lint', .5, w=w, h=h))
    parchment = lambda w=W, h=H: (
        noise('blotch', '0.005 0.006', 4, 12, tint(.48, .33, .17, 1.5, -.55)) +
        noise('fibre', '0.5 0.03', 3, 5, tint(.36, .25, .12, 1.3, -.72)) +
        noise('tooth', '0.8', 2, 1, tint(1, .97, .88, 1.2, -.72)),
        full('blotch', .45, w=w, h=h) + full('fibre', .5, w=w, h=h) + full('tooth', .28, w=w, h=h))
    leather = lambda w=W, h=H: (
        noise('stain', '0.006 0.008', 4, 14, tint(.07, .04, .03, 1.7, -.6)) + relief('rel', '0.11', 3, 6, 4.5) +
        noise('sheen', '0.01 0.012', 3, 2, tint(.75, .56, .40, 1.2, -.62)),
        full('sheen', .22, w=w, h=h) + full('stain', .8, w=w, h=h) + full('rel', .5, 'multiply', w, h))
    slab = lambda w=W, h=H: (
        noise('mottle', '0.01 0.012', 4, 3, tint(.5, .5, .5, 1.3, -.62)) + relief('rel', '0.5', 3, 8, 1.2),
        full('mottle', .18, w=w, h=h) + full('rel', .22, 'multiply', w, h) + scratches(41, 46, '#e9e6dd', w, h))
    # name: field colour, field texture, rail colours, vignette, and the rail's own material.
    return {
        'slate': ('#39424a', stone, ('#59646c', '#2a3136', '#14181b'), .5, ('#39424a', stone)),
        'felt': ('#1d4a39', felt, ('#8a6238', '#4d3017', '#22140a'), .5, ('#4b2e1c', wood)),
        'parchment': ('#d7c193', parchment, ('#8a6a40', '#5b4226', '#2e2011'), .32, ('#5b4226', leather)),
        'leather': ('#4a342a', leather, ('#5e4334', '#33231b', '#150d0a'), .5, ('#4a342a', leather)),
        'walnut': ('#4b2e1c', wood, ('#8a6238', '#4d3017', '#22140a'), .45, ('#4b2e1c', wood)),
        'slab': ('#1c1c1c', slab, ('#3a3a3a', '#202020', '#0a0a0a'), .5, ('#1c1c1c', slab)),
    }


def frame_svg(base, texture, rail):
    """A rail with a bevel; its inner 32 px are only the shadow it casts on the field."""
    light, body, dark = rail
    defs, body_layers = texture(F, F)
    outer, lip = 64, 96
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{F}" height="{F}" viewBox="0 0 {F} {F}"><defs>{defs}'
            f'<mask id="rail"><rect width="{F}" height="{F}" rx="20" fill="#fff"/>'
            f'<rect x="{outer}" y="{outer}" width="{F - 2 * outer}" height="{F - 2 * outer}" rx="6" fill="#000"/></mask>'
            f'<linearGradient id="bevel" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="{light}"/>'
            f'<stop offset="0.5" stop-color="{body}"/><stop offset="1" stop-color="{dark}"/></linearGradient>'
            f'<filter id="soft" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="9"/></filter>'
            f'<clipPath id="field"><rect x="{outer}" y="{outer}" width="{F - 2 * outer}" height="{F - 2 * outer}" rx="6"/></clipPath>'
            f'</defs>'
            # Shadow of the rail on the field: a blurred dark ring clipped to the opening.
            f'<g clip-path="url(#field)"><rect x="{outer - 14}" y="{outer - 14}" width="{F - 2 * outer + 28}" '
            f'height="{F - 2 * outer + 28}" rx="10" fill="none" stroke="#000" stroke-opacity=".75" stroke-width="34" '
            f'filter="url(#soft)"/></g>'
            f'<g mask="url(#rail)"><rect width="{F}" height="{F}" fill="{base}"/>{body_layers}'
            f'<rect width="{F}" height="{F}" fill="url(#bevel)" opacity=".62"/></g>'
            # Outer and inner edges.
            f'<rect x="1.5" y="1.5" width="{F - 3}" height="{F - 3}" rx="19" fill="none" stroke="#000" stroke-opacity=".8" stroke-width="3"/>'
            f'<rect x="5" y="5" width="{F - 10}" height="{F - 10}" rx="16" fill="none" stroke="{light}" stroke-opacity=".55" stroke-width="2"/>'
            f'<rect x="{outer - 7}" y="{outer - 7}" width="{F - 2 * outer + 14}" height="{F - 2 * outer + 14}" rx="9" fill="none" stroke="{dark}" stroke-opacity=".9" stroke-width="5"/>'
            f'<rect x="{outer - 1}" y="{outer - 1}" width="{F - 2 * outer + 2}" height="{F - 2 * outer + 2}" rx="6" fill="none" stroke="{light}" stroke-opacity=".35" stroke-width="2"/>'
            f'</svg>')


CELL = ('<svg xmlns="http://www.w3.org/2000/svg" width="384" height="512" viewBox="0 0 384 512"><defs>'
        '<filter id="soft" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="10"/></filter>'
        '<clipPath id="slot"><rect x="3" y="3" width="378" height="506" rx="9"/></clipPath></defs>'
        # A shallow recess: darker floor, shadow under the top and left lips, light on the far edges.
        '<rect x="3" y="3" width="378" height="506" rx="9" fill="#000" fill-opacity=".20"/>'
        '<g clip-path="url(#slot)"><path d="M-20 540 L-20 -20 L420 -20" fill="none" stroke="#000" stroke-opacity=".8" '
        'stroke-width="46" filter="url(#soft)"/>'
        '<path d="M404 -20 L404 532 L-20 532" fill="none" stroke="#fff" stroke-opacity=".10" stroke-width="30" '
        'filter="url(#soft)"/></g>'
        '<rect x="3" y="3" width="378" height="506" rx="9" fill="none" stroke="#000" stroke-opacity=".55" stroke-width="3"/>'
        '<rect x="6.5" y="6.5" width="371" height="499" rx="7" fill="none" stroke="#fff" stroke-opacity=".07" stroke-width="1.5"/>'
        '</svg>')


def render(svg, target):
    with tempfile.NamedTemporaryFile('w', suffix='.svg', delete=False) as handle:
        handle.write(svg)
    subprocess.run(['rsvg-convert', handle.name, '-o', str(target)], check=True)
    pathlib.Path(handle.name).unlink()


ART = ROOT / 'tools/board-art'
# The field sits behind cards, so bright materials are toned down: brightness, saturation.
TONE = {'felt': (72, 85), 'parchment': (84, 80), 'walnut': (88, 95), 'leather': (90, 95)}
# A rail in another material than its field.
RAIL = {'felt': 'walnut', 'parchment': 'leather'}


def painted(name, procedural):
    """A texture function drawing the painted source art, or None to use the procedural one."""
    source = ART / f'{name}.webp'
    if procedural or not source.is_file():
        return None
    brightness, saturation = TONE.get(name, (100, 100))

    def texture(w=W, h=H):
        # librsvg only loads files beside the SVG, so the art is embedded.
        jpeg = subprocess.check_output(['magick', str(source), '-resize', f'{w}x{h}^', '-gravity', 'center',
                                        '-extent', f'{w}x{h}', '-modulate', f'{brightness},{saturation}',
                                        '-quality', '92', 'jpg:-'])
        data = base64.b64encode(jpeg).decode()
        return '', f'<image width="{w}" height="{h}" href="data:image/jpeg;base64,{data}"/>'
    return texture


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--procedural', action='store_true', help='ignore tools/board-art and generate every material')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    render(CELL, OUT / 'cell.png')
    for name, (base, texture, rail, vignette, (rail_base, rail_texture)) in materials().items():
        folder = OUT / name
        folder.mkdir(exist_ok=True)
        texture = painted(name, args.procedural) or texture
        rail_texture = painted(RAIL.get(name, name), args.procedural) or rail_texture
        defs, body = texture()
        # The field is opaque, so it ships as WebP; the rail needs its alpha and stays PNG.
        with tempfile.TemporaryDirectory() as work:
            png = pathlib.Path(work) / 'surface.png'
            render(layers(base, defs, body, vignette), png)
            subprocess.run(['magick', str(png), '-quality', '90', str(folder / 'surface.webp')], check=True)
        render(frame_svg(rail_base, rail_texture, rail), folder / 'frame.png')
        print(name)


if __name__ == '__main__':
    main()
