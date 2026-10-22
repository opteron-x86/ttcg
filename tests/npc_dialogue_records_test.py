"""Public NPC dialogue gates remain reusable, singular, and idempotent."""
import sys,struct
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from plugin_records import PLUGIN, walk,parse_fields
from npc_dialogue_records import AVAILABILITY,GENERIC,ACCEPT,DECLINE,NO_CARDS
from importlib.util import spec_from_file_location,module_from_spec
spec=spec_from_file_location('builder',PLUGIN.parent/'tools/build-plugin.py');module=module_from_spec(spec);spec.loader.exec_module(module)
blob=PLUGIN.read_bytes();assert module.generate(blob)[0]==blob
records,_=walk(blob)
for form in (AVAILABILITY,GENERIC):
 start,end,_=records[form];assert blob[start:start+4]==b'FACT';fields=dict(parse_fields(blob[start:end])[1]);assert struct.unpack('<I',fields[b'DATA'])[0]==0
for form,state in ((ACCEPT,1),(DECLINE,2),(NO_CARDS,3)):
 start,end,_=records[form];fields=parse_fields(blob[start:end])[1];conditions=[v for k,v in fields if k==b'CTDA'];f=dict(fields)
 assert any(struct.unpack_from('<H',c,8)[0]==73 and struct.unpack_from('<I',c,12)[0]==AVAILABILITY and struct.unpack_from('<f',c,4)[0]==state for c in conditions)
 assert any(struct.unpack_from('<H',c,8)[0]==71 and struct.unpack_from('<I',c,12)[0]==GENERIC for c in conditions)
 assert struct.unpack_from('<I',conditions[0],12)[0]==0x02000FDA
 assert (b'VMAD' in f)==(state==1)
print('NPC dialogue: shared readiness, custom-topic suppression, disabled-game gate and generation verified')
