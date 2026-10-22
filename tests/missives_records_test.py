"""Validate the optional plugin, references, script bindings and refresh lists."""
from pathlib import Path
import struct
import sys
import re
import importlib.util
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from plugin_records import walk, parse_fields
spec=importlib.util.spec_from_file_location('missives_builder',Path(__file__).resolve().parents[1]/'tools/build-missives-addon.py')
builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
OUT,OWN,MASTERS,HOLDS,MISSIVES=(getattr(builder,key) for key in ('OUT','OWN','MASTERS','HOLDS','MISSIVES'))

blob=(OUT/'Tessera Missives.esp').read_bytes()
records,groups=walk(blob)
def fields(form):
    a,b,_=records[form];return parse_fields(blob[a:b])[1]
assert struct.unpack_from('<I',blob,8)[0]&0x200
assert [v.rstrip(b'\0').decode() for k,v in fields(0) if k==b'MAST']==MASTERS
assert len(records)==88
assert all(form>>24==len(MASTERS) and 0x800<=(form&0xFFFFFF)<0x1000 for form in records if form)
assert (OUT/'Seq/Tessera Missives.seq').read_bytes()==struct.pack('<I',OWN|0x800)
assert len({form&0xFFFFFF for form in records})==len(records)
assert records[OWN|0x900][0]<records[OWN|0x820][0], 'Persistent references must load before forced quest aliases'
name_bases=[]
for local in range(0x910,0x922):
    reference=dict(fields(OWN|local))
    base=struct.unpack('<I',reference[b'NAME'])[0]
    assert base==OWN|(local+0x20)
    assert dict(fields(base))[b'FULL']==b'Card\0'
    name_bases.append(base)
assert len(set(name_bases))==18, 'Every hold and every text field needs a separate base name'
cell_parent=[(a,b) for a,b,_ in groups if a<records[OWN|0x900][0]<b]
assert [(struct.unpack_from('<I',blob,a+12)[0],struct.unpack_from('<I',blob,a+8)[0]) for a,b in cell_parent if struct.unpack_from('<I',blob,a+12)[0] in (2,3)]==[(2,4),(3,0)]
assert [struct.unpack('<I',v)[0] for k,v in fields(OWN|0x801) if k==b'LNAM']==[MISSIVES|h[2] for h in HOLDS]

class VM:
    def __init__(self,b):self.b=b;self.at=0
    def read(self,fmt):
        result=struct.unpack_from('<'+fmt,self.b,self.at);self.at+=struct.calcsize('<'+fmt);return result[0] if len(result)==1 else result
    def text(self):
        n=self.read('H');s=self.b[self.at:self.at+n].decode();self.at+=n;return s
    def script(self):
        name=self.text();self.read('B');n=self.read('H');props={}
        for _ in range(n):
            key=self.text();kind,status=self.read('BB');assert status==1
            if kind==1:props[key]=self.read('HhI')
            else:assert kind==3;props[key]=self.read('i')
        path=OUT/'Scripts/Source'/f'{name}.psc';assert path.is_file(),name
        declared=set(re.findall(r'\bProperty\s+(\w+)',path.read_text(),re.I))
        assert set(props)<=declared,(name,props,declared)
        return name,props

