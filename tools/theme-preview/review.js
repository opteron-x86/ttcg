'use strict';
const themes={
 charcoal:{title:'Charcoal',copy:'Cool charcoal, silver rules, and restrained sans-serif type.',surface:'slate'},
 oxblood:{title:'Oxblood',copy:'Matte charcoal, muted red selections, and fine ivory borders.',surface:'leather'},
 parchment:{title:'Parchment',copy:'Warm paper, dark ink, and fine ruled divisions.',surface:'parchment'},
 umber:{title:'Umber',copy:'Deep umber, russet selections, and aged brass.',surface:'walnut'},
 tessera:{title:'Tessera',copy:'The existing green and gold interface, for comparison.',surface:'felt'}
};
const labels={plain:'plain',slate:'slate',felt:'green felt',parchment:'parchment',leather:'dark leather',walnut:'walnut'};
const frame=document.getElementById('preview'),params=new URLSearchParams(location.search);
const prefs={theme:themes[params.get('theme')]?params.get('theme'):'oxblood',scene:['match','album','lobby','settings','board'].includes(params.get('scene'))?params.get('scene'):'match',surface:'theme',grain:10,empty:false,fullscreen:false};
let lastScene='match';
function update(){
 const theme=themes[prefs.theme];
 document.querySelectorAll('[data-theme]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.theme===prefs.theme)));
 document.querySelectorAll('[data-scene]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.scene===prefs.scene)));
 document.getElementById('theme-title').textContent=theme.title;
 document.getElementById('theme-copy').textContent=theme.copy;
 document.getElementById('surface-note').textContent=(prefs.surface==='theme'?'Suggested board: ':'Board: ')+labels[prefs.surface==='theme'?theme.surface:prefs.surface];
 document.getElementById('grain-value').textContent=prefs.grain+'%';
 document.getElementById('board-toggle').setAttribute('aria-pressed',String(prefs.scene==='board'));
 document.getElementById('board-toggle').textContent=prefs.scene==='board'?'← Back to window':'Show board detail ↗';
 const shot=prefs.scene==='board'?'board':prefs.scene==='lobby'?'lobby':prefs.scene==='settings'?'settings':prefs.scene;
 const link=document.getElementById('screenshot-link');link.href=prefs.theme+'-'+shot+'.png';link.title='Default sample for this theme; custom combinations are shown in the live preview.';
 frame.contentWindow.postMessage({type:'tessera-theme-preview',prefs},'*');
}
document.getElementById('theme-options').addEventListener('click',e=>{const button=e.target.closest('[data-theme]');if(button){prefs.theme=button.dataset.theme;update();}});
document.getElementById('scene-options').addEventListener('click',e=>{const button=e.target.closest('[data-scene]');if(button){prefs.scene=button.dataset.scene;update();}});
document.getElementById('surface').onchange=e=>{prefs.surface=e.target.value;update();};
document.getElementById('grain').oninput=e=>{prefs.grain=Number(e.target.value);update();};
document.getElementById('empty-board').onchange=e=>{prefs.empty=e.target.checked;update();};
document.getElementById('large-window').onchange=e=>{prefs.fullscreen=e.target.checked;update();};
document.getElementById('board-toggle').onclick=()=>{if(prefs.scene==='board')prefs.scene=lastScene;else{lastScene=prefs.scene;prefs.scene='board';}update();};
window.addEventListener('message',e=>{if(e.source===frame.contentWindow&&e.data?.type==='tessera-preview-ready')update();});
frame.addEventListener('load',update);
update();
