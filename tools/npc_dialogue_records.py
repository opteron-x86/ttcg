"""Shared runtime opponent gates and responses; no per-NPC ESP records."""
import struct
from plugin_records import walk, parse_fields, build_record, apply, ctda, zstring, shared_response

# Stable public forms used by author-written dialogue conditions.
AVAILABILITY = 0x02000886
GENERIC = 0x02000887
ACCEPT = 0x02000888
DECLINE = 0x02000889
NO_CARDS = 0x0200088A

def generate_npc_dialogue(blob):
    def put(form, template, fields, parent=None):
        nonlocal blob
        records, groups = walk(blob)
        a, z, parents = records[template]
        header = bytearray(parse_fields(blob[a:z])[0]); struct.pack_into('<I', header, 12, form)
        record = build_record(header, fields)
        if form in records:
            a, z, parents = records[form]
        elif parent:
            a, z, _ = next(g for g in groups if g[2] == parent)
            parents = records[parent][2] + [a]; a = z
        else:
            group = parents[-1]; a = z = group + struct.unpack_from('<I', blob, group + 4)[0]
        blob = apply(blob, a, z, parents, record)

    for form, name in ((AVAILABILITY, 'TTCG_PlayerAvailability'), (GENERIC, 'TTCG_GenericPlayerDialogue')):
        put(form, 0x02000882, [(b'EDID', zstring(name)), (b'DATA', struct.pack('<I', 0))])
    for form, rank, text in ((ACCEPT, 1, 'Of course.'), (DECLINE, 2, 'Not right now.'), (NO_CARDS, 3, "I don't have enough cards for a game.")):
        template = 0x02000803 if rank == 1 else 0x0200081E
        a, z, _ = walk(blob)[0][template]
        _, fields = parse_fields(blob[a:z])
        fields = [(k, zstring('TTCG_RuntimeResponse_' + str(rank)) if k == b'EDID' else
                   zstring(text) if k == b'NAM1' else struct.pack('<HH', 0xA01, 0) if k == b'ENAM' else v)
                  for k, v in fields if k not in (b'CTDA', b'DNAM')]
        # The acceptance can share the game's matching vanilla recording;
        # custom voices fall back to the same subtitle rather than a wrong voice.
        if rank == 1:
            fields = shared_response(fields, 0x000DBA22)
        fields += [(b'CTDA', ctda(0, rank, 73, AVAILABILITY)),
                   (b'CTDA', ctda(0, 1, 71, GENERIC)),
                   (b'CTDA', ctda(0, 0, 590, 0)),
                   (b'CTDA', ctda(0, 0, 289, 0)),
                   (b'CTDA', ctda(0, 0, 46, 0)),
                   (b'CTDA', ctda(0, 0, 35, 0))]
        put(form, template, fields, 0x02000802)
    records, groups = walk(blob)
    for form, (a, z, parents) in sorted(records.items(), key=lambda p:p[1][0], reverse=True):
        if blob[a:a+4] != b'DIAL': continue
        count = sum(1 for x, y, ancestors in records.values() if blob[x:x+4] == b'INFO' and ancestors and struct.unpack_from('<I', blob, ancestors[-1]+8)[0] == form)
        header, fields = parse_fields(blob[a:z]);fields = [(k, struct.pack('<I', count) if k == b'TIFC' else v) for k,v in fields]
        blob = apply(blob,a,z,parents,build_record(header,fields))
    records, groups = walk(blob);a,z,parents=records[0];header,fields=parse_fields(blob[a:z])
    maximum=max(form & 0xFFFFFF for form in records if form >> 24 == 2)+1
    fields=[(k,v[:4]+struct.pack('<II',len(records)-1+len(groups),maximum) if k==b'HEDR' else v) for k,v in fields]
    return apply(blob,a,z,parents,build_record(header,fields))
