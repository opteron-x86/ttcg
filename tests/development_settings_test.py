from pathlib import Path
import importlib.util,subprocess,tempfile,configparser
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('settings',root/'tools/update-development-settings.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
text,removed=module.refresh((root/'SKSE/Plugins/TTCG.ini.example').read_text(),'[Development]\nEnabled=1\nAddAllCards=1\nFriendlyGame=1\nOpponentSkill=2\n[Input]\nCollectionKey=70\n[Audio]\nEnabled=0\n')
assert 'FriendlyGame' not in text and removed==['Development.friendlygame']
assert 'AddAllCards=1' in text and 'OpponentSkill=2' in text and 'CollectionKey=70' in text and '[Audio]\n; Match music only; rule and result sounds remain enabled.\nEnabled=0' in text
assert 'WhiterunTournament' not in text
assert all(key+'=0' in text for key in ('RegularTournaments','InvitationalTournaments','MastersTournaments'))
assert 'TournamentHold=1' in text
assert '[Interface]' in text and 'PauseWorldInAlbum=1' in text
unpaused,_=module.refresh((root/'SKSE/Plugins/TTCG.ini.example').read_text(),'[Interface]\nPauseWorldInAlbum=0\n')
assert 'PauseWorldInAlbum=0' in unpaused and 'PauseWorldDuringMatches=1' in unpaused
match_unpaused,_=module.refresh((root/'SKSE/Plugins/TTCG.ini.example').read_text(),'[Interface]\nPauseWorldDuringMatches=0\n')
assert 'PauseWorldDuringMatches=0' in match_unpaused and 'PauseWorldInAlbum=1' in match_unpaused
pause_keys=('PauseWorldInAlbum','PauseWorldInShops','PauseWorldInMatchSetup','PauseWorldDuringMatches')
assert all(key+'=1' in text for key in pause_keys)
for key in pause_keys:
 changed,_=module.refresh((root/'SKSE/Plugins/TTCG.ini.example').read_text(),'[Interface]\n'+key+'=0\n')
 assert key+'=0' in changed and all(other+'=1' in changed for other in pause_keys if other!=key)
all_unpaused,_=module.refresh((root/'SKSE/Plugins/TTCG.ini.example').read_text(),'[Interface]\n'+'\n'.join(key+'=0' for key in pause_keys)+'\n')
assert all(key+'=0' in all_unpaused for key in pause_keys)
assert module.refresh((root/'SKSE/Plugins/TTCG.ini.example').read_text(),text)==(text,[])
# Retired keys are dropped, never used to enable a current test switch.
for value in ('1','2','9','0','-1','bad'):
 migrated,removed=module.refresh(text,'[Development]\nWhiterunTournament='+value+'\n')
 assert 'InvitationalTournaments=0' in migrated and removed==['Development.whiteruntournament']
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory)/'TTCG.ini';old='[Development]\nEnabled=0\nWhiterunTournament=1\nRules=-1\n[Input]\nCollectionKey=70\n[Audio]\nEnabled=0\n';path.write_text(old)
 command=['python3',str(root/'tools/update-development-settings.py'),str(path),'--all-tournaments']
 subprocess.run(command,check=True,capture_output=True,text=True)
 config=configparser.ConfigParser();config.read(path)
 for key in ('Enabled','RegularTournaments','InvitationalTournaments','MastersTournaments'):assert config.getint('Development',key)==1
 assert config.getint('Development','TournamentHold')==1 and config.getint('Development','Rules')==-1
 assert config.getint('Input','CollectionKey')==70 and config.getint('Audio','Enabled')==0
 backups=list(Path(directory).glob('TTCG.ini.backup-*'));assert len(backups)==1 and backups[0].read_text()==old
 assert subprocess.run(command,check=True,capture_output=True,text=True).stdout.strip()=='INI already current'
print('Development INI: obsolete switches removed, active values/input/audio preserved, test option added, idempotent refresh passed')
