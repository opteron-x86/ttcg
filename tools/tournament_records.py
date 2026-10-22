"""Add host dialogue using the existing album quest and native bridge."""
import struct
from plugin_records import walk,parse_fields,build_record,apply,ctda,zstring,shared_response
IDS=(0x02000883,0x02000884,0x02000885)
HOSTS=(0x13BA3,0x13655,0x13266,0x135ED,0x13621,0x13365,0x1C183,0x1412A,0x133A3,0x01018290)
def generate_tournaments(blob):
 records,groups=walk(blob)
 def cloned(source,id,replace):
  a,b,_=records[source];header,fields=parse_fields(blob[a:b]);header=bytearray(header);struct.pack_into('<I',header,12,id)
  return build_record(header,replace(fields))
 branch=cloned(0x02000830,IDS[0],lambda f:[(k,zstring('TTCG_TournamentBranch') if k==b'EDID' else struct.pack('<I',IDS[1]) if k==b'SNAM' else v) for k,v in f])
 topic=cloned(0x02000831,IDS[1],lambda f:[(k,zstring('TTCG_TournamentTopic') if k==b'EDID' else zstring('Tell me about the Tessera tournaments.') if k==b'FULL' else struct.pack('<I',IDS[0]) if k==b'BNAM' else v) for k,v in f])
 def response_fields(fields):
  result=[]
  for k,v in fields:
   if k==b'CTDA':continue
   if k==b'EDID':v=zstring('TTCG_TournamentResponse')
   if k==b'VMAD':
    old=b'TTCG_AlbumPurchaseFragment';new=b'TTCG_TournamentFragment';v=v.replace(struct.pack('<H',len(old))+old,struct.pack('<H',len(new))+new)
   result.append((k,v))
  result.append((b'CTDA',ctda(0x60,1,74,0x02000850)))
  for base in HOSTS:result.append((b'CTDA',ctda(1,1,72,base)))
  result.append((b'CTDA',ctda(0,1,71,0x5091B))) # Skyrim.esm JobInnkeeperFaction
  return shared_response(result,0x0007F6BC) # Take a look. Speech only; never its barter script.
 response=cloned(0x02000832,IDS[2],response_fields)
 if IDS[0] in records:
  for id,record in zip(IDS,(branch,topic,response)):
   records,_=walk(blob);a,b,parents=records[id];blob=apply(blob,a,b,parents,record)
  return blob
 # Insert the branch into its existing top group.
 _,_,parents=records[0x02000830];parent=parents[0];end=parent+struct.unpack_from('<I',blob,parent+4)[0];blob=apply(blob,end,end,[parent],branch)
 records,_=walk(blob);parent=records[0x02000831][2][0];end=parent+struct.unpack_from('<I',blob,parent+4)[0]
 group=b'GRUP'+struct.pack('<IIIHHHH',24+len(response),IDS[1],7,0,0,0,0)+response
 blob=apply(blob,end,end,[parent],topic+group)
 records,_=walk(blob);a,b,parents=records[0];header,fields=parse_fields(blob[a:b]);fields=[(k,v[:4]+struct.pack('<I',struct.unpack_from('<I',v,4)[0]+4)+v[8:] if k==b'HEDR' else v) for k,v in fields]
 return apply(blob,a,b,parents,build_record(header,fields))
