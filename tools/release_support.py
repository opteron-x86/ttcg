"""Version, source and asset checks shared by release commands."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATHS = ('.github', 'src', 'include', 'native', 'cmake', 'tools', 'tests',
                'data', 'docs', 'examples', 'addons', 'Scripts', 'Seq', 'SKSE',
                'TesseraUI', 'Sound', 'Music', 'meshes', 'textures', '.gitignore',
                '.gitattributes', 'CMakeLists.txt', 'CMakePresets.json', 'vcpkg.json',
                'package.json', 'package-lock.json', 'README.md', 'BUILDING.md',
                'CHANGELOG.md', 'LICENSE', 'Tessera TCG.esp')


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def version(root=ROOT):
    value = json.loads((root / 'vcpkg.json').read_text(encoding='utf-8'))['version-semver']
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-(?:alpha|beta|rc)\.\d+)?', value):
        raise ValueError('Invalid version in vcpkg.json')
    return value


def package_version(release, revision, tag=None):
    if tag:
        if tag != 'v' + release:
            raise ValueError(f'Tag must match vcpkg.json: v{release}')
        return release
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        raise ValueError('Development packages require a Git commit ID')
    return f'{release}-dev.{revision[:12]}'


def source_state(root=ROOT):
    if (root / '.git').exists():
        def git(*args):
            return subprocess.check_output(['git', *args], cwd=root, text=True).strip()
        return {'commit': git('rev-parse', 'HEAD'),
                'dirty': bool(git('status', '--porcelain', '--untracked-files=all')),
                'timestamp': int(git('show', '-s', '--format=%ct', 'HEAD'))}
    return json.loads((root / 'SOURCE-INFO.json').read_text(encoding='utf-8'))


def private_asset(name):
    path = PurePosixPath(name.replace('\\', '/').lower())
    parts = path.parts
    return (any(parts[i:i+2] in {('art', 'cards'), ('art', 'thumbs'), ('art', 'expansions')}
                for i in range(len(parts) - 1)) or
            ('card-art-prompts' in parts and path.suffix in {'.png', '.jpg', '.jpeg', '.webp'}))


def check_public_files(names):
    for name in names:
        if private_asset(name):
            raise ValueError(f'Artwork belongs in the Nexus download: {name}')
        if '\\' in name or '..' in PurePosixPath(name).parts or PurePosixPath(name).is_absolute():
            raise ValueError(f'Unsafe archive path: {name}')


def source_files(root=ROOT):
    if (root / '.git').exists():
        raw = subprocess.check_output(['git', 'ls-files', '-z'], cwd=root)
        names = [n.decode('utf-8') for n in raw.split(b'\0') if n]
    else:
        names = [p.relative_to(root).as_posix() for top in SOURCE_PATHS
                 for p in ((root / top).rglob('*') if (root / top).is_dir() else [root / top])
                 if p.is_file() and not any(x in p.parts for x in ('__pycache__', 'bin', 'obj'))]
    check_public_files(names)
    files = {}
    for name in names:
        path = root / name
        if path.is_symlink():
            raise ValueError(f'Source archive requires regular files: {name}')
        if path.is_file() and path.suffix != '.pyc':
            files[name] = path
    return files


def verify_dependencies(root=ROOT):
    manifest = json.loads((root / 'native/third_party/manifest.json').read_text(encoding='utf-8'))
    count = 0
    for library, metadata in manifest.items():
        for name, expected in metadata['files'].items():
            if digest(root / 'native/third_party' / library / name) != expected:
                raise ValueError(f'Dependency differs: {library}/{name}')
            count += 1
    return count


def verify_runtime_assets(root=ROOT):
    manifest = json.loads((root / 'cmake/runtime-assets.json').read_text(encoding='utf-8'))
    for name, expected in manifest.items():
        if digest(root / name) != expected:
            raise ValueError(f'Update compiled assets and cmake/runtime-assets.json together: {name}')
    return len(manifest)


def release_notes(release, root=ROOT):
    text = (root / 'CHANGELOG.md').read_text(encoding='utf-8')
    marker = f'## {release}\n'
    if marker not in text:
        raise ValueError(f'Missing changelog entry for {release}')
    return text.split(marker, 1)[1].split('\n## ', 1)[0].strip() + '\n'
