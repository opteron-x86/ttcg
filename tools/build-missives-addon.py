#!/usr/bin/env python3
"""Build the optional addon from its own records; never modify Missives."""
from pathlib import Path
import struct
from plugin_records import field, zstring, ctda, walk, parse_fields

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'addons/missives'
MASTERS = ['Skyrim.esm', 'Update.esm', 'Dragonborn.esm', 'Missives.esp', 'Tessera TCG.esp']
OWN = len(MASTERS) << 24
MISSIVES = 3 << 24
STORES_TEXT = 0x100
# Board replacers can change the base object while retaining the original
# reference. Accept those references as well as ordinary Missives boards.
BOARD_REFERENCES = (0x12CC, 0x9477, 0x948F, 0x9491, 0x94A2, 0x94A6, 0x94AA,
                    0x94AE, 0x94B2, 0x21838, 0x2183A, 0x21840, 0x21844, 0x21846)
HOLDS = [
    ('Whiterun', 0x16772, 0x1D8F), ('Falkreath', 0x1676F, 0x9463),
    ('Haafingar', 0x16770, 0x9468), ('Hjaalmarch', 0x1676E, 0x9458),
    ('The Pale', 0x1676D, 0x945C), ('The Rift', 0x1676C, 0x946B),
    ('Winterhold', 0x1676B, 0x9470), ('Eastmarch', 0x1676A, 0x1D91),
    ('The Reach', 0x16769, 0x945F),
]

def u32(n): return struct.pack('<I', n)
def text(s):
    b = s.encode('utf-8')
    return struct.pack('<H', len(b)) + b
def ref(form, alias=-1): return struct.pack('<HhI', 0, alias, form)
def prop(name, value, kind=1):
    if kind == 3: value = struct.pack('<i', value)
    return text(name) + bytes([kind, 1]) + value
def script(name, props=()): return text(name) + b'\0' + struct.pack('<H', len(props)) + b''.join(props)
def vmad(name, props=(), stages=(), aliases=(), quest=0):
    data = struct.pack('<HHH', 5, 2, 1) + script(name, props)
    data += b'\x02' + struct.pack('<H', len(stages)) + text(name)
    for stage, function in stages:
        data += struct.pack('<HHIB', stage, 0, 0, 1) + text(name) + text(function)
    data += struct.pack('<H', len(aliases))
    for alias, name, props in aliases:
        data += ref(quest, alias) + struct.pack('<HHH', 5, 2, 1) + script(name, props)
    return data
def info_vmad():
    name = 'Tessera_MissivesDialogue'
    return struct.pack('<HHH', 5, 2, 1) + script(name) + b'\x02\x02' + text(name) + b'\x01' + text(name) + text('Fragment_0')
def record(tag, form, fields, flags=0):
    body = b''.join(field(k.encode(), v) for k, v in fields)
    return tag.encode() + struct.pack('<IIIIHH', len(body), flags, form, 0, 44, 0) + body
def group(label, kind, body):
    if isinstance(label, int): label = u32(label)
    return b'GRUP' + u32(len(body)+24) + label + struct.pack('<IHHHH', kind, 0, 0, 0, 0) + body
def alias(number, name, flags, fill):
    return [('ALST', u32(number)), ('ALID', zstring(name)), ('FNAM', u32(flags))] + fill + [('VTCK', u32(0)), ('ALED', b'')]
def quest_data(flags=0): return struct.pack('<HBBII', flags, 40, 255, 0, 6)

