'use strict';
(() => {
 const data=JSON.parse(document.getElementById('editor-data').textContent),core=createExpansionEditorCore(data),files=createExpansionFiles(core);
 const $=id=>document.getElementById(id), roman=['','I','II','III','IV','V','VI','VII','VIII','IX','X'];
 const clone=core.clone, undo=[],redo=[],assets=new Map(),urls=new Map(),tierRarities=new Map();
 let project=core.newProject(),selected=null,pickedArt=null,db=null,saveTimer=null,dirty=false,revision=0,busy=false,lastEdit=null,conflict=false;
 const session=core.uid(),storedImages=new Map();let savedRevision=0,saveQueue=Promise.resolve();
 const current=()=>project.cards.find(c=>c.uid===selected);
 const meta=asset=>{const {blob,...entry}=asset;return entry;};
 const el=(tag,cls,text)=>{const node=document.createElement(tag);if(cls)node.className=cls;if(text!==undefined)node.textContent=text;return node;};
 const button=(text,action,cls='')=>{const b=el('button',cls,text);b.type='button';b.addEventListener('click',action);return b;};
 const option=(value,text)=>{const o=el('option','',text);o.value=value;return o;};
 const paragraph=(text,cls='hint')=>el('p',cls,text);
 function message(text,error=false){$('message').textContent=text;$('message').classList.toggle('error',error);$('message').hidden=!text;}
 function download(blob,name){const url=URL.createObjectURL(blob),a=el('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);}
 function assetURL(id){if(!urls.has(id)&&assets.has(id))urls.set(id,URL.createObjectURL(assets.get(id).blob));return urls.get(id)||data.cardBack;}
 function clearURLs(){for(const url of urls.values())URL.revokeObjectURL(url);urls.clear();}
 function modal(title){$('modal-title').textContent=title;$('modal-body').replaceChildren();if(!$('modal').open)$('modal').showModal();return $('modal-body');}
 function closeModal(){$('modal').close();}
 function progress(text,done,total){$('busy-label').textContent=text;if(total){$('busy-progress').max=total;$('busy-progress').value=done;}else $('busy-progress').removeAttribute('value');}
 async function run(label,fn){if(busy)return;busy=true;$('busy').hidden=false;progress(label);await files.yieldFrame();try{await fn();}catch(e){message(e.message,true);}finally{busy=false;$('busy').hidden=true;}}
 function history(fn,key=null){if(key===null||lastEdit!==key){undo.push(clone(project));if(undo.length>40)undo.shift();}redo.length=0;lastEdit=key;fn();markDirty();}
 function mutate(fn,{detail=true,key=null}={}){history(fn,key);renderRoster();renderCounts();renderArt();if(detail)renderInspector();else updateDetail();}
 function markDirty(){dirty=true;revision++;$('save-status').textContent='Saving draft…';clearTimeout(saveTimer);saveTimer=setTimeout(()=>queueSave(),400);}
 function openDB(){return new Promise((resolve,reject)=>{const req=indexedDB.open('tessera-expansion-workshop',1);req.onupgradeneeded=()=>{req.result.createObjectStore('state');req.result.createObjectStore('images');};req.onerror=()=>reject(req.error);req.onblocked=()=>reject(Error('Close another editor tab to open the draft.'));req.onsuccess=()=>resolve(req.result);});}
 function transactionDone(tx){return new Promise((resolve,reject)=>{tx.oncomplete=resolve;tx.onabort=tx.onerror=()=>reject(tx.error||Error('Draft save was interrupted.'));});}
 function queueSave(){
  clearTimeout(saveTimer);const snapshot=clone(project), atRevision=revision,images=[...assets.values()];
  saveQueue=saveQueue.catch(()=>{}).then(async()=>{
   if(!db||conflict){$('save-status').textContent=conflict?'Draft changed in another tab — save a project':'Browser storage unavailable — save a project';return;}
   const tx=db.transaction(['state','images'],'readwrite'),done=transactionDone(tx),store=tx.objectStore('state'),check=store.get('current');
   check.onsuccess=()=>{
    const existing=check.result;
    if(existing && existing.writer!==session && existing.revision!==savedRevision){conflict=true;tx.abort();return;}
    const next=(existing?.revision||0)+1;
    store.put({doc:snapshot,writer:session,revision:next},'current');
    // Only new images are written; typing a name must not rewrite a whole art library.
    const imageIDs=new Set(images.map(a=>a.id));
    for(const asset of images)if(storedImages.get(asset.id)!==asset.blob)tx.objectStore('images').put(asset,asset.id);
    for(const id of storedImages.keys())if(!imageIDs.has(id))tx.objectStore('images').delete(id);
    tx.addEventListener('complete',()=>{storedImages.clear();images.forEach(a=>storedImages.set(a.id,a.blob));savedRevision=next;if(atRevision===revision){dirty=false;$('save-status').textContent='Draft saved in this browser';}});
   };
   await done;
  }).catch(e=>{dirty=true;$('save-status').textContent=conflict?'Another tab changed this draft — save a project':'Could not save draft — save a project';message(conflict?'Another editor tab saved a newer draft. Save this project before reloading to avoid overwriting it.':'Draft storage failed: '+e.message+'. Use Save project to keep your work.',true);});
  return saveQueue;
 }
 async function restore(){
  try{
   db=await openDB();const tx=db.transaction(['state','images'],'readonly'),done=transactionDone(tx);
   const state=tx.objectStore('state').get('current'),images=tx.objectStore('images').getAll();
   await done;
   if(state.result){project=core.readProject(state.result.doc);savedRevision=state.result.revision;for(const a of images.result){assets.set(a.id,a);storedImages.set(a.id,a.blob);}}
   $('save-status').textContent='Draft saved in this browser';
  }catch(e){db?.close();db=null;message('Browser draft could not be restored: '+e.message+'. Open a saved project or start a new one.',true);$('save-status').textContent='Use Save project for a portable backup';}
  selected=project.cards[0]?.uid||null;showCards();render();
 }
 function emptySlot(c){return !c.name.trim()&&!c.id&&!c.asset&&!c.missingArt;}
 function renderCounts(){
  const count=project.cards.length,problems=core.issues(project),errors=problems.filter(p=>p.level==='error').length;
  $('roster-count').textContent=count+' card'+(count===1?'':'s');$('art-count').textContent=project.assets.length+' images';
  const placed=project.cards.filter(c=>c.asset).length;
  $('summary').textContent=count?placed+' with artwork · '+(count-placed)+' placeholders or missing images':'Choose a group, then drop artwork onto a tier to add a card.';
  $('review').textContent=errors?errors+' to review':'Ready to export';$('review').classList.toggle('danger',errors>0);
  $('undo').disabled=!undo.length;$('redo').disabled=!redo.length;
  $('remove-empty').hidden=!project.cards.some(emptySlot);
 }
 function render(){
  $('set-name').value=project.name;$('set-id').value=project.setId;$('balance').value=project.balance;
  renderCounts();renderArt();renderRoster();renderInspector();
 }
 function cropStyle(img,card){
  const a=assets.get(card.asset),ratio=a?a.width/a.height:.75,c=card.crop;
  const width=Math.max(100,ratio/.75*100)*c.zoom,height=width*.75/ratio;
  img.style.width=width+'%';img.style.height=height+'%';img.style.left=((100-width)*c.x/100)+'%';img.style.top=((100-height)*c.y/100)+'%';
 }
 function face(card){
  const node=el('div','card-face'+(card.asset?'':' placeholder'));node.style.setProperty('--rarity','var(--'+card.rarity.toLowerCase()+')');
  const img=el('img');img.alt='';img.src=card.asset?assetURL(card.asset):data.cardBack;img.draggable=false;cropStyle(img,card);node.append(img);
  node.append(el('span','card-tier',roman[card.tier]));
  const af=el('span','card-affinity');for(const name of card.affinities)af.append(affinityIcon(name));af.title=card.affinities.join(' / ')||'Neutral';node.append(af);
  ['top','right','bottom','left'].forEach((pos,i)=>node.append(el('span','rank '+pos,card.sides[i]===10?'A':String(card.sides[i]??''))));
  node.append(el('span','card-title',card.name||'New card'));return node;
 }
 function filtered(){return project.cards.filter(c=>(!$('group-filter').value||c.groups.includes($('group-filter').value))&&(!$('tier-filter').value||c.tier===Number($('tier-filter').value))&&(!($('card-search').value)||[c.name,c.id].join(' ').toLowerCase().includes($('card-search').value.toLowerCase())));}
 function renderRoster(){
  const root=$('roster'),scroll=root.scrollTop;root.replaceChildren();const shown=filtered(),problems=core.issues(project),bad=new Set(problems.filter(p=>p.level==='error').map(p=>p.uid));
  if(!shown.length&&$('card-search').value){const empty=el('div','empty-roster');empty.append(el('h2','','No matching cards'),button('Clear search',()=>{$('card-search').value='';renderRoster();}));root.append(empty);return;}
  const group=defaultGroup(),tiers=$('tier-filter').value?[Number($('tier-filter').value)]:[...new Set([...core.curve(group).map(row=>row.tier),...shown.map(c=>c.tier)])].sort((a,b)=>a-b);
  for(const tier of tiers){
   const cards=shown.filter(c=>c.tier===tier);if($('card-search').value&&!cards.length)continue;
   const section=el('section','tier-section');section.id='tier-'+tier;const header=el('div','tier-header');header.append(el('h2','','Tier '+roman[tier]));
   const counts=core.rarities.filter(r=>cards.some(c=>c.rarity===r)).map(r=>cards.filter(c=>c.rarity===r).length+' '+r.toLowerCase()).join(' · ');
   header.append(el('span','counts',counts));if(cards.length)header.append(button('Randomize row',()=>randomize(cards)));
   const rarity=el('select','tier-rarity'),rarityKey=group+':'+tier;for(const r of core.rarities)rarity.append(option(r,r));
   rarity.value=tierRarities.get(rarityKey)||core.suggestedRarity(tier,group);rarity.setAttribute('aria-label','Rarity for new tier '+roman[tier]+' card');
   rarity.addEventListener('change',()=>tierRarities.set(rarityKey,rarity.value));header.append(rarity);section.append(header);
   const slots=el('div','slots'+(cards.length?'':' empty-tier'));
   for(const c of cards){
    const slot=el('article','slot'+(selected===c.uid?' selected':''));slot.dataset.cardId=c.uid;
    const pick=button('',()=>{if(pickedArt){assign(c,pickedArt);pickedArt=null;renderArt();}else{if(selected!==c.uid)$('inspector').scrollTop=0;selected=c.uid;lastEdit=null;renderRoster();renderInspector();}},'slot-pick');pick.setAttribute('aria-label','Edit '+(c.name||'unnamed '+c.rarity+' card'));pick.append(face(c));
    const sm=el('div','slot-meta');sm.style.setProperty('--rarity','var(--'+c.rarity.toLowerCase()+')');sm.append(el('span','rarity',c.rarity+(c.unique?' · Unique':'')),el('span',bad.has(c.uid)?'slot-warning':'',bad.has(c.uid)?'!':core.ranks.cost(c)+' pt'));
    slot.append(pick,sm,el('div','slot-name',c.name||'Drop artwork or add a name'));
    const actions=el('div','slot-tools');actions.append(button('Randomize',()=>randomize([c])),button('Remove',()=>removeCard(c)));slot.append(actions);
    slot.addEventListener('dragover',e=>{e.preventDefault();slot.classList.add('drag-over');});slot.addEventListener('dragleave',()=>slot.classList.remove('drag-over'));
    slot.addEventListener('drop',e=>{e.preventDefault();slot.classList.remove('drag-over');const id=e.dataTransfer.getData('application/x-tessera-art');if(id&&assets.has(id))assign(c,id);else importArtwork([...e.dataTransfer.files],c.uid);});slots.append(slot);
   }
   const add=button('',()=>{if(pickedArt)createTierCard(tier,group,rarity.value,pickedArt);else addDialog(tier,group,rarity.value);},'add-card-target');
   add.setAttribute('aria-label','Add card to tier '+roman[tier]);add.disabled=project.cards.length>=512;
   add.append(el('span','add-card-symbol','+'),el('strong','','Add card'),el('span','hint','Drop artwork here'));
   add.addEventListener('dragover',e=>{e.preventDefault();if(!add.disabled)add.classList.add('drag-over');});add.addEventListener('dragleave',()=>add.classList.remove('drag-over'));
   add.addEventListener('drop',e=>{e.preventDefault();add.classList.remove('drag-over');if(add.disabled)return;const id=e.dataTransfer.getData('application/x-tessera-art');
    if(id&&assets.has(id))createTierCard(tier,group,rarity.value,id);else importArtwork([...e.dataTransfer.files],null,{tier,group,rarity:rarity.value});
   });slots.append(add);section.append(slots);root.append(section);
  }
  root.scrollTop=scroll;
 }
 function renderArt(){
  const root=$('art-list'),scroll=root.scrollTop;root.replaceChildren();const used=new Set(project.cards.map(c=>c.asset));
  for(const a of project.assets.filter(a=>a.name.toLowerCase().includes($('art-search').value.toLowerCase()))){
   const item=button('',()=>{pickedArt=pickedArt===a.id?null:a.id;renderArt();},'art-item'+(pickedArt===a.id?' selected':'')+(used.has(a.id)?' used':''));
   item.title=a.path;item.setAttribute('aria-label','Select artwork '+a.name);item.draggable=true;
   item.addEventListener('dragstart',e=>{e.dataTransfer.setData('application/x-tessera-art',a.id);e.dataTransfer.effectAllowed='copy';});
   const img=el('img');img.src=assetURL(a.id);img.alt='';img.loading='lazy';img.draggable=false;item.append(img,el('span','',a.name));root.append(item);
  }
  root.scrollTop=scroll;$('clear-art-selection').hidden=!pickedArt;
  $('art-hint').textContent=pickedArt?'Click Add card in a tier, or an existing card to replace its artwork.':'Drag artwork onto Add card in a tier, or onto an existing card.';
 }
 function displayName(name){return name.replace(/\.[^.]+$/,'').replace(/[-_]+/g,' ').replace(/\s+/g,' ').trim();}
 function placeArt(card,id){
  card.asset=id;card.placeholder=false;card.missingArt='';card.crop={x:50,y:50,zoom:1};
  if(!card.name){card.name=displayName(assets.get(id).name);if(!card.id)card.id=core.freshID(card.name,project.cards.filter(c=>c!==card));}
 }
 function tierCard(tier,group,rarity){return core.newCard(tier,rarity,group,project.cards.filter(c=>c.tier===tier&&c.groups.includes(group)&&c.rarity===rarity).length);}
 function createTierCard(tier,group,rarity,assetID){
  if(project.cards.length>=512){message('An expansion can contain up to 512 cards.',true);return;}
  const c=tierCard(tier,group,rarity);pickedArt=null;selected=c.uid;$('inspector').scrollTop=0;$('card-search').value='';
  mutate(()=>{placeArt(c,assetID);project.cards.push(c);});
 }
 function assign(card,id){
  if(!assets.has(id))return;selected=card.uid;
  mutate(()=>placeArt(card,id));
 }
 async function importArtwork(input,target=null,newSlot=null){
  const list=[...input].filter(f=>files.imageName(f.name||f.path));if(!list.length){message('Choose PNG, JPEG or WebP images.',true);return;}
  if((target||newSlot)&&list.length!==1){message('Drop one illustration onto a slot. Drop multiple images into the artwork panel.',true);return;}
  if(newSlot&&project.cards.length>=512){message('An expansion can contain up to 512 cards.',true);return;}
  await run('Importing artwork',async()=>{
   if(project.assets.length+list.length>2048)throw Error('The artwork library is limited to 2,048 images.');
   const imported=[],errors=[];let bytes=project.assets.reduce((n,a)=>n+(assets.get(a.id)?.blob.size||0),0);
   for(const [i,file] of list.entries()){progress('Importing '+file.name,i,list.length);await files.yieldFrame();try{
    const a=await files.importImage(file,file.webkitRelativePath||file.path||file.name);
    if(bytes+a.blob.size>500*1024*1024){errors.push('The artwork library reached 500 MiB. Remaining images were skipped; use a smaller folder or separate expansion.');break;}
    bytes+=a.blob.size;imported.push(a);
   }catch(e){errors.push(file.name+': '+e.message);}}
   if(imported.length){mutate(()=>{
    for(const a of imported){assets.set(a.id,a);project.assets.push(meta(a));}
    const c=newSlot?tierCard(newSlot.tier,newSlot.group,newSlot.rarity):project.cards.find(c=>c.uid===target);
    if(c){placeArt(c,imported[0].id);selected=c.uid;pickedArt=null;if(newSlot){project.cards.push(c);$('card-search').value='';}}
   });await queueSave();}
   message('Imported '+imported.length+' image'+(imported.length===1?'':'s')+'.'+(errors.length?' '+errors.slice(0,4).join(' ')+(errors.length>4?' '+(errors.length-4)+' more files could not be imported.':''):' Select an image and a slot, or use Match names.'),errors.length>0);
  });
 }
 function matchArt(){
  const normalized=s=>core.slug(displayName(s.split('/').pop())),matches=[],ambiguous=[];
  for(const c of project.cards.filter(c=>!c.asset)){
   const candidates=project.assets.filter(a=>c.missingArt?(a.path.endsWith(c.missingArt)||a.name===c.missingArt.split('/').pop()):[normalized(c.name||'no-name'),c.id].includes(normalized(a.name)));
   if(candidates.length===1)matches.push([c,candidates[0]]);else if(candidates.length>1)ambiguous.push(c.name||c.id);
  }
  if(matches.length)mutate(()=>matches.forEach(([c,a])=>{c.asset=a.id;c.placeholder=false;c.missingArt='';c.crop={x:50,y:50,zoom:1};}));
  message('Matched '+matches.length+' card'+(matches.length===1?'':'s')+'.'+(ambiguous.length?' Choose artwork manually for duplicate matches: '+ambiguous.join(', ')+'.':''));
 }
 function inputField(label,value,onInput,{id='',type='text',max=null}={}){
  const wrap=el('label','field',label),input=el('input');input.type=type;input.value=value;input.id=id;if(max)input.maxLength=max;
  input.addEventListener('input',()=>onInput(input.value));input.addEventListener('blur',()=>lastEdit=null);wrap.append(input);return [wrap,input];
 }
 function selectField(label,options,value,onChange,id=''){
  const wrap=el('label','field',label),select=el('select');select.id=id;for(const [v,t] of options)select.append(option(v,t));select.value=value;select.addEventListener('change',()=>onChange(select.value));wrap.append(select);return wrap;
 }
 function check(label,checked,onChange,id=''){
  const wrap=el('label','check'),input=el('input');input.type='checkbox';input.checked=checked;input.id=id;input.addEventListener('change',()=>onChange(input.checked));wrap.append(input,document.createTextNode(label));return wrap;
 }
 function fieldset(title){const set=el('fieldset');set.append(el('legend','',title));return set;}
 function updateDetail(){
  const c=current();if(!c)return;
  const preview=$('card-preview');if(preview)preview.replaceChildren(face(c));
  const note=$('strength-note'),limits=data.policy.tiers[c.tier-1],valid=core.ranks.validRanks(c);
  if(note){note.textContent='Weighted strength '+core.ranks.cost(c)+' / '+limits.weighted_min+'–'+limits.weighted_max+'. '+(valid?'Within tier rules.':'Outside tier rules.');note.classList.toggle('invalid',!valid);}
  if($('detail-issues')){const list=core.issues(project).filter(p=>p.uid===c.uid&&p.level==='error');$('detail-issues').replaceChildren(...list.map(p=>paragraph(p.message,'')));}
 }
 function renderInspector(){
  const root=$('inspector'),scroll=root.scrollTop,c=current();root.replaceChildren();
  if(!c){const empty=el('div','empty-inspector');empty.append(el('span','drop-symbol','◇'),el('h2','','Select a card'),paragraph('Choose a slot to edit its name, sides, affinity and acquisition.'));root.append(empty);return;}
  const preview=el('div','preview');preview.id='card-preview';root.append(preview);
  const edit=(fn,key)=>mutate(fn,{detail:false,key:c.uid+key});
  const [name,nameInput]=inputField('Card name',c.name,v=>edit(()=>c.name=v,'name'),{id:'card-name',max:128});nameInput.addEventListener('change',()=>{if(!c.id&&c.name.trim()){mutate(()=>c.id=core.freshID(c.name,project.cards.filter(o=>o!==c)),{detail:false});$('card-id').value=c.id;}});root.append(name);
  const [id]=inputField('Card ID',c.id,v=>edit(()=>c.id=v,'id'),{id:'card-id',max:64});root.append(id,paragraph('Keep this ID after release. Renaming the card does not change it.'));
  const row=el('div','fields-two');
  row.append(selectField('Tier',roman.slice(1).map((r,i)=>[i+1,r]),c.tier,v=>mutate(()=>c.tier=Number(v)),'card-tier'),selectField('Rarity',core.rarities.map(r=>[r,r]),c.rarity,v=>mutate(()=>c.rarity=v),'card-rarity'));root.append(row);
  root.append(check('Unique',c.unique,v=>mutate(()=>c.unique=v),'card-unique'));
  const strength=fieldset('Side values'),sides=el('div','sides');
  ['Top','Right','Bottom','Left'].forEach((label,i)=>{const [wrap,input]=inputField(label,c.sides[i],v=>edit(()=>c.sides[i]=v===''?0:Number(v),'side'+i),{id:'side-'+i,type:'number'});input.min=1;input.max=10;input.step=1;sides.append(wrap);});strength.append(sides);
  const note=el('p','strength-note');note.id='strength-note';strength.append(note);
  const actions=el('div','button-row');actions.append(button('Randomize',()=>randomize([c])),button('Rotate',()=>mutate(()=>c.sides=[c.sides[3],...c.sides.slice(0,3)])),button('Suggested values',()=>mutate(()=>{const s=core.newCard(c.tier,c.rarity,c.groups[0]||'Nord');c.sides=s.sides;})));strength.append(actions,paragraph('10 is printed as A. Values are always Top · Right · Bottom · Left.'));root.append(strength);
  const groups=fieldset('Groups'),groupChecks=el('div','checks group-checks');for(const g of data.groups)groupChecks.append(check(g,c.groups.includes(g),v=>mutate(()=>c.groups=v?[...c.groups,g]:c.groups.filter(x=>x!==g))));groups.append(groupChecks);root.append(groups);
  const affinities=fieldset('Affinity'),affChecks=el('div','checks');for(const a of data.workshopPolicy.affinities)affChecks.append(check(a,c.affinities.includes(a),v=>mutate(()=>c.affinities=v?[...c.affinities,a]:c.affinities.filter(x=>x!==a))));affinities.append(affChecks,paragraph('Leave all unchecked for Neutral.'));root.append(affinities);
  const art=fieldset('Artwork');
  if(c.asset){const a=assets.get(c.asset);art.append(paragraph(a?.name||'Missing image','image-name'));
   for(const [key,label,min,max,step] of [['x','Horizontal',0,100,1],['y','Vertical',0,100,1],['zoom','Zoom',1,3,.05]]){const wrap=el('label','crop-control',label),input=el('input');input.type='range';input.min=min;input.max=max;input.step=step;input.value=c.crop[key];input.id='crop-'+key;input.addEventListener('input',()=>edit(()=>c.crop[key]=Number(input.value),'crop'+key));input.addEventListener('change',()=>lastEdit=null);wrap.append(input);art.append(wrap);}
   art.append(button('Use card-back placeholder',()=>mutate(()=>{c.asset=null;c.placeholder=true;c.missingArt='';c.crop={x:50,y:50,zoom:1};}),'text-button'));
  }else{art.append(paragraph(c.placeholder?'Using the Tessera card back. Add artwork whenever it is ready.':'Missing: '+c.missingArt));if(!c.placeholder)art.append(button('Use card-back placeholder',()=>mutate(()=>{c.placeholder=true;c.missingArt='';}),'text-button'));}
  art.append(paragraph('Drop an image onto this card, or select one in the artwork panel.'));root.append(art);
  const acquisition=fieldset('Acquisition'),a=c.acquisition;
  acquisition.append(check('Standard packs · tiers I–VI',a.standardPacks,v=>mutate(()=>a.standardPacks=v),'standard-packs'),check('Tournament packs · tiers III–VII',a.tournamentPacks,v=>mutate(()=>a.tournamentPacks=v),'tournament-packs'));
  acquisition.append(selectField('Card traders',[['none','None'],['regular','Regular'],['specialist','Specialist'],['all','All traders']],a.merchants,v=>mutate(()=>a.merchants=v),'merchants'));
  acquisition.append(paragraph('Tournament trophies · unique Legendary'));
  for(const [key,label] of [['invitational','Invitational'],['masters','Masters']])acquisition.append(check(label,a.tournamentPrizes.includes(key),v=>mutate(()=>a.tournamentPrizes=v?[...a.tournamentPrizes,key]:a.tournamentPrizes.filter(x=>x!==key)),'prize-'+key));
  acquisition.append(button('Use suggested acquisition',()=>mutate(()=>c.acquisition=core.suggestedRoutes(c)),'text-button'),paragraph('With no routes selected, supply the card through an NPC assignment or quest reward.'));root.append(acquisition);
  const errors=el('div','card-issues');errors.id='detail-issues';root.append(errors,button('Remove card',()=>removeCard(c),'text-button danger'));
  updateDetail();root.scrollTop=scroll;
 }
 function randomize(cards){
  mutate(()=>{const targets=new Set(cards.map(c=>c.uid)),used=new Set(project.cards.filter(c=>c.tier===cards[0]?.tier&&!targets.has(c.uid)).map(c=>c.sides.join()));
   for(const c of [...cards].sort((a,b)=>core.ranks.randomProfiles(a).length-core.ranks.randomProfiles(b).length)){const sides=core.ranks.pickRandomSides(c,used);if(sides)c.sides=sides;used.add(c.sides.join());}});
 }
 function removeCard(c){mutate(()=>{project.cards=project.cards.filter(x=>x!==c);if(selected===c.uid)selected=project.cards[0]?.uid||null;});message('Removed '+(c.name||'slot')+'. Undo restores it.');}
 function defaultGroup(){return $('group-filter').value||current()?.groups[0]||'Dark Elf';}
 function showCards(group=current()?.groups[0]||'Dark Elf'){ $('card-search').value='';$('tier-filter').value='';$('group-filter').value=group;}
 function addDialog(tier,group=defaultGroup(),rarity=null){
  const body=modal('Add a card');let t=Number(tier)||Number($('tier-filter').value)||current()?.tier||(group==='God'?10:group==='Dragon'?6:1),r=rarity||core.suggestedRarity(t,group),name='';
  body.append(selectField('Group',data.groups.map(g=>[g,g]),group,v=>group=v,'new-group'),selectField('Tier',roman.slice(1).map((x,i)=>[i+1,x]),t,v=>t=Number(v),'new-tier'),selectField('Rarity',core.rarities.map(x=>[x,x]),r,v=>r=v,'new-rarity'));
  const [nameField]=inputField('Name (you can add it later)',name,v=>name=v,{id:'new-card-name'});body.append(nameField);
  const actions=el('div','button-row');actions.append(button('Cancel',closeModal),button('Add card',()=>{if(project.cards.length>=512){message('An expansion can contain up to 512 cards.',true);return;}const c=tierCard(t,group,r);c.name=name.trim();if(c.name)c.id=core.freshID(c.name,project.cards);closeModal();$('card-search').value='';$('group-filter').value=group;if(Number($('tier-filter').value)!==t)$('tier-filter').value='';selected=c.uid;mutate(()=>project.cards.push(c));$('tier-'+t)?.scrollIntoView({block:'nearest'});},'primary'));body.append(actions);
 }
 function review(exporting=false){
  const body=modal(exporting?'Export expansion':'Review expansion'),issues=core.issues(project),errors=issues.filter(i=>i.level==='error'),warnings=issues.filter(i=>i.level==='warning');
  body.append(paragraph(project.cards.length+' cards · '+project.cards.filter(c=>c.asset).length+' illustrations · '+project.cards.filter(c=>!c.asset&&c.placeholder).length+' card-back placeholders.'));
  body.append(paragraph(errors.length?errors.length+' issue'+(errors.length===1?'':'s')+' to resolve before export.':warnings.length?'Ready to export. Review the notes below.':'Ready to export.'));
  if(issues.length){const list=el('div','review-list');for(const issue of issues){const b=button('',()=>{if(issue.uid){selected=issue.uid;showCards();closeModal();renderRoster();renderInspector();document.querySelector('[data-card-id="'+selected+'"]')?.scrollIntoView({block:'nearest'});}else{closeModal();$('set-name').focus();}},'review-item '+issue.level);b.append(el('strong','',issue.uid?issue.name:'Expansion'),document.createTextNode(issue.message));list.append(b);}body.append(list);}
  if(exporting){body.append(paragraph('The ZIP contains only this expansion’s JSON and card art, in the folders expected by MO2, Vortex and Skyrim. Requires Tessera and its Card Art package.'));
   body.append(el('code','','SKSE/Plugins/Tessera/Expansions/'+(project.setId||'set-id')+'.json'),el('br'),el('code','','TesseraUI/art/expansions/'+(project.setId||'set-id')+'/'));
   const actions=el('div','button-row');actions.append(button('Back',closeModal));const exportButton=button('Download expansion ZIP',()=>{closeModal();run('Preparing expansion',async()=>{const blob=await files.exportMod(project,assets,progress);download(blob,project.setId+'.zip');message('Exported '+project.cards.length+' cards. Install the ZIP as a separate mod and restart Skyrim.');});},'primary');exportButton.id='download-expansion';exportButton.disabled=errors.length>0;actions.append(exportButton);body.append(actions);
  }
 }
 function saveProject(){run('Saving project',async()=>{const blob=await files.exportProject(project,assets,progress);download(blob,(project.setId||'tessera-expansion')+'.tessera-project.zip');message('Project backup saved, including artwork and unfinished slots.');});}
 function confirmReplace(title,action){if(!project.cards.length&&!project.assets.length&&!project.name){action();return;}const body=modal(title);body.append(paragraph('This replaces the draft in this browser. Save a project first if you want to keep the current work.'));const actions=el('div','button-row');actions.append(button('Cancel',closeModal),button('Save current project',saveProject),button('Replace draft',()=>{closeModal();action();},'primary'));body.append(actions);}
 function openDialog(){const body=modal('Open an expansion or project');body.append(paragraph('Open a project backup to restore your slots, artwork and crops. You can also edit an existing expansion ZIP or its JSON manifest.'),paragraph('Opening JSON alone preserves its art references. Import the matching images afterwards, or choose the complete mod folder.'));
  const actions=el('div','button-row');actions.append(button('Choose mod folder',()=>{$('expansion-folder').click();}),button('Choose ZIP or JSON',()=>{$('project-files').click();},'primary'));body.append(actions);}
 async function importProject(list){
  let result=null;await run('Opening project',async()=>{result=await files.readImport(list,progress);});if(!result)return;
  confirmReplace('Replace the current draft?',()=>{
   clearURLs();assets.clear();for(const [id,a] of result.assets)assets.set(id,a);project=result.project;selected=project.cards[0]?.uid||null;pickedArt=null;undo.length=0;redo.length=0;lastEdit=null;showCards();markDirty();render();
   message('Opened '+project.name+'.'+(result.missing?' '+result.missing+' images still need importing. Use Match names after adding them.':''));
  });
 }
 function guide(){const body=modal('Expansion Workshop');const list=el('ol','guide-list');
  for(const line of ['Name your expansion and give it a distinctive set ID. Keep set and card IDs unchanged after release.','Choose a group to see its empty tier rows. Drag artwork onto Add card in a tier to create one card, or click the target to add a card without art. The row’s rarity selector sets its rarity.','Choose an artwork folder or drop PNG, JPG or WebP images into the left panel. Drag an illustration onto Add card or an existing card. Match names links images to existing names or IDs; ambiguous matches are left for you.','Edit names, sides, groups, affinities and acquisition. Adjust the crop in the card editor. No artwork yet? Keep the card-back placeholder.','Use Save project for a portable backup, including unfinished slots and artwork. Open it on another computer to continue. Browser autosaves are local to this browser and address.','Export expansion creates an installable ZIP with the manifest, 768×1024 WebP faces and 240×320 thumbnails. Install it as a separate mod beside Tessera and Card Art, then restart Skyrim.'])list.append(el('li','',line));body.append(list,paragraph('Tessera tier rules blocks export when side values break the official budgets or placement constraints. Custom values permits them with a warning; runtime limits still apply. Only cards you add are counted and exported. Remove empty slots clears unused blank slots in an existing draft; Undo restores them.'),paragraph('Acquisition follows Tessera’s existing buckets. Unique Legendary cards can enter Invitational and Masters trophy pools. Cards without a route need a separate NPC or quest integration.'),paragraph('Imported illustrations are stored up to 2048 pixels on their longest side. Keep your original art files separately. No uploads, accounts or network connection are required.'));
 }
 function historyStep(from,to){if(!from.length||busy)return;to.push(clone(project));project=from.pop();selected=project.cards.some(c=>c.uid===selected)?selected:project.cards[0]?.uid||null;lastEdit=null;markDirty();render();}
 for(const g of data.groups)$('group-filter').append(option(g,g));for(let t=1;t<=10;t++)$('tier-filter').append(option(t,'Tier '+roman[t]));
 $('set-name').addEventListener('input',()=>mutate(()=>project.name=$('set-name').value,{detail:false,key:'set-name'}));
 $('set-name').addEventListener('change',()=>{lastEdit=null;if(!project.setId&&project.name.trim()){mutate(()=>project.setId=core.slug(project.name),{detail:false});$('set-id').value=project.setId;}});
 $('set-id').addEventListener('input',()=>mutate(()=>project.setId=$('set-id').value,{detail:false,key:'set-id'}));$('set-id').addEventListener('blur',()=>lastEdit=null);
 $('balance').addEventListener('change',()=>mutate(()=>project.balance=$('balance').value));
 for(const id of ['card-search','tier-filter'])$(id).addEventListener('input',renderRoster);
 $('group-filter').addEventListener('input',()=>{const group=$('group-filter').value;selected=project.cards.find(c=>c.groups.includes(group))?.uid||null;showCards(group);$('roster').scrollTop=0;$('inspector').scrollTop=0;renderRoster();renderInspector();});
 $('art-search').addEventListener('input',renderArt);$('clear-art-selection').onclick=()=>{pickedArt=null;renderArt();};
 $('add-card').onclick=()=>addDialog();$('add-art').onclick=()=>$('art-files').click();$('add-folder').onclick=()=>$('art-folder').click();$('match-art').onclick=matchArt;
 $('remove-empty').onclick=()=>{const count=project.cards.filter(emptySlot).length;mutate(()=>{project.cards=project.cards.filter(c=>!emptySlot(c));if(!current())selected=project.cards.find(c=>c.groups.includes(defaultGroup()))?.uid||null;});message('Removed '+count+' empty slots. Undo restores them.');};
 for(const id of ['art-files','art-folder'])$(id).addEventListener('change',e=>{importArtwork(e.target.files);e.target.value='';});
 for(const id of ['project-files','expansion-folder'])$(id).addEventListener('change',e=>{const list=[...e.target.files];e.target.value='';if(list.length){closeModal();importProject(list);}});
 $('art-drop').addEventListener('dragover',e=>{e.preventDefault();$('art-drop').classList.add('drag-over');});$('art-drop').addEventListener('dragleave',()=>$('art-drop').classList.remove('drag-over'));
 $('art-drop').addEventListener('drop',async e=>{e.preventDefault();$('art-drop').classList.remove('drag-over');try{const list=await files.droppedFiles(e.dataTransfer);await importArtwork(list);}catch(error){message(error.message,true);}});
 window.addEventListener('dragover',e=>e.preventDefault());window.addEventListener('drop',e=>e.preventDefault());
 $('undo').onclick=()=>historyStep(undo,redo);$('redo').onclick=()=>historyStep(redo,undo);$('review').onclick=()=>review();$('export').onclick=()=>review(true);$('save-project').onclick=saveProject;$('open-project').onclick=openDialog;
 $('new-project').onclick=()=>confirmReplace('Start a new expansion?',()=>{clearURLs();assets.clear();project=core.newProject();selected=null;pickedArt=null;undo.length=redo.length=0;showCards();markDirty();render();message('');});
 $('help').onclick=guide;$('close-modal').onclick=closeModal;
 document.addEventListener('keydown',e=>{if(!(e.ctrlKey||e.metaKey)||busy)return;if(e.key.toLowerCase()==='s'){e.preventDefault();saveProject();}if(e.key.toLowerCase()==='z'&&!['INPUT','TEXTAREA'].includes(document.activeElement?.tagName)){e.preventDefault();e.shiftKey?historyStep(redo,undo):historyStep(undo,redo);}});
 window.addEventListener('beforeunload',e=>{if(dirty||busy){e.preventDefault();e.returnValue='';}});
 document.addEventListener('visibilitychange',()=>{if(document.hidden&&dirty)queueSave();});
 restore();
})();
