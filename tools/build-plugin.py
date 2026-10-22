#!/usr/bin/env python3
"""Refresh album purchase dialogue, book metadata and start-enabled quests."""
import argparse, struct
from plugin_records import ROOT, PLUGIN, walk, parse_fields, build_record, apply, ctda, shared_response

from tournament_records import generate_tournaments
from shop_records import generate_shop
from tournament_invitations import generate_invitations
from game_settings_records import generate_game_settings
from pack_assets import apply_pack_models
from child_dialogue_records import generate_child_dialogue
from npc_dialogue_records import generate_npc_dialogue
from legacy_compat_records import disable_retired_approaches

def generate_reward_pack(blob):
 # The tournament prize is the same kind of movable/sellable MISC stack as
 # ordinary packs. Only the runtime opener chooses the improved card pool.
 records,_=walk(blob);source=0x02000840;target=0x02000FD6
 start,end,parents=records[source];header,fields=parse_fields(blob[start:end]);header=bytearray(header)
 struct.pack_into('<I',header,12,target)
 fields=[(k,b'TTCG_TournamentPack\0' if k==b'EDID' else b'Tessera Tournament Pack\0' if k==b'FULL' else v) for k,v in fields]
 record=build_record(header,fields)
 if target in records:
  start,end,parents=records[target];return apply(blob,start,end,parents,record)
 group=parents[0];end=group+struct.unpack_from('<I',blob,group+4)[0]
 blob=apply(blob,end,end,[group],record)
 records,_=walk(blob);start,end,parents=records[0];header,fields=parse_fields(blob[start:end])
 fields=[(k,v[:4]+struct.pack('<II',struct.unpack_from('<I',v,4)[0]+1,max(struct.unpack_from('<I',v,8)[0],0xFD7)) if k==b'HEDR' else v) for k,v in fields]
 return apply(blob,start,end,parents,build_record(header,fields))

def generate(blob):
 blob=generate_reward_pack(blob)
 blob=apply_pack_models(blob)
 blob=generate_tournaments(blob)
 records,_=walk(blob)
 for form in (0x02000831,0x02000832):
  start,end,parents=records[form];header,fields=parse_fields(blob[start:end])
  if form==0x02000831:
   fields=[(k,b"I'd like to buy a Tessera Album. (100 gold)\0" if k==b'FULL' else v) for k,v in fields]
  else:
   fields=shared_response([(k,v) for k,v in fields if k!=b'CTDA'],0x000DBA22) # Of course.
   # Global on the subject is independent of actor. Faction checks run on the
   # speaker: vanilla general-goods role OR a detected mod service-faction seller.
   fields += [(b'CTDA',ctda(0,0,74,0x02000850)),
              (b'CTDA',ctda(1,1,71,0x51599)),
              (b'CTDA',ctda(0,1,71,0x02000882))]
  blob=apply(blob,start,end,parents,build_record(header,fields));records,_=walk(blob)
 start,end,parents=records[0x02000870]
 header,fields=parse_fields(blob[start:end])
 # BOOK DATA is flags/type, taught skill/spell, value, weight.
 fields=[(k,v[:8]+struct.pack('<I',0)+v[12:] if k==b'DATA' else v) for k,v in fields]
 keywords=[]
 for k,v in fields:
  if k==b'KWDA':keywords.extend(struct.unpack('<'+'I'*(len(v)//4),v))
 for keyword in (0x937A2,0xFF9FB): # VendorItemBook and VendorNoSale: personal album, no resale.
  if keyword not in keywords:keywords.append(keyword)
 fields=[(k,v) for k,v in fields if k not in (b'KSIZ',b'KWDA')]
 # BOOK text (DESC) precedes keywords; DATA and INAM follow them.
 index=next(i for i,(k,v) in enumerate(fields) if k==b'DATA')
 fields[index:index]=[(b'KSIZ',struct.pack('<I',len(keywords))),(b'KWDA',struct.pack('<'+'I'*len(keywords),*keywords))]
 blob=apply(blob,start,end,parents,build_record(header,fields))
 # Invitations copy the album, so repair its layout before cloning the notes.
 blob=generate_invitations(blob)
 blob=generate_shop(blob)
 blob=generate_child_dialogue(blob)
 blob=generate_npc_dialogue(blob)
 blob=disable_retired_approaches(blob)
 blob=generate_game_settings(blob)
 records,_=walk(blob);quests=[]
 for form,(start,end,_) in records.items():
  if blob[start:start+4]==b'QUST':
   fields=dict(parse_fields(blob[start:end])[1])
   if struct.unpack_from('<H',fields[b'DNAM'])[0]&1:quests.append(form)
 return blob,struct.pack('<'+'I'*len(quests),*sorted(quests))

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
 plugin,seq=generate(PLUGIN.read_bytes());outputs={PLUGIN:plugin,ROOT/'Seq/Tessera TCG.seq':seq}
 for path,content in outputs.items():
  if args.check:
   if path.read_bytes()!=content:raise SystemExit(f'Regenerate {path.name}')
  elif path.read_bytes()!=content:
   temp=path.with_suffix(path.suffix+'.tmp');temp.write_bytes(content);temp.replace(path)
 print('Plugin album dialogue and start-enabled quests verified')
if __name__=='__main__':main()
