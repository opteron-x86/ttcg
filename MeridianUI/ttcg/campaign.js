'use strict';
let ruleAnswerPending='';
function renderRuleRequest() {
 const c=state.collection||{},key=`${state.session}:${state.revision}`;
 const show=state.phase==='ready'&&state.screen==='lobby'&&Boolean(c.ruleRequest||c.culture?.offerRule);
 $('table').classList.toggle('rule-prompt',show);$('rule-request').hidden=!show;
 if(!show){ruleAnswerPending='';return false;}
 for(const id of ['challenge','album','game'])$(id).hidden=true;
 $('rules-label').textContent=rulesText(c.fixedRules??state.rules);
 $('rule-request-speaker').textContent=state.opponent;
 const offer=c.culture?.offerRule;
 $('rule-request-text').textContent=offer?`You brought ${rulesText(offer)} from ${c.culture.source}. Shall we try it?${c.culture.replaces?' It replaces '+rulesText(c.culture.replaces)+' for this game.':''} If it catches on, local players may adopt it.`:`Can we play with ${rulesText(c.ruleRequest)}?`;
 $('accept-rule').disabled=$('decline-rule').disabled=ruleAnswerPending===key;
 if(!document.activeElement?.closest('#rule-request'))$('accept-rule').focus();
 return true;
}
function answerRule(accept) {
 const key=`${state.session}:${state.revision}`;
 if(ruleAnswerPending===key)return;
 ruleAnswerPending=key;$('accept-rule').disabled=$('decline-rule').disabled=true;
 if(state.collection.culture?.offerRule)send('culture',`trial ${accept?1:0}`);
 else send('terms',`rule ${accept?state.collection.ruleRequest:0}`);
}
function renderCampaignState(){
 const result=state.collection?.culture?.result||'';
 $('culture-result').textContent=result;$('culture-result').hidden=!result;
}
$('accept-rule').onclick=()=>answerRule(true);$('decline-rule').onclick=()=>answerRule(false);
let abolitionChoice=0;
function closeAbolition(focus=true){if($('abolition-dialog').hidden)return false;$('abolition-dialog').hidden=true;abolitionChoice=0;if(focus)$('abolition-open').focus();return true;}
function renderCultureTerms(){
 const c=state.collection,culture=c.culture||{},trial=culture.trialRule,abolition=culture.abolitionRule;
 const text=abolition?`Abolition challenge: win to remove ${rulesText(abolition)} from ${c.hold}. The rule remains active in this game.`:trial?`Trying ${rulesText(trial)}. Local players may adopt it after this game.`:culture.expert&&culture.localRules?(culture.retryHours?`Another abolition challenge in ${Math.ceil(culture.retryHours/24)} game days.`:''):'';
 $('culture-status').textContent=text;$('abolition-open').hidden=!culture.canAbolish;
 $('culture-terms').hidden=!text&&!culture.canAbolish;
}
$('abolition-open').onclick=()=>{
 const c=state.collection;if(!c.culture?.canAbolish)return;
 abolitionChoice=0;$('abolition-title').textContent=`Abolish a rule in ${c.hold}`;$('abolition-confirm').disabled=true;
 $('abolition-description').textContent='Choose a rule. Accepting commits this attempt. A loss, draw, or abandoned challenge means waiting seven game days before trying again anywhere in this hold.';
 const root=$('abolition-options');root.replaceChildren();
 for(const [rule] of ruleDefinitions)if(c.culture.localRules&rule){
  const b=document.createElement('button');b.textContent=rulesText(rule);b.setAttribute('aria-pressed','false');
  b.onclick=()=>{abolitionChoice=rule;for(const button of root.children)button.setAttribute('aria-pressed',String(button===b));$('abolition-confirm').disabled=false;
   $('abolition-description').textContent=`Play with ${rulesText(rule)} still active. Win to abolish it throughout ${c.hold}.${rule===16||rule===64?' Unplayed hands will then be hidden.':''} Accepting commits this attempt. A loss, draw, or abandonment means waiting seven game days to try again.`;};root.append(b);
 }
 $('abolition-dialog').hidden=false;root.querySelector('button')?.focus();
};
$('abolition-cancel').onclick=()=>closeAbolition();
$('abolition-confirm').onclick=()=>{if(!abolitionChoice||$('abolition-confirm').disabled)return;$('abolition-confirm').disabled=true;send('culture',`abolish ${abolitionChoice}`);};
