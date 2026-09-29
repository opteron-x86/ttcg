'use strict';
let shopTab='buy',shopSelected=-1,shopSession=null,shopConfirmation=null;
const shopNumbers=new Intl.NumberFormat('en-US');
function shopMoney(n){return shopNumbers.format(n)+' gold';}
function shopRows(){
 const shop=state?.shop;if(!shop)return [];
 if(shopTab==='sell')return shop.sell.map(row=>({...row,count:$('shop-inventory').value==='surplus'?row.surplus:row.count})).filter(row=>row.count>0);
 if(shopTab==='buyback')return shop.buy.filter(row=>row.buyback>0).map(row=>({...row,count:row.buyback,price:row.salePrice}));
 return shop.buy;
}
function resetShop(){shopSession=null;shopSelected=-1;shopConfirmation=null;$('shop-confirm').hidden=true;$('shop').hidden=true;}
function renderShop(){
 if(!state.shop)return;
 const focus=document.activeElement?.id,grid=$('shop-grid'),scroll=grid.scrollTop;
 if(shopSession!==state.session){
  shopSession=state.session;shopTab='buy';shopSelected=-1;$('shop-search').value='';
  for(const id of ['shop-tier','shop-rarity','shop-inventory']){const select=$(id);select.selectedIndex=0;select._refresh();}
 }
 const shop=state.shop;$('shop-name').textContent=shop.name;$('shop-specialist').hidden=!shop.specialist;
 $('shop-gold').textContent=shopNumbers.format(shop.gold);$('shop-merchant-gold').textContent=shopNumbers.format(shop.merchantGold);
 for(const tab of ['buy','sell','buyback'])$('shop-'+tab+'-tab').setAttribute('aria-pressed',String(shopTab===tab));
 $('shop-inventory').nextElementSibling.hidden=shopTab!=='sell';
 $('shop-filter-note').textContent=shopTab==='buyback'?'Buy back at the price you received, until you leave.':shopTab==='sell'&&$('shop-inventory').value==='surplus'?'Keeps two standard copies of each card.':'';
 const query=$('shop-search').value.trim().toLocaleLowerCase(),tier=Number($('shop-tier').value),rarity=$('shop-rarity').value;
 const rows=shopRows().filter(row=>{const c=state.cards[row.id];return c&&(!tier||c.tier===tier)&&(!rarity||c.rarity===rarity)&&(!query||[c.name,...(c.groups||[]),...(c.affinities||[]),c.foil?'foil':''].join(' ').toLocaleLowerCase().includes(query));});
 rows.sort((a,b)=>state.cards[a.id].tier-state.cards[b.id].tier||a.price-b.price||state.cards[a.id].name.localeCompare(state.cards[b.id].name));
 if(!rows.some(row=>row.id===shopSelected))shopSelected=rows[0]?.id??-1;
 grid.replaceChildren();
 for(const row of rows){
  const c=state.cards[row.id],button=document.createElement('button');button.className='shop-card';button.id='shop-card-'+row.id;button.setAttribute('aria-pressed',String(row.id===shopSelected));
  button.setAttribute('aria-label',`${c.name}${c.foil?', foil':''}, ${shopMoney(row.price)}, ${row.count} available`);
  const art=document.createElement('span');art.className='shop-card-art';art.append(cardElement(row.id,0,0,true));
  if(row.count>1){const copies=document.createElement('span');copies.className='owned-count';copies.textContent='×'+row.count;art.append(copies);}
  const price=document.createElement('span');price.className='shop-card-price';price.textContent=shopMoney(row.price);
  const owned=state.collection?.owned?.[row.id]||0;if(shopTab!=='sell'&&owned){const count=document.createElement('small');count.textContent=owned+' owned';price.append(count);}
  button.append(art,price);button.onclick=()=>{shopSelected=row.id;renderShop();$(button.id)?.focus({preventScroll:true});};
  button.oncontextmenu=e=>{e.preventDefault();inspectTableCard(row.id);};grid.append(button);
 }
 grid.scrollTop=scroll;$('shop-results').textContent=rows.length+(rows.length===1?' card':' cards');
 $('shop-empty').hidden=rows.length>0;grid.hidden=!rows.length;
 $('shop-empty').textContent=query||tier||rarity?'No cards match these filters.':shopTab==='buyback'?'Cards you sell during this visit appear here.':shopTab==='sell'&&$('shop-inventory').value==='surplus'?'No surplus cards. Choose All cards to see the rest of your collection.':shopTab==='sell'?'No cards to sell.':'The merchant has no cards left.';
 renderShopSelection(rows.find(row=>row.id===shopSelected));$('shop-notice').textContent=shop.notice||'';
 if(!activeOverlay()&&!openSelectMenu){const target=focus?$(focus):null;if(target&&!target.disabled&&target.getClientRects().length)target.focus({preventScroll:true});else (grid.querySelector('button')||$('shop-search')).focus({preventScroll:true});}
}
function renderShopSelection(row){
 $('shop-selection').hidden=!row;$('shop-no-selection').hidden=Boolean(row);if(!row)return;
 const card=state.cards[row.id],preview=$('shop-preview');
 if(preview.dataset.card!==String(row.id)){preview.dataset.card=row.id;preview.replaceChildren(cardElement(row.id,0));$('shop-quantity').value='1';}
 preview.setAttribute('aria-label','Inspect '+card.name);preview.onclick=()=>inspectTableCard(row.id);
 $('shop-count').textContent=`${state.collection?.owned?.[row.id]||0} owned · Tier ${['','I','II','III','IV','V','VI','VII','VIII','IX','X'][card.tier]}`;
 renderCardMetadata($('shop-metadata'),card);
 const owned=state.shop.sell.find(x=>x.id===row.id);$('shop-deck-note').textContent=owned?.reserved?'Used in a saved deck':'';
 const quantity=$('shop-quantity'),previous=Number(quantity.value)||1,max=Math.min(99,row.count,Math.floor(1000000/row.price));
 quantity.replaceChildren(...Array.from({length:max},(_,i)=>new Option(String(i+1),String(i+1))));quantity.value=String(Math.max(1,Math.min(previous,max)));quantity._refresh();
 quantity.parentElement.hidden=max<=1;
 const total=Number(quantity.value)*row.price,selling=shopTab==='sell',enough=(selling?state.shop.merchantGold:state.shop.gold)>=total;
 $('shop-action').textContent=(selling?'Sell':shopTab==='buyback'?'Buy back':'Buy')+' · '+shopMoney(total);
 $('shop-action').disabled=sent||state.shop.pending||!enough||!max;
 $('shop-reason').textContent=state.shop.pending?'Payment is pending. Reopen trading to retry.':!enough?(selling?'The merchant doesn’t have enough gold.':'You don’t have enough gold.'):'';
}
function closeShopConfirmation(focus=true){
 if($('shop-confirm').hidden)return false;
 $('shop-confirm').hidden=true;shopConfirmation=null;if(focus)$('shop-action').focus({preventScroll:true});return true;
}
function shopSellWarnings(row,quantity){
 const messages=[];
 if(row.warnings&1)messages.push('This is a unique card.');
 if(row.warnings&2)messages.push('This copy is foil.');
 if(row.count-quantity<row.reserved)messages.push('This sale will leave an empty slot in a saved deck.');
 if(row.lastHandAt&&quantity>=row.lastHandAt)messages.push('You won’t have enough cards for a game.');
 return messages;
}
function submitShopTrade(confirmed=false){
 const row=shopRows().find(r=>r.id===shopSelected);if(!row||sent||$('shop-action').disabled)return;
 const quantity=Number($('shop-quantity').value)||1;
 if(shopTab==='sell'&&!confirmed){
  const owned=state.shop.sell.find(r=>r.id===row.id),warnings=shopSellWarnings(owned,quantity);
  if(warnings.length){
   shopConfirmation={session:state.session,revision:state.revision,id:row.id,quantity};
   $('shop-confirm-title').textContent=`Sell ${quantity>1?quantity+' × ':''}${state.cards[row.id].name} for ${shopMoney(row.price*quantity)}?`;
   $('shop-confirm-copy').textContent=warnings.join('\n');$('shop-confirm').hidden=false;closeSelectMenu();$('shop-cancel').focus();return;
  }
 }
 sent=true;$('shop-action').disabled=true;send('shop',`${shopTab} ${row.id} ${quantity} ${confirmed?1:0}`);
}
for(const tab of ['buy','sell','buyback'])$('shop-'+tab+'-tab').onclick=()=>{shopTab=tab;shopSelected=-1;$('shop-grid').scrollTop=0;renderShop();};
$('shop-search').oninput=()=>{$('shop-grid').scrollTop=0;renderShop();};
for(const id of ['shop-tier','shop-rarity','shop-inventory'])$(id).addEventListener('change',()=>{$('shop-grid').scrollTop=0;renderShop();});
$('shop-quantity').addEventListener('change',()=>renderShopSelection(shopRows().find(row=>row.id===shopSelected)));
$('shop-action').onclick=()=>submitShopTrade();$('shop-cancel').onclick=()=>closeShopConfirmation();
$('shop-confirm-action').onclick=()=>{
 const request=shopConfirmation;if(!request||request.session!==state?.session||request.revision!==state?.revision)return;
 closeShopConfirmation(false);submitShopTrade(true);
};
$('shop-confirm').onclick=e=>{if(e.target===$('shop-confirm'))closeShopConfirmation();};
