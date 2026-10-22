"""Keep retired form identities resolvable without making their dialogue playable."""
import json
import struct
from plugin_records import ROOT, walk, parse_fields, build_record, apply, ctda


def disable_retired_approaches(blob):
    retired = json.loads((ROOT / 'data/free-play-record-layout.json').read_text())['retiredForms']
    records, _ = walk(blob)
    for local in retired:
        if 0x02000000 | local not in records:
            raise ValueError(f'Missing legacy compatibility form {local:06X}; do not delete or reuse it')
    for form, (start, end, parents) in sorted(records.items(), key=lambda row: row[1][0], reverse=True):
        if form >> 24 != 2 or form & 0xFFFFFF not in retired or blob[start:start+4] != b'INFO':
            continue
        header, fields = parse_fields(blob[start:end])
        # GetIsID returns 0 or 1. Comparing it with -1 is always false, even if
        # an old save restores a nonzero invitation global. No polling needed.
        fields = [(key, b'\0' if key == b'NAM1' else value) for key, value in fields
                  if key not in (b'CTDA', b'CIS1', b'CIS2', b'TCLT')]
        fields.append((b'CTDA', ctda(0, -1, 72, 0x7)))
        blob = apply(blob, start, end, parents, build_record(header, fields))
    return blob
