// Affinity artwork and the card element shared by the rule lab, the expansion editor and
// tools/build-native-ui-assets.cjs. The tools cut it at the names they need.
const affinityNames=['Strength','Intelligence','Willpower','Agility','Endurance'];
const affinityColors=['#e78871','#7fbce9','#e4ce80','#91c78b','#c2a0e4'];
// Set A: solid heraldic silhouettes with cream edges; no enclosing badge.
// Geometry uses a shared 100-unit canvas so it stays sharp in small hands and zoom.
const affinityArtwork=[
 // Strength: flexed arm, fist and bicep.
 `<path class="affinity-silhouette" d="M9 72C6 61 8 52 13 42L26 17Q28 13 34 10L47 4Q50 3 53 7L60 17Q63 22 59 25Q61 30 56 32Q52 34 48 30Q43 34 39 30L36 26Q31 40 35 51L36 60Q46 49 59 55Q69 40 83 49Q96 59 92 74L86 93Q79 88 74 80Q54 96 35 87L13 79Q8 78 9 72Z"/>
 <path class="affinity-detail" d="M49 12 56 23M44 17 49 29M36 26 39 22M36 60Q33 66 28 68M59 55Q65 59 69 65M38 76Q48 82 61 76M74 80 71 71"/>`,
 // Intelligence: an open volume, with fanned pages and a center fold.
 `<path class="affinity-silhouette" d="M7 22 14 21 14 16 21 17 21 11Q38 9 50 23Q62 9 79 11L79 17 86 16 86 21 93 22 97 87Q72 79 57 87Q50 93 43 87Q27 79 3 87Z"/>
 <path class="affinity-detail" d="M21 17 19 68Q37 64 50 77Q63 64 81 68L79 17M14 21 11 77Q34 70 50 81Q66 70 89 77L86 21M50 23 50 77"/>`,
 // Willpower: frontal lion, angular mane and broad negative-space facial cuts.
 `<path class="affinity-silhouette" d="M50 3 54 12Q65 7 73 10L66 17Q79 14 84 21Q88 27 82 34L94 49 84 46 94 65 84 59Q88 75 78 85L77 72 65 94 63 84 50 98 37 84 35 94 23 72 22 85Q12 75 16 59L6 65 16 46 6 49 18 34Q12 27 16 21Q21 14 34 17L27 10Q35 7 46 12Z"/>
 <path class="affinity-cutout" d="M20 23Q26 20 31 24L25 31Q18 27 20 23M80 23Q74 20 69 24L75 31Q82 27 80 23M50 25Q43 17 34 18L39 26 30 24 23 36 32 33 22 48 31 45 21 64 31 55 29 76 38 64 40 80 50 91 60 80 62 64 71 76 69 55 79 64 69 45 78 48 68 33 77 36 70 24 61 26 66 18Q57 17 50 25Z"/>
 <path fill="currentColor" d="M50 31 39 26 30 39 35 49 39 54 35 63Q36 69 46 69L50 74 54 69Q64 69 65 63L61 54 65 49 70 39 61 26Z"/>
 <path class="affinity-cutout" d="M33 39 43 42 44 49 38 46ZM67 39 57 42 56 49 62 46ZM43 55Q50 58 57 55L54 61 51 63 51 67 61 71 56 73 50 70 44 73 39 71 49 67 49 63 46 61Z"/>
 <path class="affinity-detail" d="M36 61 43 62M64 61 57 62"/>
 <path fill="#f2e3b8" d="M41 75Q50 70 59 75L57 82 54 79 50 85 46 79 43 82Z"/>`,
 // Agility: one diagonal arrow, with a broad point and cut feather edges.
 `<path class="affinity-silhouette" d="M93 7 80 43 74 34 41 68 40 81 32 90 30 76 25 82 25 92 16 98 15 83 3 81 10 74 20 74 13 70 22 62 35 61 67 28 58 23Z"/>
 <path class="affinity-detail" d="M19 81 72 28M26 68 18 68M33 76 33 68"/>`,
 // Endurance: wide anvil horn, narrow waist, and a planted foot.
 `<path class="affinity-silhouette" d="M4 29 37 29 37 24 96 24Q98 35 76 40L68 42Q59 49 65 60Q69 67 77 70L77 82 61 82Q60 77 52 77L41 77Q33 77 32 82L24 82 24 71Q40 66 40 55Q40 45 29 43Q10 39 4 29Z"/>`
];
const affinityPigments=['#b63c33','#3879b4','#d7a631','#55913c','#79459c'];
function affinityIcon(name) {
 const i=affinityNames.indexOf(name),icon=document.createElementNS('http://www.w3.org/2000/svg','svg');icon.setAttribute('viewBox','0 0 100 100');icon.setAttribute('role','img');icon.setAttribute('aria-label',name);icon.setAttribute('focusable','false');icon.classList.add('affinity-icon');icon.style.color=affinityPigments[i];
 // Only fixed, local artwork enters this SVG, never card metadata or user text.
 icon.innerHTML=affinityArtwork[i]||'';return icon;
}
const tierMetrics=new Map();
function tierNumeral(label) {
 // Center the visible glyphs, rather than the font's line box and side bearings.
 const ns='http://www.w3.org/2000/svg',svg=document.createElementNS(ns,'svg'),text=document.createElementNS(ns,'text');
 if(!tierMetrics.has(label)) {
   const context=document.createElement('canvas').getContext('2d');context.font='16px Georgia,serif';
   const m=context.measureText(label);
   tierMetrics.set(label,{x:20+((m.actualBoundingBoxLeft??0)-(m.actualBoundingBoxRight??m.width))/2,y:20+((m.actualBoundingBoxAscent??11)-(m.actualBoundingBoxDescent??0))/2});
 }
 const metrics=tierMetrics.get(label);svg.setAttribute('viewBox','0 0 40 40');svg.setAttribute('aria-hidden','true');svg.setAttribute('focusable','false');
 text.setAttribute('x',metrics.x);text.setAttribute('y',metrics.y);text.textContent=label;svg.append(text);return svg;
}
function cardIdentity(id){return id<0?-1:state.cards[id].baseIndex??id;}
function identityCopies(deck,id){return deck.filter(x=>x>=0&&cardIdentity(x)===cardIdentity(id)).length;}
function identityTotal(predicate){return new Set(state.cards.flatMap((_,i)=>predicate(i)?[cardIdentity(i)]:[])).size;}
function cardElement(id,owner,modifier=0,thumbnail=false) {
 if(id===-2)return cardBackElement(owner);
 const data=state.cards[id], card=document.createElement('div'); card.className=`card card-face owner-${owner}`;
 const portrait=document.createElement('div'); portrait.className='portrait';
 const image=document.createElement('img'); setCardImage(image,data,thumbnail);if(thumbnail){image.loading='lazy';image.decoding='async';} image.style.objectPosition=data.focus||'50% 40%'; image.alt=''; image.draggable=false; portrait.append(image); card.append(portrait);
 if(data.foil){card.classList.add('foil');appendFoilLayers(portrait);}
 card.title=(data.foil?'Foil · ':'')+data.name;
 const name=document.createElement('span'); name.className='card-name'; name.textContent=data.name; card.append(name);
 card.dataset.rarity=data.rarity||'Common';
 const tier=document.createElement('span'),numeral=['','I','II','III','IV','V','VI','VII','VIII','IX','X'][data.tier]||String(data.tier); tier.className='tier'; tier.dataset.rarity=card.dataset.rarity; tier.append(tierNumeral(numeral)); tier.title=`Tier ${numeral} · ${card.dataset.rarity}`; tier.setAttribute('aria-label',`Tier ${data.tier}, ${card.dataset.rarity}`); card.append(tier);
 const affinity=document.createElement('span');affinity.className='card-affinities';affinity.title=(data.affinities||[]).join(' / ')||'Neutral';for(const name of data.affinities||[])affinity.append(affinityIcon(name));card.append(affinity);
 if(modifier){const change=document.createElement('span');change.className='affinity-modifier '+(modifier>0?'positive':'negative');change.textContent=(modifier>0?'+':'−')+Math.abs(modifier);change.title='All sides '+change.textContent;card.append(change);}
 ['top','right','bottom','left'].forEach((side,index)=>{ const rank=document.createElement('span'); rank.className=`rank ${side}`; rank.textContent=data.sides[index]===10?'A':data.sides[index];if(modifier)rank.title=`${data.sides[index]} ${modifier>0?'+':'−'} ${Math.abs(modifier)} = ${data.sides[index]+modifier}`; card.append(rank); });
 return card;
}
