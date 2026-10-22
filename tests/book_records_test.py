"""Check BOOK serialization against xEdit's Skyrim schema and repair old output."""
from pathlib import Path
import importlib.util
import struct
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from plugin_records import walk, parse_fields, build_record

spec = importlib.util.spec_from_file_location('build_plugin', root / 'tools/build-plugin.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

artifact = root / 'build/clean-catalog/runtime' if '--stage' in sys.argv else root
blob = (artifact / 'Tessera TCG.esp').read_bytes()
records, _ = walk(blob)
books = {}
for form, (start, end, _) in records.items():
    if blob[start:start + 4] != b'BOOK':
        continue
    header, fields = parse_fields(blob[start:end])
    tags = [tag for tag, _ in fields]
    values = dict(fields)
    editor = values[b'EDID'].rstrip(b'\0').decode('ascii')
    # xEdit Core/wbDefinitionsTES5.pas, wbRecord(BOOK): Book Text comes
    # before keywords; the DATA and inventory-art fields come after them.
    # https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsTES5.pas
    required = (b'DESC', b'KSIZ', b'KWDA', b'DATA', b'INAM')
    assert all(tags.count(tag) == 1 for tag in required), (editor, tags)
    positions = [tags.index(tag) for tag in required]
    assert positions == sorted(positions), (editor, 'BOOK subrecord order', tags)
    assert tags.index(b'MODL') < tags.index(b'DESC'), (editor, tags)
    assert tags.index(b'KWDA') == tags.index(b'KSIZ') + 1, (editor, tags)
    assert len(values[b'DATA']) == 16 and len(values[b'INAM']) == 4, editor
    assert len(values[b'KSIZ']) == 4, editor
    assert len(values[b'KWDA']) == 4 * struct.unpack('<I', values[b'KSIZ'])[0], editor
    assert values[b'DESC'].endswith(b'\0'), editor
    books[form] = (start, end, header, fields)

assert 0x02000870 in books
assert all(0x02000F80 + n in books for n in range(30))
generated, sequence = builder.generate(blob)
assert generated == blob, 'Plugin is not current or the generator is not idempotent'
assert sequence == (artifact / 'Seq/Tessera TCG.seq').read_bytes()

# Normalize book subrecord order while preserving values and unrelated records.
legacy = bytearray(blob)
for start, end, header, fields in books.values():
    keywords = [(tag, value) for tag, value in fields if tag in (b'KSIZ', b'KWDA')]
    fields = [(tag, value) for tag, value in fields if tag not in (b'KSIZ', b'KWDA')]
    index = next(i for i, (tag, _) in enumerate(fields) if tag == b'DESC')
    fields[index:index] = keywords
    invalid = build_record(header, fields)
    assert len(invalid) == end - start
    legacy[start:end] = invalid
assert legacy != blob, 'Regression fixture must contain the reported invalid order'
repaired, repaired_sequence = builder.generate(bytes(legacy))
assert repaired == blob, 'One build must repair the album and every cloned invitation'
assert repaired_sequence == sequence
assert builder.generate(repaired) == (repaired, sequence)
print(f'BOOK records: {len(books)} valid layouts; legacy repair preserves all values and is idempotent')
