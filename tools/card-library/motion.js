// A single interpolated pointer response also drives the in-game reference finish.
const motionPreference=window.matchMedia('(prefers-reduced-motion: reduce)');
let activeFoil=null,foilFrame=0,foilClock=0;
const resting={x:.5,y:.38,rx:0,ry:0};
let foilNow={...resting},foilTarget={...resting};
function paintFoil(card,v){
 const x=v.x,y=v.y;
 const values={
  '--mx':x*100+'%','--my':y*100+'%',
  '--light-x':x*100+'%','--light-y':y*100+'%',
  '--film-x':50+(x-.5)*76+'%','--film-y':50+(y-.38)*72+'%',
  '--grain-x':(x-.5)*30+'px',
  '--from-left':x,'--from-top':y,'--from-center':Math.min(1,Math.hypot(x-.5,y-.5)*2),
  '--bx':70-x*40+'%','--by':65-y*55+'%',
  '--sx':25+x*50+'%','--sy':20+y*60+'%',
  '--rx':v.rx+'deg','--ry':v.ry+'deg'
 };
 for(const [name,value] of Object.entries(values))card.style.setProperty(name,String(value));
 if(card.classList.contains('finish-relief')){
  card.classList.add('foil-lit');
  const light=$('foil-spec');light.setAttribute('x',x.toFixed(3));light.setAttribute('y',y.toFixed(3));
 }
 foilMaterial.draw(card,v);
}
function animateFoil(time){
 foilFrame=0;
 if(!activeFoil?.isConnected||document.hidden){activeFoil=null;return;}
 const step=1-Math.exp(-Math.min(64,time-(foilClock||time-16))/65);foilClock=time;
 let unsettled=false;
 for(const key of Object.keys(resting)){
  foilNow[key]+=(foilTarget[key]-foilNow[key])*step;
  if(Math.abs(foilTarget[key]-foilNow[key])>.0005)unsettled=true;else foilNow[key]=foilTarget[key];
 }
 paintFoil(activeFoil,foilNow);
 if(unsettled)foilFrame=requestAnimationFrame(animateFoil);
}
function scheduleFoil(){if(!foilFrame){foilClock=0;foilFrame=requestAnimationFrame(animateFoil);}}
function releaseFoil(){
 if(!activeFoil)return;
 activeFoil.classList.remove('foil-active');foilTarget={...resting};scheduleFoil();
}
function foilPointer(e){
 if(motionPreference.matches)return;
 const card=e.target.closest('.card.foil');
 if(!card){releaseFoil();return;}
 if(card!==activeFoil){
  if(activeFoil){paintFoil(activeFoil,resting);activeFoil.classList.remove('foil-active','foil-lit');}
  activeFoil=card;foilNow={...resting};
 }
 const anchor=card.parentElement,r=anchor.getBoundingClientRect();
 const x=Math.min(1,Math.max(0,(e.clientX-r.left)/card.offsetWidth));
 const y=Math.min(1,Math.max(0,(e.clientY-r.top)/card.offsetHeight));
 const tilt=card.closest('#finish-compare')?3:7;
 foilTarget={x,y,rx:(.5-y)*tilt,ry:(x-.5)*tilt};
 card.classList.add('foil-active');scheduleFoil();
}
function stopFoil(){
 cancelAnimationFrame(foilFrame);foilFrame=0;
 if(activeFoil){paintFoil(activeFoil,resting);activeFoil.classList.remove('foil-active','foil-lit');activeFoil=null;}
}
for(const id of ['gallery','detail-art','finish-compare']){
 $(id).addEventListener('pointerleave',releaseFoil);
 $(id).addEventListener('pointercancel',releaseFoil);
 $(id).addEventListener('pointerup',e=>{if(e.pointerType==='touch')releaseFoil();});
}
motionPreference.addEventListener('change',stopFoil);
document.addEventListener('visibilitychange',()=>{if(document.hidden)stopFoil();});
$('detail').addEventListener('close',stopFoil);
