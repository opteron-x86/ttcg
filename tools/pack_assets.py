"""The physical pack assets shared by plugin generation, installs and releases."""
import struct
from plugin_records import walk, parse_fields, build_record, apply

PACK_MODELS = {0x02000840: 'standard', 0x02000FD6: 'tournament'}
PACK_TEXTURES = ('standard', 'tournament', 'materials')
PACK_BOUNDS = (-5, -6, -1, 5, 6, 1)


def apply_pack_models(blob):
    """Change only the two owned pack model records; clear obsolete model hashes."""
    for form, name in PACK_MODELS.items():
        records, _ = walk(blob)
        start, end, parents = records[form]
        if blob[start:start + 4] != b'MISC':
            raise ValueError(f'Pack is not a MISC record: {form:08X}')
        header, fields = parse_fields(blob[start:end])
        fields = [(key, (f'Tessera\\packs\\{name}.nif\0'.encode('ascii') if key == b'MODL'
                        else struct.pack('<6h', *PACK_BOUNDS) if key == b'OBND' else value))
                  for key, value in fields if key not in (b'MODT', b'MODS')]
        blob = apply(blob, start, end, parents, build_record(header, fields))
    return blob


def pack_runtime_files(root):
    # Explicit ownership keeps authoring .blend/.png/JSON files out of both installers.
    names = [f'meshes/Tessera/packs/{name}.nif' for name in PACK_MODELS.values()]
    names += [f'textures/Tessera/packs/{name}{suffix}.dds'
              for name in PACK_TEXTURES for suffix in ('', '_n')]
    files = {name: root / name for name in names}
    for name, path in files.items():
        if not path.is_file():
            raise FileNotFoundError(f'Missing pack asset: {name}')
    return files
