const reviewFinishes=[['current','Tessera · Prismatic','Approved in-game finish · 72%'],['satin','Satin','Soft silver with a trace of colour'],['engraved','Engraved','Diamond engraving that catches the light']];
const review=$('foil-review'),reviewGrid=$('review-grid');
let reviewFrame=0,reviewLast=0,reviewAngle=45,reviewRunning=!motionPreference.matches,reviewVisible=false,reviewCards=[],reviewPose={...resting};
let reviewHover=false,reviewTarget=null;
const reviewNames=['Nocturnal','Riverwood Woodcutter','Dwarven Ballista','Akatosh','Dibella','Piercing Twilight','M’aiq the Liar'];
const reviewExamples=reviewNames.map(name=>cards.find(c=>c.name===name||c.name.replaceAll("'",'’')===name)).filter(Boolean);
for(const card of reviewExamples)$('review-art').add(new Option(card.name,card.id));
function updateReviewButton(){
 $('review-motion').textContent=reviewRunning?'Pause light':'Turn light';
 $('review-motion').setAttribute('aria-pressed',String(reviewRunning));
 $('review-motion').disabled=motionPreference.matches;
}
function paintReview(pose){
 reviewPose={...pose};
 const strength=Number($('review-strength').value)/100;
 for(const card of reviewCards){card.style.setProperty('--foil-strength',String(strength));paintFoil(card,pose);}
}
function anglePose(angle){
 const t=angle*Math.PI/180,x=.5+Math.cos(t)*.42,y=.48+Math.sin(t)*.36;
 return {x,y,rx:(.5-y)*9,ry:(x-.5)*11};
}
function paintReviewAngle(){
 $('review-angle').value=String(Math.round(reviewAngle)%360);
 reviewTarget=null;paintReview(anglePose(reviewAngle));
}
function stopReviewFrame(){cancelAnimationFrame(reviewFrame);reviewFrame=0;reviewLast=0;}
function reviewCanMove(){return reviewVisible&&review.open&&!document.hidden&&!$('detail').open&&!motionPreference.matches;}
function tickReview(time){
 reviewFrame=0;
 if(!reviewCanMove()){reviewLast=0;return;}
 if(reviewLast&&time-reviewLast<32){reviewFrame=requestAnimationFrame(tickReview);return;}
 const elapsed=reviewLast?Math.min(80,time-reviewLast):33;reviewLast=time;
 const turning=reviewRunning&&!reviewHover;
 if(turning){
  reviewAngle=(reviewAngle+elapsed*.013)%360;
  reviewTarget=anglePose(reviewAngle);$('review-angle').value=String(Math.round(reviewAngle)%360);
 }
 if(!reviewTarget){reviewLast=0;return;}
 // All cards share one damped response and at most one paint per frame.
 const next={...reviewPose},step=1-Math.exp(-elapsed/80);
 let unsettled=false;
 for(const key of Object.keys(resting)){
  next[key]+=(reviewTarget[key]-next[key])*step;
  if(Math.abs(reviewTarget[key]-next[key])>.0005)unsettled=true;else next[key]=reviewTarget[key];
 }
 paintReview(next);
 if(turning||unsettled)reviewFrame=requestAnimationFrame(tickReview);else{reviewTarget=null;reviewLast=0;}
}
function scheduleReview(){if((reviewRunning||reviewTarget)&&reviewCanMove()&&!reviewFrame)reviewFrame=requestAnimationFrame(tickReview);}
function renderReview(){
 const card=cards.find(c=>c.id===$('review-art').value)||reviewExamples[0];
 reviewGrid.replaceChildren();foilMaterial.prune();reviewCards=[];
 for(const [id,label,description] of reviewFinishes){
  const figure=el('figure',undefined,'review-sample'),caption=el('figcaption');
  caption.append(el('h3',label),el('p',description));
  const holder=el('div',undefined,'review-card-holder');holder.tabIndex=0;holder.setAttribute('role','group');holder.setAttribute('aria-label',`${label}: ${card.name}. Use arrow keys to move the light.`);
  const sample=cardArt(card,true,id);holder.append(sample);figure.append(caption,holder);reviewGrid.append(figure);reviewCards.push(sample);
 }
 requestAnimationFrame(()=>paintReview(reviewPose));
 updateReviewButton();scheduleReview();
}
function reviewPointer(event){
 if(motionPreference.matches)return;
 const holder=event.target.closest('.review-card-holder');if(!holder){leaveReview();return;}
 reviewHover=true;
 const r=holder.getBoundingClientRect(),x=Math.max(0,Math.min(1,(event.clientX-r.left)/r.width)),y=Math.max(0,Math.min(1,(event.clientY-r.top)/r.height));
 reviewTarget={x,y,rx:(.5-y)*9,ry:(x-.5)*11};scheduleReview();
}
function leaveReview(){
 if(!reviewHover)return;reviewHover=false;
 // Ease back onto the orbit from the nearest direction instead of snapping home.
 reviewAngle=(Math.atan2((reviewPose.y-.48)/.36,(reviewPose.x-.5)/.42)*180/Math.PI+360)%360;
 scheduleReview();
}
function pauseReview(){reviewRunning=false;reviewTarget=null;stopReviewFrame();updateReviewButton();}
reviewGrid.addEventListener('pointermove',reviewPointer);
reviewGrid.addEventListener('pointerleave',leaveReview);
reviewGrid.addEventListener('pointercancel',leaveReview);
reviewGrid.addEventListener('pointerup',event=>{if(event.pointerType==='touch')leaveReview();});
reviewGrid.addEventListener('keydown',event=>{
 if(!event.target.closest('.review-card-holder')||!['ArrowLeft','ArrowRight','ArrowUp','ArrowDown','Home'].includes(event.key))return;
 event.preventDefault();pauseReview();
 if(event.key==='Home'){reviewAngle=45;paintReviewAngle();return;}
 const p={...reviewPose},step=.04;
 if(event.key==='ArrowLeft')p.x-=step;if(event.key==='ArrowRight')p.x+=step;
 if(event.key==='ArrowUp')p.y-=step;if(event.key==='ArrowDown')p.y+=step;
 p.x=Math.max(0,Math.min(1,p.x));p.y=Math.max(0,Math.min(1,p.y));p.rx=(.5-p.y)*9;p.ry=(p.x-.5)*11;paintReview(p);
});
$('review-art').addEventListener('change',renderReview);
$('review-strength').addEventListener('input',()=>{$('review-strength-value').textContent=$('review-strength').value+'%';paintReview(reviewPose);});
$('review-angle').addEventListener('input',()=>{pauseReview();reviewAngle=Number($('review-angle').value);paintReviewAngle();});
$('review-details').addEventListener('change',()=>reviewGrid.classList.toggle('art-only',!$('review-details').checked));
$('review-motion').onclick=()=>{reviewRunning=!reviewRunning;reviewHover=false;updateReviewButton();if(reviewRunning)scheduleReview();else stopReviewFrame();};
$('review-reset').onclick=()=>{
 reviewAngle=45;$('review-strength').value='72';$('review-strength-value').textContent='72%';$('review-details').checked=true;reviewGrid.classList.remove('art-only');paintReviewAngle();
};
review.addEventListener('toggle',()=>{
 if(review.open){if(!reviewCards.length){reviewPose=anglePose(reviewAngle);renderReview();}scheduleReview();}
 else stopReviewFrame();
});
new IntersectionObserver(entries=>{reviewVisible=entries[0].isIntersecting;if(reviewVisible)scheduleReview();else stopReviewFrame();},{threshold:.05}).observe(reviewGrid);
document.addEventListener('visibilitychange',()=>{if(document.hidden)stopReviewFrame();else scheduleReview();});
motionPreference.addEventListener('change',()=>{
 if(motionPreference.matches){pauseReview();paintReview(resting);}
 updateReviewButton();
});
$('detail').addEventListener('close',scheduleReview);
function openFoilReview(card){
 if(card){
  if(![...$('review-art').options].some(o=>o.value===card.id))$('review-art').add(new Option(card.name,card.id));
  $('review-art').value=card.id;
 }
 if($('detail').open)$('detail').close();
 review.open=true;renderReview();review.scrollIntoView({behavior:'instant',block:'start'});
}
const compareCard=el('button','Compare foil finishes');compareCard.id='compare-card';compareCard.type='button';
$('full-art').after(compareCard);compareCard.onclick=()=>openFoilReview(filtered[detailIndex]);
function reviewHash(){if(location.hash==='#foil-review'){review.open=true;requestAnimationFrame(()=>review.scrollIntoView({block:'start'}));}}
window.addEventListener('hashchange',reviewHash);reviewHash();updateReviewButton();
