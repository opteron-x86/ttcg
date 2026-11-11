#!/usr/bin/env python3
"""Extract bitmap characters from a SWF into PNG/JPEG files. Usage: swfbits.py in.swf outdir"""
import struct, sys, zlib, os, lzma


def png(path, w, h, rgba):
    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xffffffff)
    rows = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
                chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))


def unpremultiply(argb):
    out = bytearray(len(argb))
    for i in range(0, len(argb), 4):
        a, r, g, b = argb[i], argb[i + 1], argb[i + 2], argb[i + 3]
        if a and a < 255:
            r, g, b = min(255, r * 255 // a), min(255, g * 255 // a), min(255, b * 255 // a)
        out[i], out[i + 1], out[i + 2], out[i + 3] = r, g, b, a
    return bytes(out)


def main(source, out):
    data = open(source, 'rb').read()
    sig = data[:3]
    if sig == b'CWS':
        data = data[:8] + zlib.decompress(data[8:])
    elif sig == b'ZWS':
        data = data[:8] + lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=[lzma._decode_filter_properties(lzma.FILTER_LZMA1, data[12:17])]).decompress(data[17:])
    bits = data[8] >> 3
    pos = 8 + (5 + bits * 4 + 7) // 8 + 4
    os.makedirs(out, exist_ok=True)
    names, found = {}, []
    while pos < len(data):
        head, = struct.unpack_from('<H', data, pos)
        pos += 2
        code, length = head >> 6, head & 63
        if length == 63:
            length, = struct.unpack_from('<I', data, pos)
            pos += 4
        body = data[pos:pos + length]
        pos += length
        if code in (20, 36):
            cid, fmt, w, h = struct.unpack_from('<HBHH', body)
            if fmt == 5:
                raw = zlib.decompress(body[7:])
                rgba = unpremultiply(raw) if code == 36 else b''.join(raw[i + 1:i + 4] + b'\xff' for i in range(0, len(raw), 4))
                png(os.path.join(out, f'{cid}.png'), w, h, rgba)
                found.append((cid, w, h, 'lossless'))
            elif fmt == 3:
                count = body[7] + 1
                raw = zlib.decompress(body[8:])
                size = 4 if code == 36 else 3
                table = [raw[i * size:(i + 1) * size] for i in range(count)]
                stride = (w + 3) & ~3
                px = raw[count * size:]
                rgba = bytearray()
                for y in range(h):
                    for x in range(w):
                        c = table[px[y * stride + x]] if px[y * stride + x] < count else b'\0\0\0\0'
                        rgba += (c + b'\xff') if size == 3 else c
                if code == 36:  # palette entries are premultiplied RGBA
                    rgba = unpremultiply(b''.join(bytes(rgba[i + 3:i + 4] + rgba[i:i + 3]) for i in range(0, len(rgba), 4)))
                png(os.path.join(out, f'{cid}.png'), w, h, bytes(rgba))
                found.append((cid, w, h, 'palette'))
        elif code in (21, 35, 6, 90):
            cid, = struct.unpack_from('<H', body)
            if code in (35, 90):
                size, = struct.unpack_from('<I', body, 2)
                start = 6 + (2 if code == 90 else 0)
                image, alpha = body[start:start + size], body[start + size:]
            else:
                image, alpha = body[2:], b''
            if image[:8] == b'\x89PNG\r\n\x1a\n':
                open(os.path.join(out, f'{cid}.png'), 'wb').write(image)
            else:
                image = image.replace(b'\xff\xd9\xff\xd8', b'')
                open(os.path.join(out, f'{cid}.jpg'), 'wb').write(image)
                if alpha:
                    open(os.path.join(out, f'{cid}.alpha'), 'wb').write(zlib.decompress(alpha))
            found.append((cid, 0, 0, 'jpeg+alpha' if alpha else 'jpeg'))
        elif code == 56:  # ExportAssets
            n, = struct.unpack_from('<H', body)
            p = 2
            for _ in range(n):
                cid, = struct.unpack_from('<H', body, p)
                end = body.index(b'\0', p + 2)
                names[cid] = body[p + 2:end].decode('latin1')
                p = end + 1
        elif code == 76:  # SymbolClass
            n, = struct.unpack_from('<H', body)
            p = 2
            for _ in range(n):
                cid, = struct.unpack_from('<H', body, p)
                end = body.index(b'\0', p + 2)
                names[cid] = body[p + 2:end].decode('latin1')
                p = end + 1
    for cid, w, h, kind in found:
        print(cid, w, h, kind, names.get(cid, ''))
    return found


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
