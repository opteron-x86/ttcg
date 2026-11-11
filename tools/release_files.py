"""Shared runtime selection for native releases and local installs."""
from pathlib import PurePosixPath
import json
from runtime_files import ROOT, validate_runtime_inputs
from pack_assets import pack_runtime_files


def safe_relative(name):
    path = PurePosixPath(name)
    if not name or path.is_absolute() or '..' in path.parts or '\\' in name:
        raise ValueError(f'Unsafe release path: {name}')
    return path.as_posix()


def ui_files(root):
    """Only resources consumed by the compiled native frontend."""
    names = ['TesseraUI/rules.json']
    names += [f'TesseraUI/fonts/{font}.ttf' for font in
              ('LiberationSerif-Regular', 'LiberationSerif-Bold', 'LiberationSans-Regular',
               'Jost-Regular', 'Jost-Bold', 'Lato-Regular')]
    names += [f'TesseraUI/icons/affinity-{i}.png' for i in range(5)]
    names += ['TesseraUI/assets/boards/cell.png']
    names += [f'TesseraUI/assets/card/frame-{level}.png' for level in (1, 2, 4, 8)]
    for board in ('slate', 'felt', 'parchment', 'leather', 'walnut', 'slab'):
        names += [f'TesseraUI/assets/boards/{board}/surface.webp', f'TesseraUI/assets/boards/{board}/frame.png']
    names += [f'TesseraUI/assets/packs/{pack}.png' for pack in ('standard', 'tournament')]
    files = {name: root / name for name in names}
    for name, path in files.items():
        if not path.is_file():
            raise FileNotFoundError(f'Missing native UI resource: {name}')
    # Each skin ships its definition and the textures beside it.
    for path in sorted((root / 'TesseraUI/skins').glob('*/*')):
        if path.is_file() and path.suffix in ('.json', '.png'):
            files[path.relative_to(root).as_posix()] = path
    return files


NOTICES = 'licenses/Tessera/THIRD-PARTY-NOTICES.txt'


def write_notices(root):
    """One file holding every licence the package must carry; GitHub keeps them separately in docs/licenses."""
    notices = root / 'docs/licenses'
    parts = [('Tessera Trading Card Game', root / 'LICENSE')]
    parts += [(path.relative_to(notices).as_posix(), path) for path in sorted(notices.rglob('*'))
              if path.is_file() and path.suffix in ('.txt', '.md') and not path.name.startswith('.')]
    if len(parts) < 2:
        raise ValueError('Release requires its third-party notices')
    rule = '=' * 78
    text = ('Licences and notices for the software, fonts and artwork in this package.\n'
            'Source and the individual files: https://github.com/opteron-x86/ttcg (docs/licenses).\n')
    for title, path in parts:
        text += f'\n{rule}\n{title}\n{rule}\n\n' + path.read_text(encoding='utf-8').strip() + '\n'
    out = root / 'build/notices/THIRD-PARTY-NOTICES.txt'
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.is_file() or out.read_text(encoding='utf-8') != text:
        out.write_text(text, encoding='utf-8', newline='\n')
    return out


def collect_release(root=ROOT, *, artwork=True, dll=None):
    root = root.resolve()
    cards = validate_runtime_inputs(root, artwork=artwork)
    main = ui_files(root)
    main.update(pack_runtime_files(root))
    for name in ['Tessera TCG.esp', 'Seq/Tessera TCG.seq']:
        main[name] = root / name
    main['SKSE/Plugins/TTCG.dll'] = dll or root / 'build/runtime-tessera/SKSE/Plugins/TTCG.dll'
    for script in (root / 'Scripts/Source').glob('*.psc'):
        name = f'Scripts/{script.stem}.pex'
        main[name] = root / name
    for back in ('mosaic', 'dragon', 'celestial'):
        name = f'TesseraUI/art/backs/{back}.webp'
        main[name] = root / name
    # Dialogue payloads are listed in the voice manifest.
    voices = json.loads((root / 'data/voice-manifest.json').read_text())
    for voice in voices:
        name = safe_relative(voice['destination'])
        extension = '.wav' if voice['kind'] == 'provisional' else '.fuz'
        if not name.startswith('Sound/Voice/Tessera TCG.esp/') or not name.endswith(extension):
            raise ValueError(f'Unexpected voice payload: {name}')
        main[name] = root / name
    for sound in ('ResultWin', 'ResultDraw', 'ResultLose', 'RuleSame', 'RulePlus',
                  'RuleCombo', 'RuleLegion', 'RuleDecimation', 'OpeningPlayer', 'OpeningOpponent'):
        name = f'Sound/SFX/TTCG_{sound}.wav'
        main[name] = root / name
    # Default music track.
    music = 'Music/TTCG/RPG Medieval Animated Music.xwm'
    main[music] = root / music
    main[NOTICES] = write_notices(root)
    if not voices or not any(n.startswith('Music/') for n in main):
        raise ValueError('Release requires its voice, music and third-party notices')
    # Nexus card-art download.
    art = {}
    for card in cards if artwork else ():
        for folder, extension in (('cards', 'webp'), ('thumbs', 'webp')):
            name = safe_relative(f'TesseraUI/art/{folder}/{card["name"]}.{extension}')
            art[name] = root / name
    if main.keys() & art.keys():
        raise ValueError('Runtime and card-art selections overlap')
    files = main | art
    for name, path in files.items():
        if not path.is_file():
            raise FileNotFoundError(f'Missing release file: {name}')
    return dict(sorted(main.items())), dict(sorted(art.items()))
