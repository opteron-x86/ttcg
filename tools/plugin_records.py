"""Read and replace Tessera-owned, uncompressed Skyrim records."""
from pathlib import Path
from datetime import datetime, timezone
import struct
import json

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / 'Tessera TCG.esp'
def walk(blob):
    records, groups = {}, []
    def inner(start, end, ancestors):
        pos = start
        while pos < end:
            tag = blob[pos:pos + 4]
            size = struct.unpack_from('<I', blob, pos + 4)[0]
            if tag == b'GRUP':
                group_end = pos + size
                group_type = struct.unpack_from('<I', blob, pos + 12)[0]
                parent = struct.unpack_from('<I', blob, pos + 8)[0] if group_type == 7 else None
                groups.append((pos, group_end, parent))
                inner(pos + 24, group_end, ancestors + [pos])
                pos = group_end
                continue
            form = struct.unpack_from('<I', blob, pos + 12)[0]
            records[form] = (pos, pos + 24 + size, list(ancestors))
            pos += 24 + size
    inner(0, len(blob), [])
    return records, groups


def zstring(text):
    return text.encode('cp1252') + b'\0'


def field(tag, payload):
    return tag + struct.pack('<H', len(payload)) + payload


def parse_fields(record):
    fields, at = [], 24
    while at < len(record):
        key, size = record[at:at + 4], struct.unpack_from('<H', record, at + 4)[0]
        payload = record[at + 6:at + 6 + size]
        fields.append((key, payload))
        at += 6 + size
    return record[:24], fields


def build_record(header, fields):
    body = b''.join(field(k, p) for k, p in fields)
    header = bytearray(header)
    struct.pack_into('<I', header, 4, len(body))
    return bytes(header) + body


def ctda(op, value, func, param1, run_on=0, reference=0, param2=0):
    payload = bytes([op, 0, 0, 0]) + struct.pack('<f', float(value)) + struct.pack('<H', func) + b'\x00\x00'
    payload += struct.pack('<II', param1, param2) + struct.pack('<IIi', run_on, reference, -1)
    return payload


def shared_response(fields, info):
    """Reuse an INFO's spoken response, retaining this topic's actions and gates.

    DNAM shares response data only. The source INFO's scripts and conditions
    must not be cloned (a merchant's response may itself open barter).
    """
    fields = [(k, v) for k, v in fields if k not in
              (b'DNAM', b'TRDT', b'NAM1', b'NAM2', b'NAM3', b'SNAM', b'LNAM')]
    fields = [(k, struct.pack('<H', struct.unpack_from('<H', v)[0] & ~0x800) + v[2:]
               if k == b'ENAM' else v) for k, v in fields]
    # Canonical INFO order: ENAM, optional previous links, CNAM, TCLT, DNAM,
    # then conditions and prompt/speaker fields. Shared speech supplies LIP.
    trailing = (b'CTDA', b'CIS1', b'CIS2', b'RNAM', b'ANAM', b'TWAT', b'ONAM')
    if not any(k == b'CNAM' for k, _ in fields):
        at = next((i for i, (k, _) in enumerate(fields) if k in (b'TCLT',) + trailing), len(fields))
        fields.insert(at, (b'CNAM', b'\0'))
    at = next((i for i, (k, _) in enumerate(fields) if k in trailing), len(fields))
    fields.insert(at, (b'DNAM', struct.pack('<I', info)))
    return fields


def apply(blob, start, end, ancestors, new_record):
    out = bytearray(blob)
    delta = len(new_record) - (end - start)
    out[start:end] = new_record
    for group_at in ancestors:
        size = struct.unpack_from('<I', out, group_at + 4)[0]
        struct.pack_into('<I', out, group_at + 4, size + delta)
    return bytes(out)
