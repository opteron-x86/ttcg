"""Plan and apply roster-workshop edits without inferring card retirements."""
from contextlib import contextmanager
import copy
from datetime import datetime, timezone
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import unicodedata
import uuid

from PIL import Image

from card_art import ART_FORMATS, render_art

from card_catalog import card_outputs, creature_groups_output, name_key, safe_name, validate_catalog
from card_rank_policy import POLICY, valid_profile

ROOT = Path(__file__).resolve().parents[1]
ART = Path('TesseraUI/art')
STATE = '.tessera-workshop/import-state.json'
HISTORY = '.tessera-workshop/import-backups'
EXTENSIONS = ART_FORMATS
FIELDS = ('name', 'tier', 'rarity', 'sides', 'groups', 'affinities', 'unique')


def tool_module(filename):
    spec = importlib.util.spec_from_file_location(filename.replace('-', '_'), ROOT / 'tools' / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def json_bytes(doc):
    return (json.dumps(doc, indent=2, ensure_ascii=False, allow_nan=False) + '\n').encode()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_hash(path):
    if not path.exists():
        return None
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def internal(root, relative):
    """All writes stay in this checkout; reject linked files and parent directories."""
    relative = Path(relative)
    if relative.is_absolute() or '..' in relative.parts:
        raise ValueError(f'Invalid destination: {relative}')
    path = root / relative
    if any(p.is_symlink() for p in (path, *path.parents) if p != root and p.is_relative_to(root)):
        raise ValueError(f'Linked destination: {relative}')
    if not path.resolve().is_relative_to(root):
        raise ValueError(f'Destination outside checkout: {relative}')
    return path


def atomic_bytes(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.writing-', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary and temporary.exists():
            temporary.unlink()


def image_key(text):
    text = unicodedata.normalize('NFC', text).replace('’', "'").replace('_', ' ')
    return re.sub(r'\s+', ' ', text).strip().casefold()


def source_images(folders):
    result = {}
    for folder in folders:
        folder = Path(folder).expanduser().resolve()
        if not folder.is_dir():
            raise ValueError(f'Artwork folder does not exist: {folder}')
        for base, dirs, files in os.walk(folder):
            dirs[:] = sorted(d for d in dirs if not d.startswith('.') and not (Path(base) / d).is_symlink())
            for filename in sorted(files):
                p = Path(base) / filename
                if p.suffix.lower() in EXTENSIONS and not p.is_symlink():
                    result.setdefault(image_key(p.stem), set()).add(p)
    return result


def runtime_images(root):
    result = {}
    for folder in ('cards', 'thumbs'):
        result[folder] = {}
        for p in sorted((root / ART / folder).glob('*')):
            if p.is_file() and p.suffix.lower() in EXTENSIONS:
                result[folder].setdefault(name_key(p.stem), []).append(p)
        for variants in result[folder].values():
            variants.sort(key=lambda p: (EXTENSIONS.index(p.suffix.lower()), p.name))
    return result


def group_document(doc, data):
    """Reuse the server's full-draft validator for an individual group export."""
    if not isinstance(doc, dict) or doc.get('kind') != 'tessera-catalog-template':
        return doc
    groups = []
    for template in data['templates']:
        if template['id'] == doc.get('template', {}).get('id'):
            groups.append(doc)
        else:
            groups.append({'kind': 'tessera-catalog-template', 'schema': 2,
                           'catalogFingerprint': data['fingerprint'], 'sideOrder': data['policy']['side_order'],
                           'template': {k: template[k] for k in ('id', 'revision', 'group', 'label')},
                           'slots': []})
    return {'kind': 'tessera-roster-workshop', 'schema': 2, 'catalogFingerprint': doc['catalogFingerprint'],
            'sideOrder': doc['sideOrder'], 'currentGroup': doc['template']['id'], 'groups': groups}


class Plan:
    def __init__(self, root, draft, *, groups=(), art_sources=(), art_map=None,
                 accept_unconfirmed=False, allow_incomplete=False, placeholder_missing_art=False):
        self.root = Path(root).resolve()
        self.inputs, self.images, self.errors, self.target_variants = {}, [], [], {}
        self.placeholder_missing_art = placeholder_missing_art
        self.placeholder_hash = None
        self.draft_path = Path(draft).expanduser().resolve()
        self.draft_bytes = self.read(self.draft_path)
        raw = json.loads(self.draft_bytes)
        # Use the same embedded template revisions and validator as the LAN server.
        page = self.root / 'docs/catalog-template-builder.html'
        self.read(page)
        self.workshop = tool_module('serve-roster-workshop.py').Catalog(self.root)
        self.verify_inputs()
        data = self.workshop.data
        self.doc = self.workshop.validate(group_document(raw, data))
        catalog = json.loads(self.read(self.root / 'data/cards.json'))
        registry = json.loads(self.read(self.root / 'data/released-card-ids.json'))
        validate_catalog(catalog, registry)
        self.catalog, self.registry = copy.deepcopy(catalog), dict(registry)
        self.ledger = json.loads(self.read(self.root / STATE)) if (self.root / STATE).exists() else {}
        if not self.ledger:
            self.inputs[str(self.root / STATE)] = None
        sources = {p: digest(self.read(self.root / p)) for p in data['sources']}
        if json.loads(self.read(self.root / 'data/rank-policy.json')) != POLICY:
            raise ValueError('Run the importer from the checkout containing this rank policy.')
        fingerprint = self.doc['catalogFingerprint']
        continuation = (self.ledger.get('schema') == 1 and fingerprint in self.ledger.get('acceptedFingerprints', [])
                        and sources == self.ledger.get('sources')
                        and digest(json_bytes(registry)) == self.ledger.get('registryHash'))
        if sources != data['sources'] and not continuation:
            drift = [p for p in sources if sources[p] != data['sources'][p]]
            raise ValueError('Catalog changed outside this import sequence: ' + ', '.join(drift)
                             + '. Review/rebase the workshop against a rebuilt catalog; stale edits were not applied.')
        self.sources = sources
        templates = {t['id']: t for t in data['templates']}
        selected = set()
        for requested in groups:
            matches = [t['id'] for t in templates.values()
                       if requested.casefold() in (t['id'].casefold(), t['group'].casefold(), t['slug'].casefold())]
            if len(matches) != 1:
                raise ValueError(f'Unknown group: {requested}')
            selected.add(matches[0])
        if not groups:
            selected = {g['template']['id'] for g in self.doc['groups'] if any(s['assignment'] for s in g['slots'])}
        if not selected:
            raise ValueError('The selected workshop has no assigned cards.')
        self.report = {'schema': 1, 'draft': str(self.draft_path), 'draftHash': digest(self.draft_bytes),
                       'sharedSave': raw.get('sharedSave'), 'catalogFingerprint': fingerprint,
                       'groups': [], 'skippedGroups': [], 'changes': [], 'errors': self.errors,
                       'unplacedPolicy': 'keep', 'unconfirmedAccepted': accept_unconfirmed,
                       'incompleteAccepted': allow_incomplete, 'placeholdersAccepted': placeholder_missing_art}
        originals = {c['id']: c for c in catalog['cards']}
        positions = {c['id']: i for i, c in enumerate(self.catalog['cards'])}
        faces = runtime_images(self.root)
        sources_by_name = source_images(art_sources)
        mappings = self.read_map(art_map)
        used_mappings, assigned, assigned_anywhere = set(), set(), set()
        all_new = sorted(a['cardId'] for g in self.doc['groups'] for s in g['slots']
                         if (a := s['assignment']) and a.get('kind') == 'new')
        next_form = max(0x400000, max(registry.values()) + 1)
        allocated = dict(registry)
        # Allocate in identity order, independent of workshop row/group ordering.
        for ident in all_new:
            ident = self.card_id(ident)
            if ident not in allocated:
                allocated[ident] = next_form
                next_form += 1
        for g in self.doc['groups']:
            t = templates[g['template']['id']]
            for s in g['slots']:
                if s['assignment']:
                    assigned_anywhere.add(self.card_id(s['assignment']['cardId']))
            if t['id'] not in selected:
                self.report['skippedGroups'].append(t['group'])
                continue
            filled = [s for s in g['slots'] if s['assignment']]
            empty = len(g['slots']) - len(filled)
            unconfirmed = sum(not s['confirmed'] for s in filled)
            self.report['groups'].append({'name': t['group'], 'assigned': len(filled), 'empty': empty,
                                          'unconfirmed': unconfirmed})
            if not filled or (empty and not allow_incomplete):
                self.errors.append(f"{t['group']}: {empty} empty slots. Finish the group or use --allow-incomplete.")
            if unconfirmed and not accept_unconfirmed:
                self.errors.append(f"{t['group']}: {unconfirmed} unconfirmed cards. Confirm them or use --accept-unconfirmed.")
            for slot in filled:
                a = slot['assignment']
                ident = self.card_id(a['cardId'])
                before = originals.get(ident)
                if not before and a.get('kind') != 'new':
                    raise ValueError(f'Missing existing identity: {ident}')
                after = copy.deepcopy(before) if before else {
                    'id': ident, 'form_id': allocated[ident], 'focus': '50% 40%', 'subtypes': [],
                    'set': 'Tessera', 'source_id': ident, 'source_name': a['name'], 'source_revision': '',
                    'acquisition': {'starter': False, 'available': not slot['unique'], 'special': slot['unique'],
                                    'opponents': [], 'route': 'Rewards and tournaments' if slot['unique'] else 'Standard play'}}
                after.update({k: copy.deepcopy(slot[k]) for k in FIELDS if k not in ('name', 'groups')})
                after.update(name=a['name'].strip(), groups=[t['group']])
                if after.get('roster_status') == 'pending':
                    after['roster_status'] = 'ready'
                    after['acquisition'].update(starter=False, available=not slot['unique'],
                                                special=slot['unique'], opponents=[],
                                                route='Tournament trophies' if slot['unique'] else 'Standard play')
                if (slot['rarity'] not in t['allowedRarities'][str(slot['tier'])]
                        or slot['unique'] != (slot['rarity'] == 'Legendary')):
                    self.errors.append(f"{after['name']}: rarity/uniqueness does not fit its workshop tier.")
                if not valid_profile(slot['sides'], slot['tier'], slot['rarity']):
                    self.errors.append(f"{after['name']}: invalid side values for tier {slot['tier']}.")
                if before and any(before[k] != after[k] for k in ('sides', 'tier')):
                    after.pop('profile_role', None)
                if not safe_name(after['name']):
                    self.errors.append(f"Unsafe card filename: {after['name']}")
                    art = {'status': 'invalid-name', 'source': None}
                else:
                    keys = {a['cardId'], ident, after['name']}.intersection(mappings)
                    if len(keys) > 1:
                        raise ValueError(f"Multiple artwork mappings for {after['name']}")
                    used_mappings.update(keys)
                    mapped = mappings[next(iter(keys))] if keys else None
                    art = self.resolve_art(before, after, mapped, faces, sources_by_name)
                changes = {k: {'before': before.get(k) if before else None, 'after': after.get(k)}
                           for k in FIELDS if not before or before.get(k) != after.get(k)}
                self.report['changes'].append({'cardId': a['cardId'], 'id': ident, 'form': after['form_id'],
                                               'slot': slot['id'], 'group': t['group'], 'name': after['name'],
                                               'new': before is None, 'fields': changes, 'art': art,
                                               'acquisition': after['acquisition']})
                if before:
                    self.catalog['cards'][positions[ident]] = after
                else:
                    self.catalog['cards'].append(after)
                    self.registry[ident] = after['form_id']
                assigned.add(ident)
        unused = set(mappings) - used_mappings
        if unused:
            raise ValueError('Artwork map contains keys outside this import: ' + ', '.join(sorted(unused)))
        self.report['unplaced'] = [{'id': c['id'], 'name': c['name'], 'groups': c['groups']}
                                   for c in catalog['cards'] if c['id'] not in assigned_anywhere]
        self.report['preserved'] = len(catalog['cards']) - len(assigned.intersection(originals))
        try:
            validate_catalog(self.catalog, self.registry)
        except ValueError as exc:
            self.errors.append(str(exc))
        self.report['summary'] = {'selected': len(assigned), 'new': sum(c['new'] for c in self.report['changes']),
                                  'updated': sum(bool(c['fields']) and not c['new'] for c in self.report['changes']),
                                  'artWrites': len(self.images), 'catalogSize': len(self.catalog['cards']),
                                  'placeholders': sum(c['art']['status'] == 'placeholder' for c in self.report['changes'])}
        self.report['integration'] = self.integration_review()
        if not self.errors:
            destinations = set(self.metadata())
            for art in self.images:
                destinations.update((art['original'], art['face'], art['thumbnail'], *art['replacedVariants']))
                self.target_variants[name_key(Path(art['face']).stem)] = sorted(art['replacedVariants'])
            for relative in destinations:
                path = internal(self.root, relative)
                expected = file_hash(path)
                if str(path) in self.inputs and self.inputs[str(path)] != expected:
                    raise ValueError(f'Destination changed during planning: {relative}')
                self.inputs[str(path)] = expected

    @staticmethod
    def card_id(ident):
        return 'workshop-' + ident[4:] if ident.startswith('new-') else ident

    def read(self, path):
        data = path.read_bytes()
        key, hashed = str(path), digest(data)
        if key in self.inputs and self.inputs[key] != hashed:
            raise ValueError(f'Input changed during planning: {path}')
        self.inputs[key] = hashed
        return data

    def verify_inputs(self):
        for path, expected in self.inputs.items():
            if file_hash(Path(path)) != expected:
                raise ValueError(f'Input changed during import: {path}. Run a fresh preview.')
        if self.target_variants:
            images = runtime_images(self.root)
            for key, expected in self.target_variants.items():
                actual = sorted(str(p.relative_to(self.root)) for folder in images.values() for p in folder.get(key, []))
                if actual != expected:
                    raise ValueError(f'Artwork variants changed during import: {key}. Run a fresh preview.')

    def read_map(self, path):
        if path is None:
            return {}
        path = Path(path).expanduser().resolve()
        doc = json.loads(self.read(path))
        if not isinstance(doc, dict) or any(not isinstance(v, (str, type(None))) for v in doc.values()):
            raise ValueError('Artwork map must be an object: card ID or final name -> image path (or null).')
        return {k: (path.parent / Path(v).expanduser()).resolve() if v else None for k, v in doc.items()}

    def resolve_art(self, before, after, mapped, faces, sources):
        name = after['name']
        existing = faces['cards'].get(name_key(before['name']), []) if before else []
        source, mode = mapped, 'mapped'
        had_placeholder = bool(before and before.get('art_status') == 'placeholder')
        still_placeholder = False
        if had_placeholder and existing:
            expected = before.get('placeholder_art_sha256') or self.placeholder_digest()
            still_placeholder = digest(self.read(existing[0])) == expected
        if source is None and existing and not still_placeholder:
            source = existing[0]
            mode = 'adopt-installed' if had_placeholder else 'installed' if before['name'] == name else 'rename'
        if source is None:
            matches = sources.get(image_key(name), set())
            if len(matches) > 1:
                self.errors.append(f'{name}: multiple source images match; choose one with --art-map.')
                return {'status': 'ambiguous', 'source': None, 'candidates': sorted(map(str, matches))}
            if matches:
                source, mode = next(iter(matches)), 'matched'
            elif still_placeholder and before['name'] == name:
                return {'status': 'placeholder', 'source': str(existing[0]), 'sha256': digest(self.read(existing[0]))}
            elif faces['cards'].get(name_key(name)):
                source, mode = faces['cards'][name_key(name)][0], 'adopt-installed'
        if source is None and (self.placeholder_missing_art or still_placeholder):
            source, mode = self.root / ART / 'backs/mosaic.webp', 'placeholder'
        if source is None or not source.is_file():
            self.errors.append(f'{name}: missing artwork' + (f' at {source}' if source else '') + '.')
            return {'status': 'missing', 'source': str(source) if source else None}
        raw = self.read(source)
        try:
            with Image.open(source) as image:
                if getattr(image, 'n_frames', 1) != 1:
                    raise ValueError('Animated images are not supported')
                image.verify()
        except (OSError, ValueError, SyntaxError) as exc:
            self.errors.append(f'{name}: unreadable artwork ({exc}).')
            return {'status': 'invalid-image', 'source': str(source)}
        if mode == 'installed':
            return {'status': mode, 'source': str(source), 'sha256': digest(raw)}
        # Keep the selected original locally so a later full art export cannot undo it.
        original = f'reference/workshop-art/{after["id"]}{source.suffix.lower()}'
        after.update(source_art=original, source_sha1=hashlib.sha1(raw).hexdigest())
        if source.resolve() == self.root / ART / 'backs/mosaic.webp':
            mode = 'placeholder'
        if mode == 'placeholder':
            after['art_status'] = 'placeholder'
            after['placeholder_art_sha256'] = self.placeholder_digest()
        else:
            after.pop('art_status', None)
            after.pop('placeholder_art_sha256', None)
        variants = [str(p.relative_to(self.root)) for folder in faces.values()
                    for p in folder.get(name_key(name), [])]
        art = {'status': mode, 'source': str(source), 'sha256': digest(raw), 'original': original,
               'face': str(ART / 'cards' / (name + '.webp')), 'thumbnail': str(ART / 'thumbs' / (name + '.webp')),
               'replacedVariants': variants}
        self.images.append(art)
        return art

    def placeholder_digest(self):
        if self.placeholder_hash is None:
            raw = self.read(self.root / ART / 'backs/mosaic.webp')
            self.placeholder_hash = digest(render_art(raw)[0])
        return self.placeholder_hash

    def integration_review(self):
        """Use the existing pool and audit policies; do not redesign the economy here."""
        cards = self.catalog['cards']
        by_form = {c['form_id']: c for c in cards}
        policy = tool_module('build-opponent-collections.py')
        rows = json.loads(self.read(self.root / 'data/opponent-collections.json'))['opponents']
        skills = {p['name']: p['skillName'] for p in json.loads(self.read(self.root / 'data/opponents.json'))}
        invalid, reserved = [], []
        for row in rows:
            if not policy.valid_pool(skills[row['name']], row['cards'], by_form):
                invalid.append(row['name'])
            blocked = [by_form[i]['name'] for i in row['cards'] if not by_form[i]['acquisition']['available']]
            if blocked:
                reserved.append({'opponent': row['name'], 'cards': blocked})
        audit = tool_module('build-catalog-audit.py')
        routed = [dict(c, routes=audit.routes_for(c, [])) for c in cards]
        buckets = {kind: audit.bucket_report(routed, kind) for kind in ('standard', 'tournament')}
        return {'invalidOpponentPools': invalid, 'reservedCardsInOpponentPools': reserved,
                'starterCardsOutsideTierOne': [c['name'] for c in cards if c['acquisition']['starter'] and (c['tier'] != 1 or c['unique'])],
                'reservedNonUniqueCards': [c['name'] for c in cards if not c['unique'] and not c['acquisition']['available']],
                'packShortfalls': {kind: [{k: r[k] for k in ('tier', 'rarity', 'count', 'maxDraws')} for r in rows if r['risk']]
                                   for kind, rows in buckets.items()},
                'trophies': {str(t): sum(c['unique'] and not c['acquisition']['available'] and c['tier'] == t for c in cards)
                             for t in range(7, 11)},
                'remainingLegacyGroups': sorted({g for c in cards for g in c['groups']} - {t['group'] for t in self.workshop.data['templates']}),
                'note': 'Catalog import only. Review acquisition, starters, signatures, NPC pools and pack/trophy rules before building and installing.'}

    def metadata(self):
        output = {p: text.encode() for p, text in card_outputs(self.catalog, self.registry).items()}
        output.update({'data/cards.json': json_bytes(self.catalog), 'data/released-card-ids.json': json_bytes(self.registry),
                       'include/CreatureGroups.h': creature_groups_output(self.catalog['cards']).encode()})
        sources = dict(self.sources, **{'data/cards.json': digest(output['data/cards.json'])})
        accepted = sorted(set(self.ledger.get('acceptedFingerprints', [])) | {self.doc['catalogFingerprint']})
        output[STATE] = json_bytes({'schema': 1, 'acceptedFingerprints': accepted, 'sources': sources,
                                    'registryHash': digest(json_bytes(self.registry))})
        return output

    def write_report(self, directory):
        directory = Path(directory).resolve()
        # A report must not overwrite any input, including a prior missing-art map.
        outputs = {'plan.json': json_bytes(self.report), 'report.md': self.markdown().encode(),
                   'proposed-cards.json': json_bytes(self.catalog), 'proposed-card-ids.json': json_bytes(self.registry),
                   'missing-art-map.json': json_bytes({c['name']: None for c in self.report['changes']
                                                       if c['art']['status'] in ('missing', 'ambiguous', 'invalid-image')})}
        for filename in outputs:
            if str(directory / filename) in self.inputs:
                raise ValueError('Report would overwrite an input. Use a different --report-dir.')
        for filename, data in outputs.items():
            atomic_bytes(directory / filename, data)

    def markdown(self):
        r, s = self.report, self.report['summary']
        lines = ['# Workshop import preview', '', f"Source: `{r['draft']}`", '',
                 f"{s['selected']} assigned cards: {s['updated']} changed existing cards and {s['new']} new cards. "
                 f"Result: {s['catalogSize']} cards; {r['preserved']} existing identities preserved outside the selection.", '',
                 'Unplaced cards are retained. No runtime mod installation is performed.', '',
                 '## Groups', '', '| Group | Assigned | Empty | Unconfirmed |', '|---|---:|---:|---:|']
        lines += [f"| {g['name']} | {g['assigned']} | {g['empty']} | {g['unconfirmed']} |" for g in r['groups']]
        lines += ['', 'Skipped: ' + (', '.join(r['skippedGroups']) or 'none') + '.', '', '## Import checks', '']
        lines += ['- ' + e for e in self.errors] if self.errors else ['All import checks passed.']
        placeholders = [c['name'] for c in r['changes'] if c['art']['status'] == 'placeholder']
        if placeholders:
            lines += ['', 'Card-back placeholders: ' + ', '.join(placeholders) + '.']
        lines += ['', '## Game integration still to review', '',
                  'Existing acquisition flags and NPC collections are preserved. New non-unique cards are available for standard play; '
                  'new unique cards are reserved for rewards. New cards are not added to starter decks or NPC collections.', '']
        review = r['integration']
        lines += [f"- {len(review['invalidOpponentPools'])} NPC collections fall outside their skill's tier curve.",
                  f"- {len(review['starterCardsOutsideTierOne'])} starter identities are now above tier I or unique.",
                  f"- Non-unique identities with reserved acquisition flags: {len(review['reservedNonUniqueCards'])}.",
                  '- Reserved trophies by tier: ' + ', '.join(f'{t}: {n}' for t, n in review['trophies'].items()) + '.',
                  '- Unassigned groups: ' + (', '.join(review['remainingLegacyGroups']) or 'none') + '.', '',
                  'Current pack buckets below their maximum draws:', '']
        for kind, rows in review['packShortfalls'].items():
            lines += [f"- {kind}: tier {row['tier']} {row['rarity']} — {row['count']} candidates; maximum draws: {row['maxDraws']}."
                      for row in rows]
        lines += ['', 'Details, including all affected NPCs and preserved cards, are in [plan.json](plan.json).', '',
                  '## Card changes', '', '| Card | Group | Metadata changed | Artwork |', '|---|---|---|---|']
        for c in r['changes']:
            def show(value):
                if isinstance(value, list):
                    return ', '.join(map(str, value)) or 'none'
                return str(value).lower() if isinstance(value, bool) else str(value)
            fields = '; '.join(f"{key}: {show(v['after'])}" if c['new'] else
                               f"{key}: {show(v['before'])} → {show(v['after'])}" for key, v in c['fields'].items()) or 'unchanged'
            lines.append(f"| {c['name'].replace('|', '&#124;')} | {c['group']} | {fields.replace('|', '&#124;')} | {c['art']['status']} |")
        return '\n'.join(lines) + '\n'


@contextmanager
def import_lock(root):
    # Also serialize with the separate Tessera art importer.
    path = internal(root, '.tessera-art-importer/import.lock')
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a') as stream:
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise ValueError('Another importer is updating this checkout.') from exc
        yield


def pending_transactions(root):
    for manifest in internal(root, HISTORY).glob('*/manifest.json'):
        if json.loads(manifest.read_text())['status'] in ('installing', 'restoring', 'recovery-required'):
            raise ValueError(f'Interrupted import needs recovery: --restore {manifest.parent}')


def prepare_images(plan, stage, output):
    for art in plan.images:
        raw = Path(art['source']).read_bytes()
        if digest(raw) != art['sha256']:
            raise ValueError(f"Artwork changed: {art['source']}")
        original = stage / art['original']
        original.parent.mkdir(parents=True, exist_ok=True)
        original.write_bytes(raw)
        for relative in (art['face'], art['thumbnail']):
            (stage / relative).parent.mkdir(parents=True, exist_ok=True)
        face, thumb = render_art(raw)
        (stage / art['face']).write_bytes(face)
        (stage / art['thumbnail']).write_bytes(thumb)
        for relative in art['replacedVariants']:
            output.setdefault(relative, None)
        for relative in (art['original'], art['face'], art['thumbnail']):
            output[relative] = stage / relative


def apply_plan(plan):
    if plan.errors:
        raise ValueError('Import checks failed; see the preview report.')
    root = plan.root
    with import_lock(root):
        pending_transactions(root)
        plan.verify_inputs()
        history = internal(root, HISTORY)
        history.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='prepare-', dir=history) as temp:
            stage = Path(temp)
            output = {}
            for relative, data in plan.metadata().items():
                p = stage / relative
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes(data)
                output[relative] = p
            prepare_images(plan, stage, output)
            files = {}
            for relative, proposed in output.items():
                target = internal(root, relative)
                before, after = file_hash(target), file_hash(proposed) if proposed else None
                if before != after:
                    files[relative] = {'before': before, 'after': after}
            if not files:
                return None
            backup = history / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S') + '-' + uuid.uuid4().hex[:10])
            backup.mkdir()
            (backup / 'draft.json').write_bytes(plan.draft_bytes)
            (backup / 'plan.json').write_bytes(json_bytes(plan.report))
            manifest = {'schema': 1, 'status': 'preparing', 'files': files}
            for relative, hashes in files.items():
                if hashes['before'] is not None:
                    saved = backup / 'originals' / relative
                    saved.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(root / relative, saved)
                    if file_hash(saved) != hashes['before']:
                        raise ValueError(f'File changed while backing up: {relative}')
            plan.verify_inputs()
            for relative, hashes in files.items():
                if file_hash(root / relative) != hashes['before']:
                    raise ValueError(f'File changed during import: {relative}')
            manifest['status'] = 'installing'
            atomic_bytes(backup / 'manifest.json', json_bytes(manifest))
            touched = []
            try:
                for relative in files:
                    target = internal(root, relative)
                    touched.append(relative)
                    if output[relative] is None:
                        target.unlink()
                    else:
                        target.parent.mkdir(parents=True, exist_ok=True)
                        os.replace(output[relative], target)
                manifest['status'] = 'applied'
                atomic_bytes(backup / 'manifest.json', json_bytes(manifest))
            except BaseException:
                try:
                    for relative in reversed(touched):
                        restore_file(root, backup, relative, files[relative]['before'])
                    manifest['status'] = 'rolled-back'
                except BaseException:
                    manifest['status'] = 'recovery-required'
                    raise
                finally:
                    atomic_bytes(backup / 'manifest.json', json_bytes(manifest))
                raise
            return backup


def restore_file(root, backup, relative, before):
    target = internal(root, relative)
    if before is None:
        target.unlink(missing_ok=True)
    else:
        atomic_bytes(target, (backup / 'originals' / relative).read_bytes())


def restore_import(root, backup):
    root, backup = Path(root).resolve(), Path(backup).resolve()
    if backup.parent != internal(root, HISTORY):
        raise ValueError('Choose an import backup from this checkout.')
    with import_lock(root):
        manifest = json.loads((backup / 'manifest.json').read_text())
        if manifest.get('schema') != 1 or manifest.get('status') not in ('applied', 'installing', 'restoring', 'recovery-required'):
            raise ValueError('This backup is not an applied or interrupted import.')
        for relative, hashes in manifest['files'].items():
            path = internal(root, relative)
            allowed = {hashes['after']} if manifest['status'] == 'applied' else {hashes['before'], hashes['after']}
            if file_hash(path) not in allowed:
                raise ValueError(f'File changed since import; refusing to overwrite it: {relative}')
            if hashes['before'] is not None and file_hash(backup / 'originals' / relative) != hashes['before']:
                raise ValueError(f'Backup missing or damaged: {relative}')
        manifest['status'] = 'restoring'
        atomic_bytes(backup / 'manifest.json', json_bytes(manifest))
        for relative, hashes in reversed(list(manifest['files'].items())):
            restore_file(root, backup, relative, hashes['before'])
        manifest['status'] = 'restored'
        atomic_bytes(backup / 'manifest.json', json_bytes(manifest))
