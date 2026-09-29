'use strict';
function developmentSettings(){return state?.settings?.development;}
function developmentCommand(args){
 if(!developmentSettings()?.enabled||state.phase!=='ready')return;
 for(const control of $('development-settings').querySelectorAll('button,select')){control.disabled=true;control._refresh?.();}
 send('development',args);
}
function renderDevelopmentSettings(){
 const config=developmentSettings(),section=$('development-settings');section.hidden=!config?.enabled;
 $('settings-panel').firstElementChild.classList.toggle('has-development',!!config?.enabled);
 if(!config?.enabled)return;
 const disabled=Boolean(bindings().capturing||bindingPending)||state.phase!=='ready';
 for(const control of section.querySelectorAll('button,select'))control.disabled=disabled;
 for(const control of section.querySelectorAll('[data-development]')){
  const value=config[control.dataset.development];
  if(control.tagName==='SELECT'){control.value=String(value);control._refresh();}
  else {control.setAttribute('aria-checked',String(!!value));control.lastElementChild.textContent=value?'On':'Off';}
 }
 const inherited=config.Rules<0,hold=$('dev-hold-rules');hold.setAttribute('aria-checked',String(inherited));hold.lastElementChild.textContent=inherited?'On':'Off';
 $('dev-rules').hidden=inherited;
 for(const button of $('dev-rules').children)button.setAttribute('aria-pressed',String(Boolean(config.Rules&Number(button.dataset.rule))));
 $('development-status').textContent=config.notice||'';
}
for(const control of $('development-settings').querySelectorAll('[data-development]')){
 const key=control.dataset.development;
 if(control.tagName==='SELECT')control.onchange=()=>developmentCommand(`set ${key} ${control.value}`);
 else control.onclick=()=>developmentCommand(`set ${key} ${developmentSettings()[key]?0:1}`);
}
for(const [flag,name] of ruleDefinitions){
 const button=document.createElement('button');button.textContent=name;button.dataset.rule=flag;
 button.onclick=()=>{
  let flags=developmentSettings().Rules^flag;
  if(flags&flag){for(const [a,b] of [[16,64],[128,256],[512,1024]]){if(flag===a)flags&=~b;if(flag===b)flags&=~a;}if(flag===8192)flags|=1;}
  if(!(flags&1))flags&=~8192;
  developmentCommand(`set Rules ${flags}`);
 };
 $('dev-rules').append(button);
}
$('dev-hold-rules').onclick=()=>developmentCommand(`set Rules ${developmentSettings().Rules<0?state.rules:-1}`);
$('dev-add-cards').onclick=()=>developmentCommand('cards');
$('dev-add-foils').onclick=()=>developmentCommand('foils');
