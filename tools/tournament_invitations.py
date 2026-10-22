"""Courier letters and host entry dialogue owned by Tessera."""
import struct
from plugin_records import walk, parse_fields, build_record, apply, ctda, zstring, shared_response
from tournament_records import HOSTS
VENUES=(('Whiterun','The Bannered Mare','Hulda'),('Falkreath',"Dead Man's Drink",'Valga Vinicia'),('Haafingar','The Winking Skeever','Corpulus Vinius'),('Hjaalmarch','Moorside Inn','Jonna'),('The Pale','Windpeak Inn','Thoring'),('The Rift','The Bee and Barb','Keerava'),('Winterhold','The Frozen Hearth','Dagur'),('Eastmarch','Candlehearth Hall','Elda Early-Dawn'),('The Reach','Silver-Blood Inn','Kleppr'),('Solstheim','The Retching Netch','Geldis Sadri'))
OWN=0x02000000

def generate_invitations(blob):
 before_records,before_groups=walk(blob)
 def record(source,local,fields):
  rs,_=walk(blob);a,b,_=rs[source];header=bytearray(blob[a:a+24]);struct.pack_into('<I',header,12,OWN|local)
  return build_record(header,fields)
 def fields(source):
  rs,_=walk(blob);a,b,_=rs[source];return parse_fields(blob[a:b])[1]
 def put(id,payload,tag):
  nonlocal blob
  rs,groups=walk(blob)
  if id in rs:
   a,b,parents=rs[id];blob=apply(blob,a,b,parents,payload);return
  parent=next(a for a,_,_ in groups if struct.unpack_from('<I',blob,a+12)[0]==0 and blob[a+8:a+12]==tag)
  end=parent+struct.unpack_from('<I',blob,parent+4)[0];blob=apply(blob,end,end,[parent],payload)
 album=fields(OWN|0x870)
 for hold,(region,venue,host) in enumerate(VENUES):
  for circuit in range(3):
   kind=('tournament','invitational','Tessera Masters')[circuit]
   opening=(f"I've heard of your games in {region}. I'd like you to join our next Tessera tournament.",f"Your reputation in {region} has earned you a place at our invitational. Expect a strong field.","You are invited to the Tessera Masters. Masters from across Skyrim will compete for our highest prize.")[circuit]
   body=f"<font face='$HandwrittenFont'>{opening}<br><br>Bring this invitation and your album to {venue}. Speak to me there to enter. You have seven days from the delivery of this letter. The closing date and prize are recorded in your album.<br><br>{host}</font>"
   replacements={b'EDID':zstring(f'TTCG_Invitation_{hold+1}_{circuit}'),b'FULL':zstring(f'{region}: {kind} invitation'),b'MODL':zstring('Clutter\\Books\\Note01.nif'),b'DESC':zstring(body),b'INAM':struct.pack('<I',0x97788)}
   data=[(k,replacements.get(k,v)) for k,v in album if k!=b'VMAD']
   put(OWN|0xF80+hold*3+circuit,record(OWN|0x870,0xF80+hold*3+circuit,data),b'BOOK')
  global_fields=[(b'EDID',zstring(f'TTCG_Registration_{hold+1}')),(b'FNAM',b'f'),(b'FLTV',struct.pack('<f',0))]
  put(OWN|0xFCC+hold,record(OWN|0x850,0xFCC+hold,global_fields),b'GLOB')
 # A separate topic proves registration came through the host, rather than album browsing.
 branch=[(k,zstring('TTCG_RegistrationBranch') if k==b'EDID' else struct.pack('<I',OWN|0xFC1) if k==b'SNAM' else v) for k,v in fields(OWN|0x883)]
 put(OWN|0xFC0,record(OWN|0x883,0xFC0,branch),b'DLBR')
 topic=[(k,zstring('TTCG_RegistrationTopic') if k==b'EDID' else zstring("I'd like to enter the Tessera tournament.") if k==b'FULL' else struct.pack('<I',OWN|0xFC0) if k==b'BNAM' else struct.pack('<I',10) if k==b'TIFC' else v) for k,v in fields(OWN|0x884)]
 responses=[]
 for hold,base in enumerate(HOSTS):
  result=[]
  for k,v in fields(OWN|0x885):
   if k==b'CTDA':continue
   if k==b'EDID':v=zstring(f'TTCG_RegistrationResponse_{hold+1}')
   if k==b'VMAD':
    old=b'TTCG_TournamentFragment';new=b'TTCG_TournamentEntryFragment';v=v.replace(struct.pack('<H',len(old))+old,struct.pack('<H',len(new))+new)
   result.append((k,v))
  result += [(b'CTDA',ctda(0,1,72,base)),(b'CTDA',ctda(0,1,74,OWN|0xFCC+hold))]
  responses.append(record(OWN|0x885,0xFC2+hold,shared_response(result,0x000DBA22))) # Of course.
 rs,_=walk(blob)
 if OWN|0xFC1 in rs:
  put(OWN|0xFC1,record(OWN|0x884,0xFC1,topic),b'DIAL')
  for hold,payload in enumerate(responses):put(OWN|0xFC2+hold,payload,b'DIAL')
 else:
  payload=b''.join(responses)
  group=b'GRUP'+struct.pack('<IIIHHHH',24+len(payload),OWN|0xFC1,7,0,0,0,0)+payload
  put(OWN|0xFC1,record(OWN|0x884,0xFC1,topic)+group,b'DIAL')
 rs,gs=walk(blob);delta=len(rs)+len(gs)-len(before_records)-len(before_groups)
 a,b,parents=rs[0];header,data=parse_fields(blob[a:b])
 data=[(k,v[:4]+struct.pack('<I',struct.unpack_from('<I',v,4)[0]+delta)+struct.pack('<I',max(struct.unpack_from('<I',v,8)[0],0xFD6))+v[12:] if k==b'HEDR' else v) for k,v in data]
 return apply(blob,a,b,parents,build_record(header,data))
