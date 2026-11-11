#!/usr/bin/env python3
"""Convert the vector shapes in a SWF to SVG files. Usage: swfshape.py in.swf outdir"""
import struct, sys, zlib, os, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


class Bits:
    def __init__(self, data, pos=0):
        self.d, self.p, self.b = data, pos, 0

    def align(self):
        if self.b:
            self.p += 1
            self.b = 0

    def ub(self, n):
        v = 0
        for _ in range(n):
            v = (v << 1) | ((self.d[self.p] >> (7 - self.b)) & 1)
            self.b += 1
            if self.b == 8:
                self.b = 0
                self.p += 1
        return v

    def sb(self, n):
        v = self.ub(n)
        return v - (1 << n) if n and v >> (n - 1) else v

    def fb(self, n):
        return self.sb(n) / 65536

    def u8(self):
        self.align()
        v = self.d[self.p]
        self.p += 1
        return v

    def u16(self):
        self.align()
        v, = struct.unpack_from('<H', self.d, self.p)
        self.p += 2
        return v

    def rect(self):
        self.align()
        n = self.ub(5)
        r = [self.sb(n) for _ in range(4)]
        self.align()
        return r  # xmin xmax ymin ymax

    def matrix(self):
        self.align()
        a = d = 1.0
        b = c = 0.0
        if self.ub(1):
            n = self.ub(5)
            a, d = self.fb(n), self.fb(n)
        if self.ub(1):
            n = self.ub(5)
            b, c = self.fb(n), self.fb(n)
        n = self.ub(5)
        tx, ty = self.sb(n), self.sb(n)
        self.align()
        return a, b, c, d, tx, ty

    def colour(self, alpha):
        self.align()
        r, g, b = self.d[self.p:self.p + 3]
        self.p += 3
        a = 255
        if alpha:
            a = self.d[self.p]
            self.p += 1
        return r, g, b, a


def fill_style(s, version):
    kind = s.u8()
    if kind == 0:
        return ('solid', s.colour(version >= 3))
    if kind in (0x10, 0x12, 0x13):
        m = s.matrix()
        s.align()
        head = s.u8()
        stops = [(s.u8(), s.colour(version >= 3)) for _ in range(head & 15)]
        if kind == 0x13:
            s.u16()
        return ('linear' if kind == 0x10 else 'radial', m, stops)
    if kind in (0x40, 0x41, 0x42, 0x43):
        return ('bitmap', s.u16(), s.matrix())
    raise ValueError(f'fill {kind:#x}')


def styles(s, version):
    n = s.u8()
    if n == 255 and version >= 2:
        n = s.u16()
    fills = [fill_style(s, version) for _ in range(n)]
    n = s.u8()
    if n == 255:
        n = s.u16()
    lines = []
    for _ in range(n):
        width = s.u16()
        if version == 4:
            s.align()
            s.ub(2)
            join = s.ub(2)
            has_fill = s.ub(1)
            s.ub(11)
            if join == 2:
                s.u16()
            colour = (255, 255, 255, 255)
            if has_fill:
                f = fill_style(s, version)
                if f[0] == 'solid':
                    colour = f[1]
            else:
                colour = s.colour(True)
        else:
            colour = s.colour(version >= 3)
        lines.append((width, colour))
    s.align()
    return fills, lines, s.ub(4), s.ub(4)


def rgba(c):
    return f'rgb({c[0]},{c[1]},{c[2]})', c[3] / 255


