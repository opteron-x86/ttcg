#!/usr/bin/env python3
"""Render fixtures come from the production match, trade and tournament code."""
import json, subprocess, copy, os
from pathlib import Path
root=Path(__file__).resolve().parent.parent
hosts=Path(os.environ.get('TESSERA_TEST_HOSTS',root/'build/native/hosts'))
out=root/'build/test-fixtures';out.mkdir(parents=True,exist_ok=True)
class Host:
 def __init__(self,name):
  self.p=subprocess.Popen([str(hosts/('ttcg-'+name+'-host'))],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True);self.state=json.loads(self.p.stdout.readline())
 def command(self,verb):
  v,*args=verb.split(' ',1);s=self.state
  self.p.stdin.write(f'{v} {s["session"]} {s["revision"]} '+(args[0] if args else '')+'\n');self.p.stdin.flush();self.state=json.loads(self.p.stdout.readline());return copy.deepcopy(self.state)
 def close(self):self.p.stdin.close();self.p.wait()
shop=Host('shop');base=copy.deepcopy(shop.state);shop.close();N=len(base['cards'])//2
regions=[dict(id=i+1,name=name,rules=16|1|2,trade=2,reputationTitle='Renowned',wins=34,ruleChanges=[]) for i,name in enumerate(['Whiterun','Falkreath','Haafingar','Hjaalmarch','The Pale','The Rift','Winterhold','Eastmarch','The Reach','Solstheim'])]
regions[0]['ruleChanges']=[dict(rule=2,active=True,until='22 Last Seed, 4E 201 · 10:00'),dict(rule=8,active=False,until='23 Last Seed, 4E 201 · 10:00')]
regions += [dict(id=11,name='Travellers',travellers=True)]
settings=dict(windowSize=95,gamesEnabled=True,residents=1,vanillaVoicesOnly=True,rankLayout=0,showCardNames=True,nameplateStyle=0,nameCase=0,theme=0,boardSurface=0,boardTexture=10,interfaceFont=0,cardNameFont=0,cardValueFont=0,rankShadowOpacity=100,rankShadowBlur=2,rankShadowOffset=1,rankStroke=False,rankStrokeWeight=10,pauseWorldInAlbum=True,pauseWorldInShops=True,pauseWorldInMatchSetup=True,pauseWorldDuringMatches=True,collection=65,challenge=66,toggleGames=0,capturing='',notice='',interfaceNotice='',residentCandidate='Look at an eligible NPC before opening the album.',canAddResident=False,development=dict(enabled=True,RegularTournaments=True,InvitationalTournaments=True,MastersTournaments=True,TournamentHold=1,ShowAllPlayers=True,UnlockAllPlayers=True,RevealAllCards=True,AllowAnyNPC=False,RecordResults=True,Rules=-1,TradeRule=-1,Wager=-1,OpponentSkill=-1,RuleLesson=0))
base.update(settings=settings,interruptedMatch=None,screen='album',matchContext=False)
base['collection'].update(packs=[3,2],regions=regions,holdID=1,hold='Whiterun',skill='Regular',canPlay=True,maxWager=200,fixedTrade=2,fixedWager=-1,ruleLesson=0)
base['collection']['known']=[bool(x) for x in base['collection']['known']]
base['collection']['roster']=[dict(name=n,location=l,skill=k,wins=12,losses=6,draws=2,holdID=h,hold='Whiterun' if h==1 else 'Travellers',traveller=h==11,unlocked=True,friendlyOnly=f) for n,l,k,h,f in [('Hulda','The Bannered Mare','Regular',1,False),('Amren','Whiterun','Expert',1,False),('Lars Battle-Born','Whiterun','Beginner',1,True),("M’aiq the Liar",'Skyrim','Master',11,False)]]
def normalize(s):
 result=copy.deepcopy(base);result.update(copy.deepcopy(s));result['collection']=copy.deepcopy(base['collection'])|s.get('collection',{})
 c=result['collection'];c['known']=[bool(v) for v in c['known']]
 if len(c['owned'])<2*N:c['owned']+= [0]*(2*N-len(c['owned']))
 return result
def save(name,s):
 (out/(name+'.json')).write_text(json.dumps(normalize(s),ensure_ascii=False))
save('album',base);save('shop',dict(base,screen='shop'));save('lobby',dict(base,screen='lobby',opponent='Hulda',rules=16|1|2))
save('interrupted',dict(base,interruptedMatch=dict(opponent='Hulda',tournament=0,canResume=True,finished=False)))
pack=copy.deepcopy(base);pack['collection'].update(revealID=1,revealKind='tournament',revealed=[0,4,N+2,130,N+350]);save('pack',pack)
t=Host('tournament');t.command('test-regular 1');t.command('test-host');save('tournaments',t.state)
t.command('tournament enter '+str(t.state['collection']['tournaments']['events'][-1]['id']));save('tournament-lobby',t.state)
for circuit in range(3):
 for reward in range(5):
  if reward==0 and not circuit or reward==2 and circuit:continue
  t.command(f'test-final-setup {circuit} {reward}');s=t.state;save('match',s)
  s=t.command('play %d %d'%(s['testFinalMove']['hand'],s['testFinalMove']['square']));save(f'final-c{circuit}-r{reward}-animation',s)
  s=t.command('settled');save(f'final-c{circuit}-r{reward}',s)
t.close()
u=Host('ui');d=u.state['collection']['deck'];u.command('start 16403 1 0 '+' '.join(map(str,d)));save('opening',u.state)
# Follow complete production matches to generate real trade snapshots.
for rounds in range(200):
 s=u.state
 if s['settling']:u.command('settled');continue
 if s['phase']=='result':
  s=normalize(s);outcome=0 if s['score'][0]>s['score'][1] else 1 if s['score'][1]>s['score'][0] else -1
  s['collection'].update(outcome=outcome,required=1,reward=bool(s['collection']['reward']),paying=False,transferOwners=[0]*5+[1]*5)
  save('trade',s);break
 if s['turn']==0:
  h=next(i for i,v in enumerate(s['hands'][0]) if v is not None);q=next(i for i,v in enumerate(s['board']) if v is None);u.command(f'play {h} {q}')
 else:u.command('settled')
u.close()
for mode,name in [('--multi-fixtures','multi'),('--fixtures','combo')]:
 result=subprocess.run([str(hosts/'ttcg-ui-host'),mode],capture_output=True,text=True,check=True)
 for i,line in enumerate(result.stdout.splitlines()):save(f'{name}-{i}',json.loads(line))
print(f'Wrote {len(list(out.glob("*.json")))} backend fixtures to {out}')
