'use strict';
// Draw options inside the game view: native select popups ignore its scale/theme.
let openSelectMenu=null;
function closeSelectMenu(restoreFocus=false) {
 if(!openSelectMenu) return;
 const menu=openSelectMenu; openSelectMenu=null;
 menu.list.hidden=true; menu.trigger.setAttribute('aria-expanded','false');
 if(restoreFocus) menu.trigger.focus({preventScroll:true});
}
function selectMenuKey(key) {
 if(!openSelectMenu) {
   const select=document.activeElement?._selectMenu;
   if(select&&['up','down','left','right'].includes(key)) { select.open(); return true; }
   return false;
 }
 if(key==='cancel') { closeSelectMenu(true); return true; }
 if(key==='confirm') { document.activeElement?.click(); return true; }
 const options=[...openSelectMenu.list.children].filter(x=>!x.disabled);
 if(!options.length) return true;
 let index=options.indexOf(document.activeElement);
 if(key==='home') index=0;
 else if(key==='end') index=options.length-1;
 else index=(index+(['previous','left','up'].includes(key)?-1:1)+options.length)%options.length;
 options[index].focus({preventScroll:true});
 return true;
}
function buildSelectMenus() {
 document.querySelectorAll('select').forEach(select=>{
   const shell=document.createElement('div'); shell.className='select-menu';
   const trigger=document.createElement('button'); trigger.id=select.id+'-trigger'; trigger.type='button';
   trigger.className='select-trigger'; trigger.setAttribute('role','combobox');
   trigger.setAttribute('aria-haspopup','listbox'); trigger.setAttribute('aria-expanded','false');
   trigger.setAttribute('aria-label',select.getAttribute('aria-label')||'Rules');
   const list=document.createElement('div'); list.id=select.id+'-options'; list.className='select-options'; list.hidden=true;
   list.setAttribute('role','listbox'); list.setAttribute('aria-label',trigger.getAttribute('aria-label'));
   trigger.setAttribute('aria-controls',list.id);
   const menu={trigger,list,open(){
     closeSelectMenu(); openSelectMenu=menu; list.hidden=false; trigger.setAttribute('aria-expanded','true');
     list.children[select.selectedIndex]?.focus({preventScroll:true});
   }};
   trigger._selectMenu=menu;
   trigger.onclick=()=>openSelectMenu===menu?closeSelectMenu(true):menu.open();
   const rebuild=()=>{list.replaceChildren();for(const option of select.options) {
     const button=document.createElement('button'); button.type='button'; button.setAttribute('role','option');
     button.dataset.value=option.value; button.textContent=option.textContent;
     button.onclick=()=>{
       closeSelectMenu(true); select.value=option.value;
       select.dispatchEvent(new Event('change',{bubbles:true}));
     };
     list.append(button);
   }};
   let optionsKey='';
   const update=()=>{
     const key=JSON.stringify([...select.options].map(o=>[o.value,o.textContent]));
     if(key!==optionsKey){optionsKey=key;rebuild();}
     trigger.disabled=select.disabled;
     [...list.children].forEach((button,i)=>button.disabled=select.options[i].disabled);
     trigger.textContent=select.selectedOptions[0]?.textContent||'';
     for(const option of list.children) option.setAttribute('aria-selected',String(option.dataset.value===select.value));
   };
   select._refresh=update; select.addEventListener('change',update); update();
   select.hidden=true; select.setAttribute('aria-hidden','true'); select.tabIndex=-1;
   select.after(shell); shell.append(trigger,list);
 });
}
document.addEventListener('pointerdown',event=>{
 if(openSelectMenu&&!openSelectMenu.trigger.parentElement.contains(event.target)) closeSelectMenu();
});
document.addEventListener('keydown',event=>{
 if(event.key==='Tab') closeSelectMenu();
});
