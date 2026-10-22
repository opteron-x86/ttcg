"""Tournament packs use an ordinary inventory record, outside vendor loot rolls."""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from plugin_records import PLUGIN, walk, parse_fields
from pack_assets import PACK_BOUNDS

blob = PLUGIN.read_bytes()
records, _ = walk(blob)

def fields(form):
    start, end, _ = records[form]
    return blob[start:start + 4], dict(parse_fields(blob[start:end])[1])

tag, standard = fields(0x02000840)
reward_tag, reward = fields(0x02000FD6)
assert tag == reward_tag == b'MISC'
assert reward[b'EDID'] == b'TTCG_TournamentPack\0'
assert reward[b'FULL'] == b'Tessera Tournament Pack\0'
assert {k: v for k, v in reward.items() if k not in (b'EDID', b'FULL', b'MODL')} == {
    k: v for k, v in standard.items() if k not in (b'EDID', b'FULL', b'MODL')}
assert standard[b'MODL'] == b'Tessera\\packs\\standard.nif\0'
assert reward[b'MODL'] == b'Tessera\\packs\\tournament.nif\0'
assert struct.unpack('<6h', standard[b'OBND']) == PACK_BOUNDS
assert b'MODT' not in standard and b'MODT' not in reward
assert struct.unpack_from('<I', fields(0)[1][b'HEDR'], 8)[0] > 0xFD6
for start, end, _ in records.values():
    if blob[start:start + 4] == b'LVLI':
        for key, data in parse_fields(blob[start:end])[1]:
            if key == b'LVLO':
                assert struct.unpack_from('<I', data, 4)[0] != 0x02000FD6
print('Tournament pack: distinct sealed-pack models, shared weight/value/vendor keywords, unique form and no vendor/loot injection passed')
