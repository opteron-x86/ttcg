"""Validate the serialized dialogue routing and exact staged/installed voice payloads."""
from pathlib import Path
import json, struct, sys, hashlib, wave
root=Path(__file__).resolve().parents[1]
artifact=root/'build/clean-catalog/runtime' if '--stage' in sys.argv else root
blob=(artifact/'Tessera TCG.esp').read_bytes();records={};info_counts={}
def walk(start,end,parent=None):
    while start<end:
        tag=blob[start:start+4];n=struct.unpack_from('<I',blob,start+4)[0]
        if tag==b'GRUP':
            group_type=struct.unpack_from('<I',blob,start+12)[0]
            walk(start+24,start+n,struct.unpack_from('<I',blob,start+8)[0] if group_type==7 else None);start+=n;continue
        form=struct.unpack_from('<I',blob,start+12)[0];fields={};at=start+24
        while at<start+24+n:
            key=blob[at:at+4];size=struct.unpack_from('<H',blob,at+4)[0]
            fields.setdefault(key,[]).append(blob[at+6:at+6+size]);at+=6+size
        records[form]=(tag,fields)
        if tag==b'INFO' and parent is not None:info_counts[parent]=info_counts.get(parent,0)+1
        start+=24+n
walk(0,len(blob))
for form,(tag,entry) in records.items():
    if tag==b'DIAL' and b'TIFC' in entry:assert struct.unpack('<I',entry[b'TIFC'][0])[0]==info_counts.get(form,0),(hex(form),"dialogue response count")
own=0x02000000
layout=json.loads((root/'data/free-play-record-layout.json').read_text())
voices=json.loads((root/'data/voice-manifest.json').read_text())
profiles=json.loads((root/'data/opponents.json').read_text())
assert len(layout['records'])==len(profiles)
assert all((id&0xffffff)<=0xfff for id in records if id>>24==2)
assert (artifact/'Seq/Tessera TCG.seq').read_bytes()==struct.pack('<I',own|0x800)
# Every owned response is behind one leading AND, before any existing OR chain.
# Remove only that verified gate for the route-specific assertions below.
for form,(tag,entry) in records.items():
    if tag==b'INFO' and form>>24==2:
        conditions=entry.get(b'CTDA',[])
        enabled=[c for c in conditions if struct.unpack_from('<H',c,8)[0]==74 and struct.unpack_from('<I',c,12)[0]==own|0xFDA]
        assert len(enabled)==1 and conditions[0]==enabled[0],hex(form)
        assert enabled[0][0]==0 and struct.unpack_from('<f',enabled[0],4)[0]==1
        assert struct.unpack_from('<I',enabled[0],20)[0]==0
        entry[b'CTDA']=conditions[1:]
assert records[own|0xFDA][0]==b'GLOB'
assert records[own|0xFDA][1][b'FLTV']==[struct.pack('<f',1)]
fields=lambda id:records[own|id][1]
def shared(info, source):
    assert info[b'DNAM']==[struct.pack('<I',source)]
    assert not any(key in info for key in (b'TRDT',b'NAM1',b'NAM2',b'NAM3',b'SNAM',b'LNAM'))
    assert info[b'CNAM']==[b'\0']
    flags=struct.unpack_from('<H',info[b'ENAM'][0])[0]
    assert flags&1 and flags&0x200 and not flags&0x800 # Goodbye, subtitles, actual LIP.
    order=list(info);assert order.index(b'VMAD')<order.index(b'ENAM')<order.index(b'CNAM')<order.index(b'DNAM')<order.index(b'CTDA')
def gate(info,global_id,value):
    return any(struct.unpack_from('<H',c,8)[0]==74 and struct.unpack_from('<I',c,12)[0]==own|global_id and struct.unpack_from('<f',c,4)[0]==value for c in info.get(b'CTDA',[]))
assert len(layout['retiredForms'])==77
assert all(own|form in records for form in layout['retiredForms'])
for local in layout['retiredForms']:
    tag,entry=records[own|local]
    if tag==b'INFO':
        # This cannot become eligible even if a save restores the old global.
        conditions=entry[b'CTDA']
        assert len(conditions)==1
        c=conditions[0]
        assert c[0]==0 and struct.unpack_from('<f',c,4)[0]==-1
        assert struct.unpack_from('<H',c,8)[0]==72 and struct.unpack_from('<I',c,12)[0]==7
        assert b'TCLT' not in entry and entry[b'NAM1']==[b'\0']
