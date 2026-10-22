#!/usr/bin/env python3
"""Add authored opponent routes, globals and matching voices without NPC overrides.

Existing form IDs stay stable. New IDs occupy unused slots in the ESL range.
Hearthfire housecarls use native-assigned identity factions, so this plugin does
not need a new master or an override of their actor records.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from bsa_read import Bsa
from plugin_records import ROOT, PLUGIN, walk, parse_fields, build_record, apply, ctda, zstring

OWN = 0x02000000

def generate(blob, profiles, layout, voices, audit):
    records, groups = walk(blob)
    reserved = {form & 0xFFFFFF for form in records if form >> 24 == 2}
    reserved.update(layout.get('retiredForms', []))
    reserved.update(p['knownGlobal'] for p in profiles)
    reserved.update(p['dialogueFaction'] for p in profiles if p.get('dialogueFaction'))
    payloads = {}
    archives = {}
    by_base = {p['base']: p for p in audit}
    by_destination = {int(Path(v['destination']).stem.rsplit('_', 2)[1], 16): v for v in voices}
    # Keep the existing, reviewed accept/decline lines for each voice type.
    voice_samples = {}
    for row in layout['records']:
        for kind in ('accept', 'decline'):
            v = by_destination.get(row[kind])
            if v:
                voice_samples.setdefault((Path(v['destination']).parts[3].lower(), kind), v)

    def allocate():
        for form in range(0x890, 0x1000):
            if form not in reserved:
                reserved.add(form)
                return form
        raise ValueError('Opponent dialogue exhausted the ESL form range')

    def get(local):
        start, end, _ = walk(blob)[0][OWN | local]
        return parse_fields(blob[start:end])

    def put(local, template, fields, parent=None):
        nonlocal blob
        header = bytearray(get(template)[0])
        struct.pack_into('<I', header, 12, OWN | local)
        record = build_record(header, fields)
        records, groups = walk(blob)
        if OWN | local in records:
            start, end, ancestors = records[OWN | local]
        else:
            if parent is None:
                ancestors = records[OWN | template][2]
                group = ancestors[-1]
                start = end = group + struct.unpack_from('<I', blob, group + 4)[0]
            else:
                group, end, _ = next(g for g in groups if g[2] == OWN | parent)
                ancestors = records[OWN | parent][2] + [group]
                start = end
        blob = apply(blob, start, end, ancestors, record)

    def identity(p):
        if p.get('dialogueFaction'):
            return ctda(0, 1, 71, OWN | p['dialogueFaction'])
        master = p.get('master', 'Skyrim.esm')
        index = {'Skyrim.esm': 0, 'Dragonborn.esm': 1}[master]
        return ctda(0, 1, 72, (index << 24) | (p['base'] & 0xFFFFFF))

    def line(p, kind):
        voice = by_base[p['base']]['voice']
        old = voice_samples.get((voice.lower(), kind))
        if old:
            return voice, dict(old), (ROOT / old['destination']).read_bytes()
        choices = by_base[p['base']]['candidates']
        candidates = choices.get(kind, [])
        if not candidates and kind == 'decline':
            candidates = [c for c in choices.get('other', []) if c['text'] in ('Not possible.', 'Not my problem.')]
        if not candidates:
            raise ValueError(f'No reviewed {kind} audio for {p["name"]} ({voice})')
        c = sorted(candidates, key=lambda c: (len(c['text']), c['path']))[0]
        archive = archives.setdefault(c['archive'], Bsa(c['archive']))
        return voice, {'kind': 'vanilla', 'archive': c['archive'], 'source': c['path'], 'text': c['text']}, archive.read(c['path'])

    def voice_file(p, form, prefix, voice, metadata, data):
        extension = '.fuz' if metadata['kind'] == 'vanilla' else '.wav'
        destination = f'Sound/Voice/Tessera TCG.esp/{voice}/{prefix}_{form:08X}_1{extension}'
        metadata = {**metadata, 'speaker': p['name'], 'destination': destination, 'sha256': hashlib.sha256(data).hexdigest()}
        voices[:] = [v for v in voices if v['destination'] != destination]
        voices.append(metadata)
        payloads[destination] = data

    for p in profiles:
        slug = re.sub('[^A-Za-z0-9]', '', p['name'])
        if OWN | p['knownGlobal'] not in walk(blob)[0]:
            put(p['knownGlobal'], 0x910, [(b'EDID', zstring('TTCG_ChallengeKnown_' + slug)), (b'FNAM', b's'), (b'FLTV', struct.pack('<f', 0))])
        if p.get('dialogueFaction'):
            put(p['dialogueFaction'], 0x882, [(b'EDID', zstring('TTCG_Identity_' + slug)), (b'DATA', struct.pack('<I', 1))])
        row = next((r for r in layout['records'] if r['base'] == p['base']), None)
        if row is None:
            row = {'base': p['base'], 'accept': allocate(), 'decline': allocate()}
            layout['records'].append(row)
            for kind, template, state in [('accept', 0x803, 1), ('decline', 0x81E, 2)]:
                form = row[kind]
                voice, metadata, data = line(p, kind)
                fields = [(k, zstring('TTCG_' + kind.title() + '_' + slug) if k == b'EDID' else
                           zstring(metadata['text']) if k == b'NAM1' else
                           struct.pack('<HH', 0x201 if metadata['kind'] == 'vanilla' else 0xA01, 0) if k == b'ENAM' else v)
                          for k, v in get(template)[1] if k != b'CTDA']
                fields += [(b'CTDA', identity(p)), (b'CTDA', ctda(0, state, 74, OWN | p['knownGlobal']))]
                put(form, template, fields, 0x802)
                voice_file(p, form, 'TTCG_Chall_TTCG_ChallengeT', voice, metadata, data)

    # Recalculate serialized response counts and the plugin record count.
    records, groups = walk(blob)
    for form, (start, end, ancestors) in sorted(records.items(), key=lambda r:r[1][0], reverse=True):
        if blob[start:start+4] != b'DIAL':
            continue
        count = sum(1 for a, b, parents in records.values() if blob[a:a+4] == b'INFO' and parents and struct.unpack_from('<I', blob, parents[-1] + 8)[0] == form)
        header, fields = parse_fields(blob[start:end])
        fields = [(k, struct.pack('<I', count) if k == b'TIFC' else v) for k, v in fields]
        blob = apply(blob, start, end, ancestors, build_record(header, fields))
    records, groups = walk(blob)
    start, end, ancestors = records[0]
    header, fields = parse_fields(blob[start:end])
    # HEDR includes all non-header records and GRUPs.
    fields = [(k, v[:4]+struct.pack('<II', len(records)-1+len(groups), max(reserved)+1) if k == b'HEDR' else v) for k,v in fields]
    blob = apply(blob, start, end, ancestors, build_record(header, fields))
    layout['records'].sort(key=lambda r:next(i for i,p in enumerate(profiles) if p['base']==r['base']))
    return blob, layout, voices, payloads


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    read = lambda path: json.loads((ROOT/path).read_text())
    profiles = read('data/opponents.json')
    layout = read('data/free-play-record-layout.json')
    voices = read('data/voice-manifest.json')
    audit = read('build/free-play/vanilla-voice-audit.json')
    blob, layout, voices, payloads = generate(PLUGIN.read_bytes(), profiles, layout, voices, audit)
    outputs = {PLUGIN: blob, **{ROOT/path:data for path,data in payloads.items()}}
    for path, data in [('data/free-play-record-layout.json', layout), ('data/voice-manifest.json', voices)]:
        outputs[ROOT/path] = (json.dumps(data, indent=2, ensure_ascii=False)+'\n').encode()
    for path, data in outputs.items():
        if args.check:
            assert path.read_bytes() == data, f'Regenerate {path}'
        elif not path.exists() or path.read_bytes() != data:
            path.parent.mkdir(parents=True, exist_ok=True)
            temp = path.with_suffix(path.suffix+'.tmp');temp.write_bytes(data);temp.replace(path)
    print(f'{len(profiles)} adult challenge routes verified')

if __name__ == '__main__':
    main()