for i,hold in enumerate(HOLDS):
    q=OWN|0x820+i*16;f=fields(q);vm=VM(dict(f)[b'VMAD'])
    assert vm.read('HHH')==(5,2,1)
    name,props=vm.script();assert name=='Tessera_MissivesQuest' and props['Region']==i+1
    assert props['Notice']==(0,-1,q+1)
    assert vm.read('BH')==(2,4) and vm.text()==name
    stages={}
    for _ in range(4):
        stage,unknown,index,flags=vm.read('HHIB');assert unknown==index==0 and flags==1
        assert vm.text()==name;stages[stage]=vm.text()
    assert stages=={0:'Fragment_0',100:'Fragment_1',105:'Fragment_2',110:'Fragment_3'}
    assert vm.read('H')==2
    attached=[]
    for _ in range(2):
        unused,alias,form=vm.read('HhI');assert unused==0 and form==q
        assert vm.read('HHH')==(5,2,1);attached.append((alias,vm.script()[0]))
    assert attached==[(2,'Tessera_MissivesCollector'),(7,'Tessera_MissivesNote')] and vm.at==len(vm.b)
    aliases={};current=None
    for k,v in f:
        if k in (b'ALST',b'ALLS'):current=struct.unpack('<I',v)[0];aliases[current]=[]
        elif current is not None:aliases[current].append((k,v))
    assert set(aliases)=={1,2,3,4,6,7}
    for prop,number in [('Collector',2),('Requested',3),('Reward',4),('Board',6),('Missive',7)]:assert props[prop]==(0,number,q)
    board_conditions=[v for k,v in aliases[6] if k==b'CTDA']
    assert [struct.unpack_from('<H',v,8)[0] for v in board_conditions]==[136]*len(builder.BOARD_REFERENCES)+[72,360]
    assert [struct.unpack_from('<I',v,12)[0] for v in board_conditions[:-2]]==[MISSIVES|ref for ref in builder.BOARD_REFERENCES]
    assert all(v[0]==1 for v in board_conditions[:-2])
    assert board_conditions[-2][0]==board_conditions[-1][0]==0
    assert struct.unpack_from('<I',board_conditions[-2],12)[0]==MISSIVES|0x12CB
    assert struct.unpack_from('<I',board_conditions[-1],12)[0]==1
    assert dict(aliases[7])[b'FNAM']==struct.pack('<I',0x4206)
    assert dict(aliases[2])[b'ALFR']==dict(aliases[7])[b'ALFR']==bytes(4)
    note=dict(fields(q+1));assert all(b'<Alias='+n+b'>' in note[b'DESC'] for n in (b'Requested',b'Reward',b'Collector'))
    # A valid tag/binding is not enough: the engine substitutes [...] unless
    # every named source alias stores text for this quest instance.
    by_name={dict(values)[b'ALID'].rstrip(b'\0'):dict(values) for values in aliases.values()}
    assert note[b'FULL']==b'Missive: <Alias.Cap=Requested> Card\0'
    replacements=set(re.findall(rb'<Alias(?:\.Cap)?=([^>]+)>',note[b'FULL']+note[b'DESC']+dict(f)[b'NNAM']))
    assert replacements=={b'Requested',b'Reward',b'Collector'}
    for replacement in replacements:
        source=by_name[replacement]
        flags=struct.unpack('<I',source[b'FNAM'])[0]
        assert flags&0x100,(hold[0],replacement.decode(),'missing Stores Text')
    for number,local_ref in ((3,0x910+i*2),(4,0x911+i*2)):
        ref_form=struct.unpack('<I',dict(aliases[number])[b'ALFR'])[0]
        assert ref_form==OWN|local_ref
        assert records[ref_form][0]<records[q][0], 'Forced reference must precede its quest'
    assert struct.unpack('<I',dict(aliases[7])[b'FNAM'])[0]&0x4000
    info=dict(fields(q+4));assert info[b'DNAM']==struct.pack('<I',0xDBA22)
    conditions=[v for k,v in fields(q+4) if k==b'CTDA']
    assert [struct.unpack_from('<H',v,8)[0] for v in conditions]==[58,566]
    assert struct.unpack_from('<f',conditions[0],4)[0]==20

sources={p.stem for p in (OUT/'Scripts/Source').glob('*.psc')}
assert sources=={p.stem for p in (OUT/'Scripts').glob('*.pex')}
for name in sources:assert (OUT/'Scripts'/f'{name}.pex').read_bytes()[:4]==bytes.fromhex('fa57c0de')
assert not any('RegisterForUpdate' in p.read_text() or 'RegisterForSingleUpdate' in p.read_text() for p in (OUT/'Scripts/Source').glob('*.psc'))
print('Missives addon: ESL, masters, all nine jobs, alias/name bindings, stage fragments, dialogue and scripts passed')