for index,(row,p) in enumerate(zip(layout['records'],profiles)):
    assert row['base']==p['base']
    yes,no=fields(row['accept']),fields(row['decline'])
    assert gate(yes,p['knownGlobal'],1) and gate(no,p['knownGlobal'],2)
    for response in (yes,no):
        if p.get('dialogueFaction'):
            assert records[own|p['dialogueFaction']][0]==b'FACT'
            assert any(struct.unpack_from('<H',c,8)[0]==71 and struct.unpack_from('<I',c,12)[0]==own|p['dialogueFaction'] for c in response[b'CTDA'])
        else:
            base=((0x01000000 if p['base']>>24==0xDB else 0)|(p['base']&0xffffff))
            assert any(struct.unpack_from('<H',c,8)[0]==72 and struct.unpack_from('<I',c,12)[0]==base for c in response[b'CTDA'])
    assert b'TTCG_ChallengeFragment' in yes[b'VMAD'][0]
    assert b'VMAD' not in no
child=fields(layout['childAccept'])
assert any(struct.unpack_from('<H',c,8)[0]==365 for c in child.get(b'CTDA',[]))
assert any(struct.unpack_from('<H',c,8)[0]==74 and struct.unpack_from('<I',c,12)[0]==own|0x850 and c[0]==0x60 for c in child[b'CTDA'])
# Tournament hosts share one album-quest branch with an explicit host allowlist.
tournament_info=fields(0x885)
assert fields(0x883)[b'SNAM']==[struct.pack('<I',own|0x884)]
assert fields(0x884)[b'BNAM']==[struct.pack('<I',own|0x883)]
assert b'TTCG_TournamentFragment' in tournament_info[b'VMAD'][0]
shared(tournament_info,0x0007F6BC) # Vanilla "Take a look."; local script opens Tessera.
assert len(tournament_info[b'CTDA'])==12
assert struct.unpack_from('<H',tournament_info[b'CTDA'][-1],8)[0]==71 and struct.unpack_from('<I',tournament_info[b'CTDA'][-1],12)[0]==0x5091B
assert {struct.unpack_from('<I',c,12)[0] for c in tournament_info[b'CTDA'][1:-1]}=={0x13BA3,0x13655,0x13266,0x135ED,0x13621,0x13365,0x1C183,0x1412A,0x133A3,0x01018290}
# Standard top-level purchase dialogue, available by role or detected service faction.
branch,topic,response=fields(0x830),fields(0x831),fields(0x832)
assert records[own|0x830][0]==b'DLBR' and branch[b'DNAM']==[struct.pack('<I',1)]
assert branch[b'QNAM']==[struct.pack('<I',own|0x800)] and branch[b'SNAM']==[struct.pack('<I',own|0x831)]
assert topic[b'BNAM']==[struct.pack('<I',own|0x830)] and topic[b'QNAM']==branch[b'QNAM']
assert topic[b'FULL']==[b"I'd like to buy a Tessera Album. (100 gold)\0"]
shared(response,0x000DBA22) # Vanilla "Of course."; local script purchases the album.
assert b'TTCG_AlbumPurchaseFragment' in response[b'VMAD'][0]
conditions=response[b'CTDA'];assert len(conditions)==3 and all(len(c)==32 for c in conditions)
def condition(c):return (c[0],struct.unpack_from('<f',c,4)[0],struct.unpack_from('<H',c,8)[0],struct.unpack_from('<I',c,12)[0],struct.unpack_from('<I',c,20)[0])
assert [condition(c) for c in conditions]==[(0,0,74,own|0x850,0),(1,1,71,0x51599,0),(0,1,71,own|0x882,0)]
shop=fields(0xFD9)
assert fields(0xFD7)[b'SNAM']==[struct.pack('<I',own|0xFD8)]
assert fields(0xFD8)[b'BNAM']==[struct.pack('<I',own|0xFD7)]
assert fields(0xFD8)[b'FULL']==[b"I'd like to trade Tessera cards.\0"]
assert b'TTCG_CardShopFragment' in shop[b'VMAD'][0]
shared(shop,0x0007F6BC)
assert [condition(c) for c in shop[b'CTDA']]==[(0x60,1,74,own|0x850,0),(1,1,71,0x51599,0),(0,1,71,own|0x882,0)]
assert 'TTCG_Native.OpenCardShop(akSpeakerRef as Actor)' in (root/'Scripts/Source/TTCG_CardShopFragment.psc').read_text()
assert records[own|0x882][0]==b'FACT'
album=fields(0x870)
assert records[own|0x870][0]==b'BOOK'
assert struct.unpack_from('<I',album[b'DATA'][0],8)[0]==0
assert 0xFF9FB in struct.unpack('<'+'I'*(len(album[b'KWDA'][0])//4),album[b'KWDA'][0])
assert b'TTCG_AlbumItem' in album[b'VMAD'][0]

# A single leveled-list draw creates 0–2 physical packs, independent of player level.
roll = fields(0x880)
assert records[own | 0x880][0] == b'LVLI'
assert roll[b'LVLD'] == [bytes([33])] and roll[b'LVLF'] == [bytes([0])]
assert roll[b'LLCT'] == [bytes([2])]
assert [struct.unpack('<HHIHH', v) for v in roll[b'LVLO']] == [(1, 0, own | 0x840, n, 0) for n in (1, 2)]
assert records[own | 0x881][0] == b'KYWD'
assert struct.pack('<I', own | 0x881) in fields(0x840)[b'KWDA'][0]
assert struct.unpack('<I', fields(0x840)[b'DATA'][0][:4])[0] == 200
for id,(tag,f) in records.items():
    if tag==b'QUST' and id>>24==2:
        assert bool(struct.unpack_from('<H',f[b'DNAM'][0])[0]&1)==((id&0xffffff) in (0x800,))
assert b'TTCG_EncountersQuest' in fields(0x800)[b'VMAD'][0]
assert len({v['destination'] for v in voices})==len(voices)
for v in voices:
    path=root/v['destination'];payload=path.read_bytes()
    assert hashlib.sha256(payload).hexdigest()==v['sha256'],path
    info_id,number=path.stem.rsplit('_',2)[1:];info=fields(int(info_id,16))
    assert info[b'NAM1'][int(number)-1].rstrip(b'\0').decode('cp1252')==v['text'],path
    flags=struct.unpack_from('<H',info[b'ENAM'][0])[0]
    assert flags&0x200,(path,hex(flags),v["kind"])
    if v['kind']=='vanilla':assert payload[:4]==b'FUZE' and not flags&0x800
    else:
        assert flags&0x800
        with wave.open(str(path)) as wav:assert (wav.getnchannels(),wav.getsampwidth(),wav.getframerate())==(1,2,44100)
sounds={0x811:'RulePlus',0x812:'RuleCombo',0x813:'RuleSame',0x814:'RuleSame',0x815:'ResultDraw',0x816:'ResultLose',0x817:'ResultWin',0x869:'OpeningOpponent',0x86A:'OpeningPlayer',0x86B:'RuleLegion',0x86C:'RuleDecimation'}
for id,name in sounds.items():
 path=f'SFX\\TTCG_{name}.wav'
 assert records[own|id][0]==b'SNDR' and fields(id)[b'ANAM'][0].rstrip(b'\0')==path.encode()
 assert (root/'Sound'/path.replace('\\','/')).is_file()
print(f'Records/audio: {len(records)-1} records, album purchase dialogue, packs, {len(profiles)} adult routes and {len(voices)} voice payloads passed')

# Notes use native inventory events: a stacked note cannot call GetBaseObject.
invitation_source=(root/'Scripts/Source/TTCG_TournamentInvitation.psc').read_text()
assert 'Scriptname TTCG_TournamentInvitation extends ObjectReference' in invitation_source
assert 'GetBaseObject(' not in invitation_source and 'OnContainerChanged(' not in invitation_source
native=(root/'src/Plugin.cpp').read_text()
assert 'event->baseObj' in native and 'event->oldContainer==0x39FB9' in native and 'event->oldContainer==0x39FB7' in native
encounters=(root/'Scripts/Source/TTCG_EncountersQuest.psc').read_text()
assert 'courier.AddItemToContainer(invitation, 1)' in encounters
assert 'TTCG_Native.TournamentInvitation(invitation, false)' in encounters
assert fields(0xFC0)[b'SNAM']==[struct.pack('<I',own|0xFC1)]
assert fields(0xFC1)[b'TIFC']==[struct.pack('<I',10)]
for h in range(10):
    info=fields(0xFC2+h)
    assert b'TTCG_TournamentEntryFragment' in info[b'VMAD'][0]
    assert gate(info,0xFCC+h,1) and len(info[b'CTDA'])==2
    shared(info,0x000DBA22)
    assert records[own|0xFCC+h][0]==b'GLOB'
    for circuit in range(3):
        note=fields(0xF80+h*3+circuit)
        assert records[own|0xF80+h*3+circuit][0]==b'BOOK'
        assert b'VMAD' not in note
        assert note[b'MODL']==[b'Clutter\\Books\\Note01.nif\0']
        assert b'Speak to me there to enter' in note[b'DESC'][0]
        assert b'seven days from the delivery' in note[b'DESC'][0]
print('Tournament invitations: 30 courier notes, 10 host registration gates, native delivery and fragment bindings passed')

assert "TTCG_Native.BackgroundUpdate()" in encounters
assert "PathToReference" not in encounters and "NextMaster" not in encounters
assert all("TTCG_Master" not in v["destination"] for v in voices)
prompts={f[b'FULL'][0].rstrip(b'\0').decode('cp1252') for form,(tag,f) in records.items() if tag==b'DIAL' and b'FULL' in f and form&0xffffff not in layout['retiredForms']}
assert set((root/'docs/dbvo/Tessera TCG - Topics.txt').read_text().splitlines())==prompts
