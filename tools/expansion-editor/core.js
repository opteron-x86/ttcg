'use strict';
function createExpansionEditorCore(data, rankFactory = createTesseraRankTools) {
 const ranks = rankFactory(data), rarities = ['Common','Rare','Epic','Legendary'];
 const clone = value => JSON.parse(JSON.stringify(value));
 const utf8 = value => new TextEncoder().encode(value).length;
 const identifier = value => typeof value === 'string' && /^[a-z][a-z0-9_-]{0,63}$/.test(value);
 const textOK = (value,max) => typeof value === 'string' && value.length > 0 && utf8(value) <= max && value === value.trim() && !/[\x00-\x1f\x7f]/.test(value) && !/[\uD800-\uDBFF](?![\uDC00-\uDFFF])|(?<![\uD800-\uDBFF])[\uDC00-\uDFFF]/.test(value);
 const reservedFilename = value => /^(con|prn|aux|nul|com[1-9]|lpt[1-9])$/i.test(value);
 const slug = value => value.normalize('NFKD').replace(/[\u0300-\u036f]/g,'').toLowerCase().replace(/['’]/g,'').replace(/[^a-z0-9]+/g,'-').replace(/^-+|-+$/g,'').replace(/^[^a-z]+/,'').slice(0,64).replace(/-+$/,'') || 'card';
 const uid = () => Array.from(crypto.getRandomValues(new Uint8Array(12)),v=>v.toString(16).padStart(2,'0')).join('');
 function freshID(name,cards) {
  const base = slug(name), used = new Set(cards.map(c=>c.id)); let id = base, n = 2;
  while (used.has(id)) {const tail = '-' + n++; id = base.slice(0,64-tail.length) + tail;}
  return id;
 }
 function routes(card) {
  const ordinary = !card.unique && card.rarity !== 'Legendary';
  return {standardPacks:ordinary && card.tier <= 6,tournamentPacks:ordinary && card.tier >= 3 && card.tier <= 7,
   merchants:ordinary && card.tier <= 7,tournamentPrizes:card.unique && card.rarity === 'Legendary'};
 }
 function suggestedRoutes(card) {
  const r = routes(card);
  return {standardPacks:r.standardPacks,tournamentPacks:r.tournamentPacks,merchants:r.merchants?'all':'none',
   tournamentPrizes:r.tournamentPrizes?[card.tier <= 8?'invitational':'masters']:[]};
 }
 function newCard(tier,rarity,group,index = 0) {
  const prepared = data.workshopPolicy.profiles[tier]?.[rarity];
  const sides = prepared?.length ? prepared[index % prepared.length] : ranks.randomProfiles({tier,rarity})[0];
  if (!sides) throw Error('No profile is available for that tier and rarity.');
  const card = {uid:uid(),id:'',name:'',tier,rarity,sides:[...sides],groups:[group],
   affinities:[data.workshopPolicy.affinities[(data.groups.indexOf(group)+tier+index)%5]],unique:rarity==='Legendary',
   asset:null,placeholder:true,crop:{x:50,y:50,zoom:1},missingArt:''};
  if (group === 'God') card.affinities = [];
  card.acquisition = suggestedRoutes(card); return card;
 }
 function curve(group) {
  if (group === 'God') return [{tier:9,Legendary:3},{tier:10,Legendary:2}];
  if (group === 'Dragon') return data.dragonCurve;
  return data.workshopPolicy.ordinaryCurve;
 }
 function suggestedRarity(tier,group) {
  if(group==='God'||tier>=9||(group!=='Dragon'&&tier>=8))return 'Legendary';
  return group==='Dragon'?(tier>=8?'Epic':'Rare'):'Common';
 }
 function artPath(path,setId) {
  return typeof path === 'string' && utf8(path) <= 240 && /^[A-Za-z0-9/_.-]+$/.test(path) && path.startsWith('expansions/'+setId+'/') &&
   path.split('/').every(p=>p && p!=='.' && p!=='..' && !p.endsWith('.')) && /\.(png|webp|jpg|jpeg)$/.test(path);
 }
 function fields(value,keys,label) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw Error(label+' must be an object.');
  const unknown = Object.keys(value).filter(k=>!keys.includes(k));
  if (unknown.length) throw Error(label+': unsupported field '+unknown[0]+'.');
 }
 function cardErrors(card,setId) {
  const errors = [], check = (ok,msg) => {if(!ok)errors.push(msg);};
  check(identifier(card.id),'Use a card ID starting with a lowercase letter, up to 64 letters, digits, - or _.');
  check(textOK(card.name,128),'Enter a card name (up to 128 UTF-8 bytes, without surrounding spaces).');
  check(Number.isInteger(card.tier) && card.tier>=1 && card.tier<=10,'Tier must be an integer from 1 to 10.');
  check(rarities.includes(card.rarity),'Choose a supported rarity.');
  check(Array.isArray(card.sides) && card.sides.length===4 && card.sides.every(v=>Number.isInteger(v)&&v>=1&&v<=10),'Sides must be four integers from 1 to 10.');
  for (const [key,allowed,min] of [['groups',data.groups,1],['affinities',data.workshopPolicy.affinities,0]])
   check(Array.isArray(card[key]) && card[key].length>=min && card[key].every(v=>allowed.includes(v)) && new Set(card[key]).size===card[key].length,'Choose '+(min?'at least one group':'valid affinities')+' without duplicates.');
  check(typeof card.unique==='boolean','Unique must be true or false.');
  for (const key of ['art','thumbnail']) if (key in card) check(artPath(card[key],setId),'Invalid '+key+' path: use expansions/'+setId+'/ and a PNG, WebP or JPEG filename.');
  if ('acquisition' in card) {
   const a = card.acquisition;
   try {fields(a,['standardPacks','tournamentPacks','merchants','tournamentPrizes'],'Acquisition');} catch(e) {errors.push(e.message);return errors;}
   for (const key of ['standardPacks','tournamentPacks']) if(key in a) check(typeof a[key]==='boolean',key+' must be true or false.');
   if ('merchants' in a) check(['none','regular','specialist','all'].includes(a.merchants),'Choose a supported merchant route.');
   if ('tournamentPrizes' in a) {
    check(Array.isArray(a.tournamentPrizes) && a.tournamentPrizes.every(p=>['invitational','masters'].includes(p)) && new Set(a.tournamentPrizes).size===a.tournamentPrizes.length,'Choose distinct Invitational or Masters prizes.');
    if (a.tournamentPrizes?.length) check(card.unique && card.rarity==='Legendary','Tournament trophies must be unique Legendary cards.');
   }
   if (card.unique) check(!a.standardPacks&&!a.tournamentPacks&&(!a.merchants||a.merchants==='none'),'Unique cards cannot enter packs or merchant stock.');
  }
  return errors;
 }
 function validateManifest(doc) {
  fields(doc,['schemaVersion','setId','name','cards'],'Manifest');
  if (doc.schemaVersion!==1) throw Error('Expected expansion schemaVersion 1.');
  if (!identifier(doc.setId) || doc.setId==='tessera') throw Error('Invalid or reserved set ID.');
  if (!textOK(doc.name,96)) throw Error('Enter a set name (up to 96 UTF-8 bytes, without surrounding spaces).');
  if (!Array.isArray(doc.cards) || !doc.cards.length || doc.cards.length>512) throw Error('An expansion must contain 1–512 cards.');
  const used = new Set();
  for (const [i,c] of doc.cards.entries()) {
   const label = c?.name || 'Card '+(i+1);
   fields(c,['id','name','tier','rarity','sides','groups','affinities','unique','art','thumbnail','acquisition'],label);
   const errors = cardErrors(c,doc.setId);
   if (errors.length) throw Error(label+': '+errors[0]);
   if (used.has(c.id)) throw Error('Duplicate card ID: '+c.id); used.add(c.id);
  }
  if (utf8(JSON.stringify(doc))>2*1024*1024) throw Error('The manifest exceeds 2 MiB.');
  return doc;
 }
 function parseJSON(text) {
  // JSON.parse accepts duplicate keys; Tessera rejects them. Check before import
  // can silently discard the first value. JSON.parse handles syntax and strings.
  const doc = JSON.parse(text), tokens = text.match(/"(?:\\.|[^"\\])*"|[{}\[\]:]/g) || [], stack = [];
  for (let i=0;i<tokens.length;i++) {
   const token = tokens[i];
   if (token==='{' || token==='[') {stack.push(token==='{'?new Set():null);if(stack.length>16)throw Error('JSON exceeds 16 nesting levels.');}
   else if (token==='}' || token===']') stack.pop();
   else if (token[0]==='"' && tokens[i+1]===':') {
    const key = JSON.parse(token), keys = stack.at(-1);
    if(keys?.has(key))throw Error('Duplicate JSON property: '+key);keys?.add(key);
   }
  }
  return doc;
 }
 function newProject() {return {kind:'tessera-expansion-project',schema:1,setId:'',name:'',balance:'tessera',cards:[],assets:[]};}
 function fromManifest(doc) {
  validateManifest(doc);
  return {...newProject(),setId:doc.setId,name:doc.name,cards:doc.cards.map(c=>{
   const result = {...clone(c),uid:uid(),asset:null,placeholder:!c.art,missingArt:c.art||'',crop:{x:50,y:50,zoom:1}};
   result.acquisition = {standardPacks:false,tournamentPacks:false,merchants:'none',tournamentPrizes:[],...c.acquisition};
   delete result.art; delete result.thumbnail; return result;
  })};
 }
 function manifest(project) {
  return {schemaVersion:1,setId:project.setId,name:project.name,cards:project.cards.map(c=>{
   const result = Object.fromEntries(['id','name','tier','rarity','sides','groups','affinities','unique','acquisition'].map(k=>[k,clone(c[k])]));
   if (c.asset) {const base='expansions/'+project.setId+'/';result.art=base+'faces/'+c.id+'.webp';result.thumbnail=base+'thumbs/'+c.id+'.webp';}
   return result;
  })};
 }
 function issues(project) {
  const list = [], add = (level,message,card=null)=>list.push({level,message,uid:card?.uid||null,name:card?.name||'Unnamed card'});
  if (!identifier(project.setId)||project.setId==='tessera') add('error','Give the expansion a unique set ID (lowercase letters, digits, - or _; start with a letter).');
  if (reservedFilename(project.setId)) add('error','This set ID is a reserved Windows filename. Choose another ID.');
  if (!textOK(project.name,96)) add('error','Enter an expansion name (up to 96 UTF-8 bytes).');
  if (!project.cards.length || project.cards.length>512) add('error','An expansion needs 1–512 cards.');
  const ids = new Set(), assets = new Set(project.assets.map(a=>a.id));
  for (const c of project.cards) {
   const errors = cardErrors(c,project.setId); errors.forEach(msg=>add('error',msg,c));
   if(c.id && ids.has(c.id))add('error','Another card has this ID.',c);ids.add(c.id);
   if (c.asset && reservedFilename(c.id)) add('error','This card ID is a reserved Windows filename. Choose another ID before packaging artwork.',c);
   if (c.asset && !assets.has(c.asset)) add('error','The assigned artwork is missing. Import it again or choose the card-back placeholder.',c);
   if (!c.asset && !c.placeholder) add('error','Missing artwork: '+(c.missingArt||'import an image or choose a placeholder')+'.',c);
   if(!errors.length && !ranks.validRanks(c))add(project.balance==='tessera'?'error':'warning','Side values are outside the Tessera tier budget or placement rules.',c);
   const r = routes(c), a = c.acquisition;
   for (const key of ['standardPacks','tournamentPacks','merchants']) if ((key==='merchants'?a[key]!=='none':a[key]) && !r[key]) add('warning','This tier/rarity cannot appear in '+({standardPacks:'standard packs',tournamentPacks:'tournament packs',merchants:'merchant stock'}[key])+'.',c);
   if (!(a.standardPacks&&r.standardPacks) && !(a.tournamentPacks&&r.tournamentPacks) && !(a.merchants!=='none'&&r.merchants) && !a.tournamentPrizes.length) add('warning','No automatic acquisition: assign this card to an NPC or a quest reward.',c);
  }
  return list;
 }
 function readProject(doc) {
  if (!doc || doc.kind!=='tessera-expansion-project' || doc.schema!==1) throw Error('Unsupported editor project.');
  if (typeof doc.setId!=='string'||typeof doc.name!=='string'||!['tessera','custom'].includes(doc.balance)||!Array.isArray(doc.cards)||doc.cards.length>512||!Array.isArray(doc.assets)||doc.assets.length>2048) throw Error('Invalid project structure.');
  const ids = new Set(), assets = new Set();
  for (const a of doc.assets) {
   if(!/^[a-f0-9]{24}$/.test(a.id)||assets.has(a.id)||typeof a.name!=='string'||typeof a.path!=='string'||!Number.isFinite(a.width)||!Number.isFinite(a.height))throw Error('Invalid artwork entry.');assets.add(a.id);
  }
  for (const c of doc.cards) {
   if(!/^[a-f0-9]{24}$/.test(c.uid)||ids.has(c.uid))throw Error('Invalid or repeated editor card identity.');ids.add(c.uid);
   if(typeof c.id!=='string'||typeof c.name!=='string'||!Number.isInteger(c.tier)||c.tier<1||c.tier>10||!rarities.includes(c.rarity)||!Array.isArray(c.sides)||c.sides.length!==4||!c.sides.every(Number.isFinite)||!Array.isArray(c.groups)||!c.groups.every(v=>data.groups.includes(v))||!Array.isArray(c.affinities)||!c.affinities.every(v=>data.workshopPolicy.affinities.includes(v))||typeof c.unique!=='boolean')throw Error('Invalid card in project.');
   if(c.asset!==null&&!assets.has(c.asset))throw Error('Project refers to missing artwork.');
   if(typeof c.placeholder!=='boolean'||typeof c.missingArt!=='string'||!c.crop||!['x','y','zoom'].every(k=>Number.isFinite(c.crop[k]))||c.crop.x<0||c.crop.x>100||c.crop.y<0||c.crop.y>100||c.crop.zoom<1||c.crop.zoom>3)throw Error('Invalid artwork placement.');
   // Draft names, IDs and ranks may be incomplete, but routes must remain usable.
   const a=c.acquisition;if(!a||typeof a.standardPacks!=='boolean'||typeof a.tournamentPacks!=='boolean'||!['none','regular','specialist','all'].includes(a.merchants)||!Array.isArray(a.tournamentPrizes)||!a.tournamentPrizes.every(v=>['invitational','masters'].includes(v)))throw Error('Invalid project acquisition settings.');
  }
  return clone(doc);
 }
 return {ranks,rarities,clone,uid,slug,identifier,freshID,routes,suggestedRoutes,newCard,suggestedRarity,curve,artPath,cardErrors,validateManifest,parseJSON,newProject,fromManifest,manifest,issues,readProject};
}
if (typeof module !== 'undefined') module.exports = createExpansionEditorCore;
