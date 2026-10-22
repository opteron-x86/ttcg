'use strict';
// The fixture is embedded by preview-themes.cjs. This page never connects to SKSE.
const previewDefaults=Object.fromEntries(interfaceThemes.map(t=>[t.id,t.surface]));
const previewParams=new URLSearchParams(location.search);
let previewPrefs={theme:previewParams.get('theme')||'oxblood',scene:previewParams.get('scene')||'match',surface:'theme',grain:10,empty:false,fullscreen:false};
let previewSession=20,previewSceneKey='';
const idFor=(name,foil=false)=>previewFixture.cards.findIndex(c=>c.name===name&&Boolean(c.foil)===foil);
const handNames=[['Assembled Sanitizer','Moonmoth Castellan','Mistveil Enchanter','Wind Keep Spellsword','Corsair'],['Septim Guardsman','Brutal Ashlander','Dres Spy','Dune Smuggler','Dwarven Scarab']];
const previewHands=handNames.map(names=>names.map(name=>idFor(name)));
const previewOwned=['Abecean Navigator','Ambitious Hireling','Arcanaeum Librarian','Brutal Ashlander','Clannfear Runt','Corsair','Craglorn Scavenger','Craven Conscript','Crucible Blacksmith','Deathless Draugr','Dres Spy','Dune Smuggler','Dwarven Scarab','Dwarven Spider','Dust Eater Skirmisher','Fighters Guild Recruit','Frostbite Spider','Guar Stablemaster','Morthal Watchman','Murkwater Goblin','Septim Guardsman','Sharpshooter Scout','Sunhold Medic',...handNames.flat()];
previewFixture.collection.owned=previewFixture.cards.map(()=>0);
previewFixture.collection.known=previewFixture.cards.map(()=>0);
for(const name of new Set(previewOwned)){const i=idFor(name);if(i>=0){previewFixture.collection.owned[i]=i%4===0?2:1;previewFixture.collection.known[i]=1;}}
previewFixture.collection.packs=[2,1];
previewFixture.collection.deck=previewHands[0];
previewFixture.collection.savedDecks=[{id:1,name:'Everyday hand',cards:previewHands[0]}];
previewFixture.collection.fixedRules=16|2;
previewFixture.collection.fixedTrade=2;
previewFixture.musicEnabled=true;
previewFixture.musicAvailable=true;
previewFixture.opponent='Hulda';
previewFixture.settings={showCardNames:true,rankLayout:0,nameplateStyle:0,nameCase:0,rankShadowOpacity:100,rankShadowBlur:2,rankShadowOffset:1,rankStroke:false,rankStrokeWeight:10};
function showPreview(next){
 const prior=previewPrefs;previewPrefs={...previewPrefs,...next};
 if(!Object.hasOwn(previewDefaults,previewPrefs.theme))previewPrefs.theme='oxblood';
 if(!['match','album','lobby','settings','board'].includes(previewPrefs.scene))previewPrefs.scene='match';
 if(!['theme','plain','slate','felt','parchment','leather','walnut'].includes(previewPrefs.surface))previewPrefs.surface='theme';
 const root=document.documentElement,p=previewPrefs;
 root.dataset.scene=p.scene;
 const appearance={theme:interfaceThemes.findIndex(t=>t.id===p.theme),boardSurface:boardSurfaces.findIndex(s=>s.id===p.surface),boardTexture:p.grain};
 Object.assign(previewFixture.settings,appearance);
 if(state)Object.assign(state.settings,appearance);
 const sceneKey=p.scene+':'+p.empty;
 if(sceneKey!==previewSceneKey){
  previewSceneKey=sceneKey;
  const packet=structuredClone(previewFixture);packet.session=++previewSession;
  packet.phase=['match','board'].includes(p.scene)?'match':'ready';packet.screen=['album','settings'].includes(p.scene)?'album':'lobby';packet.matchContext=false;
  packet.rules=16|2;packet.opening=false;packet.settling=false;packet.thinking=false;packet.turn=0;
  packet.fullHands=structuredClone(previewHands);packet.originalHands=structuredClone(previewHands);packet.hands=structuredClone(previewHands);packet.board=Array(9).fill(null);
  if(packet.phase==='match'&&!p.empty){
   packet.board[0]={card:packet.hands[0][1],owner:0};packet.hands[0][1]=null;
   packet.board[4]={card:packet.hands[1][1],owner:1};packet.hands[1][1]=null;
   packet.board[7]={card:packet.hands[1][4],owner:1};packet.hands[1][4]=null;
  }
  albumTab=p.scene==='settings'?'settings':'cards';
  ttcgState(packet);
  if(p.scene==='album'){inspected=idFor('Dres Spy');renderAlbumInspection();}
  if(packet.phase==='match'){selected=0;render();}
  document.activeElement?.blur();
 }
 if(prior.fullscreen!==p.fullscreen||$('window-size').value!==(p.fullscreen?'100':'95')){
  $('window-size').value=p.fullscreen?'100':'95';$('window-size')._refresh?.();resizeWindow();
 }
 applyThemeSettings();renderThemeSettings();
 document.title='Tessera — '+p.theme+' / '+p.scene;
 window.previewReady=true;
}
window.ttcgCommand=command=>{
 const [verb,,,action,value]=command.split(' ');
 if(verb==='browse'){showPreview({scene:action==='album'?'album':'lobby'});return;}
 const field=verb==='settings'&&[...appearanceFontFields,...themeFields,{id:'board-texture',key:'boardTexture'}].find(f=>(f.command||f.id)===action);
 if(field){
  if(field.key==='theme')previewPrefs.theme=interfaceThemes[Number(value)].id;
  if(field.key==='boardSurface')previewPrefs.surface=boardSurfaces[Number(value)].id;
  if(field.key==='boardTexture')previewPrefs.grain=Number(value);
  previewFixture.settings[field.key]=Number(value);
  ttcgState({...state,revision:state.revision+1,settings:{...state.settings,[field.key]:Number(value)}});return;
 }
 // The live DOM remains useful for inspection, navigation, and menu review.
 // Native mutations are discarded; no game or save data exists in this page.
 queueMicrotask(()=>{sent=false;busy=false;render();});
};
window.addEventListener('message',event=>{if(event.source===window.parent&&event.data?.type==='tessera-theme-preview')showPreview(event.data.prefs);});
showPreview(previewPrefs);
window.parent.postMessage({type:'tessera-preview-ready'},'*');
