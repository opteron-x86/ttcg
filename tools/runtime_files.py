"""Select and validate the files shared by local installs and release archives."""
from pathlib import Path
import hashlib
import json
import struct
from pack_assets import pack_runtime_files

ROOT = Path(__file__).resolve().parents[1]



def validate_runtime_art(root, cards):
    # Match the formats accepted by CardArt.h. Thumbnails may fall back to faces.
    supported = {'.png', '.jpg', '.jpeg', '.webp'}
    def names(folder):
        return {p.stem.lower() for p in (root / 'TesseraUI/art' / folder).glob('*')
                if p.is_file() and p.suffix.lower() in supported}
    faces = names('cards')
    for card in cards:
        if card['name'].lower() not in faces:
            raise FileNotFoundError(f'Missing supported card artwork: {card["name"]}')
    if 'mosaic' not in names('backs'):
        raise FileNotFoundError('Missing supported card back: mosaic')


def validate_runtime_inputs(root=ROOT, *, artwork=True):
    cards = json.loads((root / 'data/cards.json').read_text())['cards']
    plugin = (root / 'Tessera TCG.esp').read_bytes()
    if not struct.unpack_from('<I', plugin, 8)[0] & 0x200:
        raise ValueError('Tessera TCG.esp must be ESL flagged')
    from plugin_records import walk, parse_fields
    records,_=walk(plugin)
    quests=[]
    for form,(start,end,_) in records.items():
        if plugin[start:start+4]==b'QUST' and struct.unpack_from('<H',dict(parse_fields(plugin[start:end])[1])[b'DNAM'])[0]&1:quests.append(form)
    if (root/'Seq/Tessera TCG.seq').read_bytes()!=struct.pack('<'+'I'*len(quests),*sorted(quests)):
        raise ValueError('SEQ does not match start-enabled plugin quests')
    scripts={p.stem+'.pex' for p in (root/'Scripts/Source').glob('*.psc')}
    if not scripts or {p.name for p in (root/'Scripts').glob('*.pex')}!=scripts:
        raise ValueError('Compiled Papyrus files do not match script sources')
    for name in scripts:
        if (root / 'Scripts' / name).read_bytes()[:4] != bytes.fromhex('fa57c0de'):
            raise ValueError(f'Invalid compiled script: {name}')
    if artwork:
        validate_runtime_art(root, cards)
    return cards


def collect_runtime(root=ROOT):
    # Select the complete local runtime, including installed artwork.
    from release_files import collect_release
    main, art = collect_release(root)
    return dict(sorted((main | art).items()))


def hashes(files):
    return {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in files.items()}
