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
               'Jost-Regular', 'Lato-Regular')]
    names += [f'TesseraUI/icons/affinity-{i}.png' for i in range(5)]
    names += [f'TesseraUI/assets/themes/{surface}.png' for surface in
              ('fine', 'slate', 'paper', 'leather', 'wood')]
    names += [f'TesseraUI/assets/packs/{pack}.png' for pack in ('standard', 'tournament')]
    files = {name: root / name for name in names}
    for name, path in files.items():
        if not path.is_file():
            raise FileNotFoundError(f'Missing native UI resource: {name}')
    return files


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
    notices = root / 'docs/licenses'
    for path in notices.rglob('*'):
        if path.is_file() and path.suffix in ('.txt', '.md') and not path.name.startswith('.'):
            main['licenses/Tessera/' + path.relative_to(notices).as_posix()] = path
    if not voices or not any(n.startswith('Music/') for n in main) or not any(n.startswith('licenses/') for n in main):
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
