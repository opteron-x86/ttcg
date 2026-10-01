'use strict';
// The same chapter panel serves the album and the table; examples and copy have
// one owner. Opening it never routes through the native album or changes a deck.
const rulesHome=document.createComment('Rules chapter home');
$('album-rules').before(rulesHome);
let matchRulesReturn=null;
function openMatchRules(topic=rulesTopic){
 if(state?.phase!=='match'||busy||sent||state.settling||state.thinking||activeOverlay())return;
 hideRuleTooltip();clearBoardGhosts();closeSelectMenu();matchRulesReturn=document.activeElement;
 rulesTopic=topic;stopRulesDemo();$('match-rules-content').append($('album-rules'));
 $('album-rules').hidden=false;$('match-rules').hidden=false;renderAlbumRules();
 $('match-rules-close').focus();
}
function closeMatchRules(focus=true){
 if($('match-rules').hidden)return false;
 stopRulesDemo();$('match-rules').hidden=true;rulesHome.after($('album-rules'));
 $('album-rules').hidden=state?.screen!=='album'||albumTab!=='rules';
 if(focus)(matchRulesReturn?.isConnected?matchRulesReturn:$('match-rules-open')).focus({preventScroll:true});
 matchRulesReturn=null;return true;
}
$('match-rules-open').onclick=()=>openMatchRules();
$('match-rules-close').onclick=()=>closeMatchRules();

// HTML title tooltips are not drawn by Meridian. Keep these within our surface,
// with keyboard focus using the same content and positioning as mouse hover.
function hideRuleTooltip(){const tip=$('rule-tooltip');tip.hidden=true;document.querySelectorAll('[aria-describedby="rule-tooltip"]').forEach(el=>el.removeAttribute('aria-describedby'));}
function showRuleTooltip(chip){
 if(!chip||activeOverlay())return;
 const tip=$('rule-tooltip');tip.textContent=chip.dataset.help;tip.hidden=false;chip.setAttribute('aria-describedby','rule-tooltip');
 const table=$('table'),frame=table.getBoundingClientRect(),rect=chip.getBoundingClientRect(),scale=frame.width/table.offsetWidth;
 const left=(rect.left+rect.width/2-frame.left)/scale-tip.offsetWidth/2;
 tip.style.left=Math.max(16,Math.min(table.offsetWidth-tip.offsetWidth-16,left))+'px';
 tip.style.top=((rect.bottom-frame.top)/scale+10)+'px';
}
document.addEventListener('mouseover',event=>{const chip=event.target.closest('.rule-chip');if(chip&&!chip.contains(event.relatedTarget))showRuleTooltip(chip);});
document.addEventListener('mouseout',event=>{const chip=event.target.closest('.rule-chip');if(chip&&!chip.contains(event.relatedTarget))hideRuleTooltip();});
document.addEventListener('focusin',event=>{if(event.target.matches('.rule-chip'))showRuleTooltip(event.target);});
document.addEventListener('focusout',event=>{if(event.target.matches('.rule-chip'))hideRuleTooltip();});
document.addEventListener('click',event=>{const chip=event.target.closest('#game .rule-chip');if(chip)openMatchRules(RULE_PAGES.find(p=>p.title===chip.textContent)?.id||'trade');});
document.addEventListener('keydown',event=>{if(event.target.matches('.rule-chip')&&['Enter',' '].includes(event.key)){event.preventDefault();event.target.click();}});
window.addEventListener('resize',hideRuleTooltip);
window.addEventListener('blur',hideRuleTooltip);

async function drawStarter(next,token){
 const draw=$('starter-draw'),reduced=matchMedia('(prefers-reduced-motion: reduce)').matches;
 draw.style.setProperty('--starter-direction',next.turn===0?'-1':'1');
 draw.style.setProperty('--starter-offset',next.turn===0?'-120px':'120px');
 draw.dataset.starter=String(next.turn);draw.hidden=false;
 await pause(reduced?350:1150);
 if(token===playbackToken)draw.hidden=true;
}