def build():
    records = {k: [] for k in ('MISC','BOOK','MESG','FLST','QUST','DLBR','DIAL')}
    def put(tag, local, fields, flags=0):
        data = record(tag, OWN|local, fields, flags);records[tag].append(data);return data
    init = OWN|0x800
    put('MISC', 0x804, [('EDID',zstring('TesseraMissivesText')),('OBND',bytes(12)),('FULL',zstring('Card')),('DATA',bytes(8))])
    put('MESG', 0x803, [('EDID',zstring('TesseraMissivesDiscard')),('DESC',zstring('Discard this collector notice?')),('INAM',u32(0)),('DNAM',u32(1)),('ITXT',zstring('Discard')),('ITXT',zstring('Keep'))])
    put('FLST', 0x801, [('EDID',zstring('TesseraMissivesBoardLists'))]+[('LNAM',u32(MISSIVES|h[2])) for h in HOLDS])
    put('FLST', 0x802, [('EDID',zstring('TesseraMissivesJobs'))]+[('LNAM',u32(OWN|0x820+i*16)) for i in range(9)])
    props = [prop('Boards',ref(OWN|0x801)),prop('Jobs',ref(OWN|0x802))]
    fields = [('EDID',zstring('TesseraMissivesInit')),('VMAD',vmad('Tessera_MissivesInit',props,aliases=[(0,'Tessera_MissivesPlayer',[])],quest=init)),('FULL',zstring('Tessera collector notices')),('DNAM',quest_data(1)),('NEXT',b''),('ANAM',u32(1))]
    fields += alias(0,'Player',0x200,[('ALFR',u32(0x14))])
    put('QUST',0x800,fields)
    proxy_refs=[]
    for i,(hold,location,_) in enumerate(HOLDS):
        local=0x820+i*16;q=OWN|local;book=q+1;branch=q+2;topic=q+3
        request_ref=OWN|(0x910+i*2);reward_ref=request_ref+1
        for index,form in enumerate((request_ref,reward_ref)):
            name_base=0x930+i*2+index
            put('MISC',name_base,[('EDID',zstring(f'TesseraMissivesName{i+1}_{index}')),('OBND',bytes(12)),('FULL',zstring('Card')),('DATA',bytes(8))])
            proxy_refs.append(record('REFR',form,[('NAME',u32(OWN|name_base)),('DATA',struct.pack('<6f',0,0,-100,0,0,0))],0xC00))
        body='<font face="$HandwrittenFont">Wanted: one standard <Alias=Requested> Tessera card.<br><br>I offer <Alias=Reward> in exchange. Bring your album and speak to me when you have the card.<br><br>--<Alias=Collector></font>'
        # Capitalize after substitution: Papyrus can return a cached lowercase
        # spelling even when the card's catalog name starts with a capital.
        put('BOOK',local+1,[('EDID',zstring(f'TesseraMissivesNote{i+1}')),('OBND',bytes(12)),('FULL',zstring('Missive: <Alias.Cap=Requested> Card')),('MODL',zstring('Clutter\\Books\\Note01.nif')),('DESC',zstring(body)),('YNAM',u32(0xC7A54)),('KSIZ',u32(1)),('KWDA',u32(0x937A2)),('DATA',bytes.fromhex('00000000ffffffff0000000000000000')),('INAM',u32(0x97788)),('CNAM',b'\0')])
        props=[prop('Region',i+1,3),prop('Notice',ref(book))]
        props += [prop(name,ref(q,number)) for name,number in [('Collector',2),('Board',6),('Missive',7),('Requested',3),('Reward',4)]]
        attachments=[(2,'Tessera_MissivesCollector',[]),(7,'Tessera_MissivesNote',[prop('DiscardMessage',ref(OWN|0x803))])]
        fields=[('EDID',zstring(f'TesseraMissivesJob{i+1}')),('VMAD',vmad('Tessera_MissivesQuest',props,[(0,'Fragment_0'),(100,'Fragment_1'),(105,'Fragment_2'),(110,'Fragment_3')],attachments,q)),('FULL',zstring(f'{hold}: A Card for a Collector')),('DNAM',quest_data()),('NEXT',b'')]
        for stage,flags,log_flags in [(0,2,0),(20,0,0),(100,0,1),(105,0,2),(110,0,0)]:
            fields += [('INDX',struct.pack('<HBB',stage,flags,0)),('QSDT',bytes([log_flags]))]
        fields += [('QOBJ',struct.pack('<H',20)),('FNAM',u32(0)),('NNAM',zstring('Bring a standard <Alias=Requested> to <Alias=Collector>')),('QSTA',struct.pack('<II',2,0)),('ANAM',u32(8))]
        fields += [('ALLS',u32(1)),('ALID',zstring('Hold')),('FNAM',u32(0x200)),('ALFL',u32(location)),('ALED',b'')]
        # The note and objective read names saved by these source aliases.
        fields += alias(2,'Collector',0x202|STORES_TEXT,[('ALFR',u32(0))]) # Empty forced alias; no unconstrained search.
        fields += alias(3,'Requested',0x282|STORES_TEXT,[('ALFR',u32(request_ref))])
        fields += alias(4,'Reward',0x282|STORES_TEXT,[('ALFR',u32(reward_ref))])
        # The OR group keeps base-object swaps working; the final AND limits
        # the search to this hold even when another board is closer.
        board_conditions = [('CTDA',ctda(1,1,136,MISSIVES|ref)) for ref in BOARD_REFERENCES]
        board_conditions += [('CTDA',ctda(0,1,72,MISSIVES|0x12CB)),('CTDA',ctda(0,1,360,1))]
        fields += alias(6,'Board',0x2200,board_conditions)
        fields += alias(7,'Missive',0x4206,[('ALFR',u32(0))]) # Filled only after naming the offer.
        put('QUST',local,fields)
        put('DLBR',local+2,[('EDID',zstring(f'TesseraMissivesBranch{i+1}')),('QNAM',u32(q)),('TNAM',u32(0)),('DNAM',u32(1)),('SNAM',u32(topic))])
        dial=put('DIAL',local+3,[('EDID',zstring(f'TesseraMissivesDelivery{i+1}')),('FULL',zstring('About the Tessera card you wanted.')),('PNAM',struct.pack('<f',50)),('BNAM',u32(branch)),('QNAM',u32(q)),('DATA',bytes(4)),('SNAM',b'CUST'),('TIFC',u32(1))])
        info=record('INFO',q+4,[('VMAD',info_vmad()),('ENAM',bytes(4)),('PNAM',u32(0)),('CNAM',b'\0'),('DNAM',u32(0xDBA22)),('CTDA',ctda(0,20,58,q)),('CTDA',ctda(0,1,566,2))])
        records['DIAL'][-1]=dial+group(topic,7,info)
    cell=OWN|0x900
    interior=record('CELL',cell,[('EDID',zstring('TesseraMissivesTextCell')),('FULL',zstring('Tessera')),('DATA',struct.pack('<H',1))])
    cell_data=group(b'CELL',0,group((cell&0xFFFFFF)%10,2,group((cell&0xFFFFFF)//10%10,3,interior+group(cell,6,group(cell,8,b''.join(proxy_refs))))))
    # Forced REFR aliases need their persistent references to exist when the
    # quest is loaded. Follow the game's record order: CELL before QUST.
    contents=b''
    for tag,entries in records.items():
        if tag=='QUST':contents+=cell_data
        contents+=group(tag.encode(),0,b''.join(entries))
    # HEDR counts both records and groups, matching the engine/editor convention.
    parsed,groups=walk(contents)
    header=[('HEDR',struct.pack('<fII',1.7,len(parsed)+len(groups),0x950)),('CNAM',zstring('Tessera')),('SNAM',zstring('Optional collector notices for Tessera and Missives.'))]
    for master in MASTERS:header += [('MAST',zstring(master)),('DATA',bytes(8))]
    plugin=record('TES4',0,header,0x200)+contents
    OUT.mkdir(parents=True,exist_ok=True);(OUT/'Seq').mkdir(exist_ok=True)
    (OUT/'Tessera Missives.esp').write_bytes(plugin)
    (OUT/'Seq/Tessera Missives.seq').write_bytes(u32(init))
    print(f'Built Tessera Missives.esp: {len(parsed)} records; nine hold quests; no overrides')

if __name__=='__main__':build()
