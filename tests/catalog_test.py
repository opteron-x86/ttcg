"""Check the installed catalog, script API, voice files and current documentation."""
from pathlib import Path
import importlib.util,json,re,struct,hashlib,sys
root=Path(__file__).resolve().parents[1]
catalog=json.loads((root/'data/cards.json').read_text())['cards']
profiles=json.loads((root/'data/opponents.json').read_text())
sys.path.insert(0,str(root/'tools'))
from runtime_files import validate_runtime_art
validate_runtime_art(root,catalog)
assert catalog and profiles
assert len({c['name'].casefold() for c in catalog})==len(catalog)
for c in catalog:
 if not c['acquisition']['available']:assert c.get('roster_status')=='pending' or c['unique'] and c['tier']>=8
 if c['tier']==7 and not c['unique'] and c.get('roster_status')!='pending':assert c['acquisition']['available']
pools=json.loads((root/'data/opponent-collections.json').read_text())['opponents']
assert {p['name'] for p in profiles}=={p['name'] for p in pools}
reserved={c['form_id'] for c in catalog if not c['acquisition']['available']}
assert all(not reserved.intersection(p['cards']) for p in pools)
spec=importlib.util.spec_from_file_location('opponent_pools',root/'tools'/'build-opponent-collections.py')
policy=importlib.util.module_from_spec(spec);spec.loader.exec_module(policy)
cards={c['form_id']:c for c in catalog}
skill={p['name']:p['skillName'] for p in profiles}
for row in pools:
 assert policy.valid_pool(skill[row['name']],row['cards'],cards,row['name']=="M'aiq the Liar"),(row['name'],skill[row['name']],len(row['cards']))
 for form in policy.SIGNATURES.get(row['name'],[]):
  assert form in row['cards']
  owners=[p['name'] for p in pools if form in p['cards']]
  assert owners==[row['name']], (cards[form]['name'], owners)
layout=json.loads((root/'data/free-play-record-layout.json').read_text())
assert [r['base'] for r in layout['records']]==[p['base'] for p in profiles]
script_names={p.stem for p in (root/'Scripts/Source').glob('*.psc')}
assert {p.stem for p in (root/'Scripts').glob('*.pex')}==script_names
native=(root/'src/Plugin.cpp').read_text();api=(root/'Scripts/Source/TTCG_Native.psc').read_text()
assert set(re.findall(r'Function (\w+)\(',api))==set(re.findall(r'RegisterFunction\("(\w+)","TTCG_Native"',native))
assert 'SetUniqueID(0x54544347)' in native
assert b"TTCG_Album\0" in (root/'Tessera TCG.esp').read_bytes()
native_collection=(root/'include/NativeCollection.h').read_text()
assert 'ttcg::supportedCollectionFormat(version)' in native_collection
for p in [root/'README.md',root/'docs/reference-guide.md',root/'docs/opponents-by-hold.md',root/'docs/card-art-replacers.md']:
 text=p.read_text()
 for link in re.findall(r'\]\(([^)]+)\)',text):
  if ':' not in link and not link.startswith('#'):assert (p.parent/link.split('#')[0]).exists(),(p,link)
for p in profiles:assert '| '+p['name']+' |' in (root/'docs/opponents-by-hold.md').read_text(),p['name']
print('Catalog: reserved acquisition, art filenames, adult pools, current Papyrus API and reference guides passed')
