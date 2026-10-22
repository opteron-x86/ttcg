#!/usr/bin/env python3
"""Find real vanilla response/audio pairs for each Tessera opponent voice type."""
from pathlib import Path
import mmap,struct,zlib,json,re,os
from bsa_read import Bsa
root=Path(__file__).resolve().parents[1];game=Path(os.environ.get('TESSERA_GAME_DATA',root.parent.parent/'Skyrim Special Edition/Data'))
interface=Bsa(game/'Skyrim - Interface.bsa')
def strings(mod):
 b=interface.read('strings/'+mod+'_english.ilstrings');n,length=struct.unpack_from('<II',b);start=8+n*8;result={}
 for i in range(n):
  id,offset=struct.unpack_from('<II',b,8+i*8);size=struct.unpack_from('<I',b,start+offset)[0];result[id]=b[start+offset+4:start+offset+4+size].rstrip(b'\0').decode('utf-8')
 return result
def fields(b):
 out={};i=0;extended=None
 while i<len(b):
  key,n=struct.unpack_from('<4sH',b,i);i+=6
  if key==b'XXXX':extended=struct.unpack_from('<I',b,i)[0];i+=n;continue
  if extended is not None:n=extended;extended=None
  out.setdefault(key,[]).append(b[i:i+n]);i+=n
 assert i==len(b);return out
npcs={};types={};candidates={};texts={}
accept={"Certainly.","Of course.","Sure.","All right.","Alright.","Very well.","Why not?","Yes.","Sure, why not?","Let's do it.","Lead on."}
decline={"No.","Not now.","Sorry, no.","No, thank you.","I don't think so.","I'm busy.","Maybe another time.","Not right now.","I think not.","No thanks."}
for mod in ['Skyrim','HearthFires','Dragonborn']:
 text=strings(mod);wanted={id:s for id,s in text.items() if len(s.strip())<=120};voice_archive=game/'Skyrim - Voices_en0.bsa'
 archive=Bsa(voice_archive);audio={}
 for name in archive.files:
  m=re.search(r'_([0-9a-f]{8})_(\d+)\.fuz$',name)
  if m and name.startswith('sound\\voice\\'+mod.lower()+'.esm\\'):
   audio.setdefault((int(m[1],16)&0xffffff,int(m[2])),[]).append(name)
 with (game/(mod+'.esm')).open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as data:
  def walk(a,b):
   while a<b:
    tag=data[a:a+4];n,flags,id=struct.unpack_from('<III',data,a+4)
    if tag==b'GRUP':walk(a+24,a+n);a+=n;continue
    if tag in (b'NPC_',b'VTYP',b'INFO'):
     body=data[a+24:a+24+n]
     if flags&0x40000:body=zlib.decompress(body[4:])
     prefix={'Skyrim':0,'HearthFires':0xFB000000,'Dragonborn':0xDB000000}[mod]
     v=fields(body);key=(prefix|(id&0xffffff)) if prefix and id>>24 else id
     if tag==b'VTYP':types[key]=v.get(b'EDID',[b''])[0].rstrip(b'\0').decode()
     elif tag==b'NPC_':
      if b'VTCK' in v:
       voice=struct.unpack('<I',v[b'VTCK'][0])[0];voice=prefix|(voice&0xffffff) if prefix and voice>>24 else voice;npcs[key]=voice
     elif tag==b'INFO':
      for number,value in enumerate(v.get(b'NAM1',[]),1):
       if len(value)!=4:continue
       txt=wanted.get(struct.unpack('<I',value)[0]);
       if not txt:continue
       for file in audio.get((id&0xffffff,number),[]):
        voice=file.split('\\')[3];kind='accept' if txt.strip() in accept else 'decline' if txt.strip() in decline else 'other';candidates.setdefault(voice,{}).setdefault(kind,[]).append({'text':txt,'archive':str(voice_archive),'path':file,'info':f'{mod}.esm:{id&0xffffff:06X}','response':number})
    a+=24+n
  walk(0,len(data))
profiles=json.loads((root/'data/opponents.json').read_text());report=[]
for p in profiles:
 voice=types.get(npcs.get(p['base'],0),'');report.append({'base':p['base'],'name':p['name'],'voice':voice,'candidates':candidates.get(voice.lower(),{})})
out=root/'build/free-play';out.mkdir(exist_ok=True);(out/'vanilla-voice-audit.json').write_text(json.dumps(report,indent=2)+'\n')
missing=[(p['name'],p['voice'],[k for k in ('accept','decline') if not p['candidates'].get(k)]) for p in report if not all(p['candidates'].get(k) for k in ('accept','decline'))]
print('Audited',len(report),'profiles;',len(report)-len(missing),'have both response types; missing:',sorted(set((v,tuple(k)) for _,v,k in missing)))
print('Voice types:',sorted({p['voice'] for p in report}))