def shape_svg(body, version, scale=1.0):
    s = Bits(body)
    cid = s.u16()
    xmin, xmax, ymin, ymax = s.rect()
    if version == 4:
        s.rect()
        s.u8()
    fills, lines, fbits, lbits = styles(s, version)
    groups = []  # (fills, lines, {fill: [edges]}, {line: [edges]})
    fill_edges, line_edges = collections.defaultdict(list), collections.defaultdict(list)
    x = y = 0
    f0 = f1 = ln = 0
    while True:
        if not s.ub(1):
            flags = s.ub(5)
            if not flags:
                break
            if flags & 1:
                n = s.ub(5)
                x, y = s.sb(n), s.sb(n)
            if flags & 2:
                f0 = s.ub(fbits)
            if flags & 4:
                f1 = s.ub(fbits)
            if flags & 8:
                ln = s.ub(lbits)
            if flags & 16:
                groups.append((fills, lines, fill_edges, line_edges))
                fill_edges, line_edges = collections.defaultdict(list), collections.defaultdict(list)
                s.align()
                fills, lines, fbits, lbits = styles(s, version)
                f0 = f1 = ln = 0
            continue
        if s.ub(1):
            n = s.ub(4) + 2
            if s.ub(1):
                dx, dy = s.sb(n), s.sb(n)
            elif s.ub(1):
                dx, dy = 0, s.sb(n)
            else:
                dx, dy = s.sb(n), 0
            edge = ((x, y), None, (x + dx, y + dy))
        else:
            n = s.ub(4) + 2
            cx, cy = x + s.sb(n), y + s.sb(n)
            ax, ay = cx + s.sb(n), cy + s.sb(n)
            edge = ((x, y), (cx, cy), (ax, ay))
        x, y = edge[2]
        if f1:
            fill_edges[f1].append(edge)
        if f0:
            fill_edges[f0].append((edge[2], edge[1], edge[0]))
        if ln:
            line_edges[ln].append(edge)
    groups.append((fills, lines, fill_edges, line_edges))
    w, h = (xmax - xmin) / 20, (ymax - ymin) / 20
    pt = lambda p: f'{(p[0] - xmin) / 20:.2f} {(p[1] - ymin) / 20:.2f}'
    defs, paths = [], []
    for gi, (fills, lines, fe, le) in enumerate(groups):
        for index, edges in sorted(fe.items()):
            style = fills[index - 1]
            starts = collections.defaultdict(list)
            for e in edges:
                starts[e[0]].append(e)
            d = []
            remaining = len(edges)
            used = set()
            for e in edges:
                if id(e) in used:
                    continue
                d.append('M' + pt(e[0]))
                cur = e
                while True:
                    used.add(id(cur))
                    d.append(('L' + pt(cur[2])) if cur[1] is None else ('Q' + pt(cur[1]) + ' ' + pt(cur[2])))
                    nxt = next((c for c in starts[cur[2]] if id(c) not in used), None)
                    if nxt is None:
                        break
                    cur = nxt
                d.append('Z')
            if style[0] == 'solid':
                colour, opacity = rgba(style[1])
                paint = f'fill="{colour}" fill-opacity="{opacity:.3f}"'
            elif style[0] in ('linear', 'radial'):
                a, b, c, dd, tx, ty = style[1]
                gid = f'g{cid}_{gi}_{index}'
                stops = ''.join(
                    f'<stop offset="{r / 255:.4f}" stop-color="{rgba(col)[0]}" stop-opacity="{rgba(col)[1]:.3f}"/>'
                    for r, col in style[2])
                transform = f'matrix({a / 20:.6f} {b / 20:.6f} {c / 20:.6f} {dd / 20:.6f} {(tx - xmin) / 20:.3f} {(ty - ymin) / 20:.3f})'
                if style[0] == 'linear':
                    defs.append(f'<linearGradient id="{gid}" gradientUnits="userSpaceOnUse" x1="-16384" x2="16384" y1="0" y2="0" gradientTransform="{transform}">{stops}</linearGradient>')
                else:
                    defs.append(f'<radialGradient id="{gid}" gradientUnits="userSpaceOnUse" cx="0" cy="0" r="16384" gradientTransform="{transform}">{stops}</radialGradient>')
                paint = f'fill="url(#{gid})"'
            else:
                bid, (a, b, c, dd, tx, ty) = style[1], style[2]
                size = BITMAPS.get(bid)
                if size and bid != 65535:
                    gid = f'p{cid}_{gi}_{index}'
                    transform = f'matrix({a / 20:.6f} {b / 20:.6f} {c / 20:.6f} {dd / 20:.6f} {(tx - xmin) / 20:.3f} {(ty - ymin) / 20:.3f})'
                    defs.append(f'<pattern id="{gid}" patternUnits="userSpaceOnUse" width="{size[0]}" height="{size[1]}" patternTransform="{transform}"><image href="{bid}.png" width="{size[0]}" height="{size[1]}"/></pattern>')
                    paint = f'fill="url(#{gid})"'
                else:
                    paint = 'fill="none"'
            paths.append(f'<path fill-rule="evenodd" {paint} d="{"".join(d)}"/>')
        for index, edges in sorted(le.items()):
            width, col = lines[index - 1]
            colour, opacity = rgba(col)
            d, last = [], None
            for e in edges:
                if e[0] != last:
                    d.append('M' + pt(e[0]))
                d.append(('L' + pt(e[2])) if e[1] is None else ('Q' + pt(e[1]) + ' ' + pt(e[2])))
                last = e[2]
            paths.append(f'<path fill="none" stroke="{colour}" stroke-opacity="{opacity:.3f}" stroke-width="{max(width / 20, .05):.2f}" stroke-linecap="round" stroke-linejoin="round" d="{"".join(d)}"/>')
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w * scale:.2f}" height="{h * scale:.2f}" viewBox="0 0 {w:.2f} {h:.2f}">'
           f'<defs>{"".join(defs)}</defs>{"".join(paths)}</svg>')
    return cid, w, h, svg, sum(len(v) for g in groups for v in g[2].values()), sum(len(v) for g in groups for v in g[3].values())


BITMAPS = {}


def main(source, out, scale=1.0):
    import swf_bitmaps as swfbits, io, contextlib
    with contextlib.redirect_stdout(io.StringIO()):
        found = swfbits.main(source, out)
    BITMAPS.clear()
    for cid, w, h, kind in found:
        if w:
            BITMAPS[cid] = (w, h)
    data = open(source, 'rb').read()
    if data[:3] == b'CWS':
        data = data[:8] + zlib.decompress(data[8:])
    bits = data[8] >> 3
    pos = 8 + (5 + bits * 4 + 7) // 8 + 4
    os.makedirs(out, exist_ok=True)
    result = []
    while pos < len(data):
        head, = struct.unpack_from('<H', data, pos)
        pos += 2
        code, length = head >> 6, head & 63
        if length == 63:
            length, = struct.unpack_from('<I', data, pos)
            pos += 4
        body = data[pos:pos + length]
        pos += length
        version = {2: 1, 22: 2, 32: 3, 83: 4}.get(code)
        if version:
            try:
                cid, w, h, svg, nf, nl = shape_svg(body, version, scale)
            except Exception as e:
                print('shape failed', code, e)
                continue
            open(os.path.join(out, f'shape{cid}.svg'), 'w').write(svg)
            result.append((cid, w, h, nf, nl))
            print(f'shape {cid}: {w:.0f}x{h:.0f} fills={nf} lines={nl}')
    return result


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else 1.0)
