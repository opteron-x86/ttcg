"""One AND gate for every Tessera dialogue response, including shared speech."""
import struct
from plugin_records import walk, parse_fields, build_record, apply, ctda, zstring

GAME_ENABLED = 0x02000FDA


def is_enable_gate(key, payload):
    return (key == b'CTDA' and struct.unpack_from('<H', payload, 8)[0] == 74
            and struct.unpack_from('<I', payload, 12)[0] == GAME_ENABLED)


def generate_game_settings(blob):
    records, _ = walk(blob)
    if GAME_ENABLED not in records:
        start, end, parents = records[0x02000850]
        header, _ = parse_fields(blob[start:end])
        header = bytearray(header)
        struct.pack_into('<II', header, 8, 0, GAME_ENABLED)
        record = build_record(header, [(b'EDID', zstring('TTCG_GamesEnabled')),
                                      (b'FNAM', b's'), (b'FLTV', struct.pack('<f', 1))])
        group = parents[0]
        end = group + struct.unpack_from('<I', blob, group + 4)[0]
        blob = apply(blob, end, end, [group], record)
        records, _ = walk(blob)
        start, end, parents = records[0]
        header, fields = parse_fields(blob[start:end])
        fields = [(k, v[:4] + struct.pack('<II', struct.unpack_from('<I', v, 4)[0] + 1,
                                         max(struct.unpack_from('<I', v, 8)[0], 0xFDB))
                   if k == b'HEDR' else v) for k, v in fields]
        blob = apply(blob, start, end, parents, build_record(header, fields))
        records, _ = walk(blob)
    else:
        start, end, _ = records[GAME_ENABLED]
        assert blob[start:start+4] == b'GLOB'
        assert dict(parse_fields(blob[start:end])[1])[b'EDID'] == zstring('TTCG_GamesEnabled')
    for form, (start, end, parents) in sorted(records.items(), key=lambda entry: entry[1][0], reverse=True):
        if form >> 24 != 2 or blob[start:start+4] != b'INFO':
            continue
        header, fields = parse_fields(blob[start:end])
        fields = [(k, v) for k, v in fields if not is_enable_gate(k, v)]
        # A leading AND keeps the existing faction/host OR groups intact.
        at = next((i for i, (k, _) in enumerate(fields) if k in
                   (b'CTDA', b'RNAM', b'ANAM', b'TWAT', b'ONAM')), len(fields))
        fields.insert(at, (b'CTDA', ctda(0, 1, 74, GAME_ENABLED)))
        blob = apply(blob, start, end, parents, build_record(header, fields))
    return blob
