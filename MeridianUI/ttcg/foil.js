// Animate the reflection only: card placement, ownership flips and drag transforms
// remain owned by the table/album. Art inspection supplies its own rotation light.
const foilMotion=(()=>{
 const reduced=matchMedia('(prefers-reduced-motion: reduce)'),rest={x:.5,y:.38};
 let active=null,frame=0,last=0,now={...rest},target={...rest},released=false;
 function paint(card,pose){
  card.style.setProperty('--light-x',pose.x*100+'%');card.style.setProperty('--light-y',pose.y*100+'%');
  card.style.setProperty('--film-x',50+(pose.x-.5)*76+'%');card.style.setProperty('--film-y',50+(pose.y-.38)*72+'%');
  foilMaterial.draw(card,pose);
 }
 function stop(){
  cancelAnimationFrame(frame);frame=0;last=0;
  if(active?.isConnected)paint(active,rest);
  active=null;
 }
 function tick(time){
  frame=0;
  if(!active?.isConnected||!active.getClientRects().length||document.hidden||reduced.matches){stop();return;}
  if(last&&time-last<32){frame=requestAnimationFrame(tick);return;}
  const step=1-Math.exp(-Math.min(80,last?time-last:33)/65);last=time;
  let moving=false;
  for(const key of ['x','y']){now[key]+=(target[key]-now[key])*step;if(Math.abs(target[key]-now[key])>.0005)moving=true;else now[key]=target[key];}
  paint(active,now);
  if(moving)frame=requestAnimationFrame(tick);else{last=0;if(released)active=null;}
 }
 function schedule(){if(!frame)frame=requestAnimationFrame(tick);}
 function release(){if(active){target={...rest};released=true;schedule();}}
 document.addEventListener('pointermove',event=>{
  if(reduced.matches||document.hidden)return;
  const card=event.target.closest('.card.foil');
  if(!card||card.closest('.art-scene')||card.closest('[aria-hidden="true"]')){release();return;}
  if(card!==active){stop();active=card;now={...rest};}
  const bounds=card.getBoundingClientRect();
  if(!bounds.width||!bounds.height)return;
  target={x:Math.max(0,Math.min(1,(event.clientX-bounds.left)/bounds.width)),y:Math.max(0,Math.min(1,(event.clientY-bounds.top)/bounds.height))};
  released=false;schedule();
 });
 document.addEventListener('pointerout',event=>{if(active&&!active.contains(event.relatedTarget))release();});
 document.addEventListener('pointercancel',release);
 document.addEventListener('pointerup',event=>{if(event.pointerType==='touch')release();});
 document.addEventListener('visibilitychange',()=>{if(document.hidden)stop();});
 window.addEventListener('blur',stop);reduced.addEventListener('change',stop);
 // Retire canvases when state publications replace cards. Ignore lighting styles.
 new MutationObserver(records=>{
  if(records.some(record=>record.removedNodes.length)){foilMaterial.prune();if(active&&!active.isConnected)stop();}
 }).observe(document.body,{childList:true,subtree:true});
 return {paint,stop,get active(){return active;},get frame(){return frame;}};
})();
