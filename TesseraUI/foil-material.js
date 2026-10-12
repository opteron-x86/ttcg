// Shared game/library foil. Prismatic is the approved 72% in-game material.
// One WebGL context serves every visible card;
// the painting remains a normal image underneath, with no texture upload or CORS dependency.
// Grain and engravings are fixed in card coordinates. Only the light changes.
// Context lifecycle: https://registry.khronos.org/webgl/specs/latest/1.0/
const foilMaterial=(()=>{
 const modes={prismatic:0,current:0,satin:1,engraved:2};
 const surface=document.createElement('canvas'),records=new Map();
 let gl=null,program=null,uniforms=null,failed=false,drawCount=0,totalDrawTime=0;
 const vertex=`attribute vec2 position; varying vec2 uv;
 void main(){uv=position*.5+.5;gl_Position=vec4(position,0.,1.);}`;
 const fragment=`precision highp float;
 varying vec2 uv;
 uniform vec2 light;
 uniform vec2 resolution;
 uniform float material;
 float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
 float bell(float d,float width){return exp(-d*d/(width*width));}
 vec3 spectrum(float phase){
   vec3 bands=.5+.5*cos(phase*6.2831853+vec3(0.,2.0943951,4.1887902));
   return mix(vec3(.12),pow(bands,vec3(1.8)),.88);
 }
 void main(){
   vec2 p=vec2(uv.x,1.-uv.y);
   vec2 paper=(p-.5)*vec2(.75,1.);
   vec2 lamp=(light-.5)*vec2(.96,1.15);
   // Fine deposited metal. The two noise scales are stationary, not animated grain.
   vec2 cell=floor(p*vec2(300.,400.));
   float grain=hash(cell),grain2=hash(cell+vec2(59.,23.));
   float deposit=.76+.24*grain;
   vec2 axis=normalize(vec2(.88,.47));
   float travel=dot(lamp,axis);
   float across=dot(paper,axis);
   float bend=.017*sin(p.y*9.)+.010*sin(p.x*17.+p.y*4.);
   float band=across-travel+bend;
   float phase=band*1.47+light.x*.30-light.y*.19;
   vec3 colour=spectrum(phase);
   vec3 incident=normalize(vec3(lamp*1.45,1.));
   vec3 eye=normalize(vec3(-paper*.18,1.));
   vec3 halfVector=normalize(incident+eye);
   vec3 normal=normalize(vec3((grain-.5)*.30,(grain2-.5)*.30,1.));
   float glint=pow(max(0.,dot(normal,halfVector)),160.);
   glint*=smoothstep(.965,1.,grain2);
   // The clear coat and diffracted metal have separate, differently sized reflections.
   vec2 distanceToLight=paper-lamp;
   float clearCoat=exp(-dot(distanceToLight*vec2(2.1,2.8),distanceToLight*vec2(2.1,2.8)));
   float crest=bell(band,.035);
   float ribbon=bell(band,.23);
   float grazing=clamp(length(lamp)*.72,0.,.42);
   vec3 reflected;
   if(material<.5){
     reflected=colour*(.018+.34*ribbon)*deposit;
     reflected+=vec3(.96,.98,1.)*(.04*clearCoat+.19*crest+glint*.38);
     // A very quiet second-order reflection prevents a flat rainbow wash.
     reflected+=spectrum(phase+.43)*bell(band-.29,.07)*.06;
   }else if(material<1.5){
     float brush=.86+.14*sin(p.y*1600.+sin(p.x*18.)*.35);
     vec3 pearl=mix(vec3(.82,.85,.83),colour,.34);
     float broad=bell(band,.32);
     reflected=pearl*(.018+.24*broad)*deposit;
     reflected+=vec3(.92,.96,1.)*(bell(band,.08)*.12*brush+glint*.16);
     reflected+=colour*clearCoat*.045;
   }else{
     // Two crossed cuts form Tessera's diamond engraving; light reveals each cut separately.
     vec2 diamond=vec2(p.x*.75+p.y,p.x*.75-p.y)*13.;
     vec2 f=fract(diamond);
     vec2 edge=min(f,1.-f);
     float footprint=13./max(resolution.y,160.);
     vec2 cut=1.-smoothstep(vec2(.005),vec2(.005+footprint*1.25),edge);
     float facet=step(.5,f.x)*.58+step(.5,f.y)*.42;
     vec3 etchedColour=spectrum(phase+facet*.15);
     float angled=(cut.x*(.25+light.x*.75)+cut.y*(1.-light.x*.65));
     float etchLight=bell(band,.19)*angled;
     reflected=colour*(.025+.16*ribbon)*deposit;
     reflected+=etchedColour*etchLight*.27;
     reflected+=vec3(.96,.91,.77)*(etchLight*.08+glint*.18);
     reflected+=vec3(.94,.97,1.)*clearCoat*.035;
   }
   reflected*=1.+grazing;
   // The clear border catches light without tinting ranks or the printed name.
   float edgeDistance=min(min(p.x,1.-p.x),min(p.y,1.-p.y));
   float rim=(1.-smoothstep(.0,.012,edgeDistance))*clearCoat;
   reflected+=vec3(.68,.79,.82)*rim*.10;
   gl_FragColor=vec4(clamp(reflected,0.,.72),1.);
 }`;
 function compile(type,code){
  const shader=gl.createShader(type);gl.shaderSource(shader,code);gl.compileShader(shader);
  if(!gl.getShaderParameter(shader,gl.COMPILE_STATUS)){
   const error=gl.getShaderInfoLog(shader);gl.deleteShader(shader);throw new Error(error);
  }
  return shader;
 }
 function initialise(){
  if(failed)return false;
  try{
   if(!gl)gl=surface.getContext('webgl',{alpha:false,antialias:false,depth:false,stencil:false,preserveDrawingBuffer:false,powerPreference:'low-power'});
   if(!gl){failed=true;return false;}
   if(gl.isContextLost())return false;
   if(program)return true;
   const vs=compile(gl.VERTEX_SHADER,vertex),fs=compile(gl.FRAGMENT_SHADER,fragment);
   program=gl.createProgram();gl.attachShader(program,vs);gl.attachShader(program,fs);gl.linkProgram(program);
   gl.deleteShader(vs);gl.deleteShader(fs);
   if(!gl.getProgramParameter(program,gl.LINK_STATUS))throw new Error(gl.getProgramInfoLog(program));
   gl.useProgram(program);
   const buffer=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,buffer);
   gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,1,-1,-1,1,-1,1,1,-1,1,1]),gl.STATIC_DRAW);
   const position=gl.getAttribLocation(program,'position');gl.enableVertexAttribArray(position);gl.vertexAttribPointer(position,2,gl.FLOAT,false,0,0);
   uniforms=Object.fromEntries(['light','resolution','material'].map(name=>[name,gl.getUniformLocation(program,name)]));
   return true;
  }catch(error){
   failed=true;program=null;
   console.warn('Tessera foil is using its CSS fallback:',error.message);
   return false;
  }
 }
 function draw(card,pose={x:.5,y:.38}){
  const entry=records.get(card);if(!entry)return;
  entry.pose={x:pose.x,y:pose.y};
  if(!entry.visible||!entry.context||!card.isConnected||!card.offsetWidth||!initialise())return;
  const started=performance.now(),scale=Math.min(devicePixelRatio||1,1.5);
  const width=Math.min(512,Math.max(80,Math.round(card.clientWidth*scale))),height=Math.round(width*card.clientHeight/card.clientWidth);
  if(card.classList.contains('material-ready')&&entry.canvas.width===width&&entry.canvas.height===height&&entry.last?.x===pose.x&&entry.last?.y===pose.y)return;
  if(surface.width!==width||surface.height!==height){surface.width=width;surface.height=height;}
  if(entry.canvas.width!==width||entry.canvas.height!==height){entry.canvas.width=width;entry.canvas.height=height;}
  gl.viewport(0,0,width,height);gl.useProgram(program);
  gl.uniform2f(uniforms.light,pose.x,pose.y);gl.uniform2f(uniforms.resolution,width,height);gl.uniform1f(uniforms.material,entry.mode);
  gl.drawArrays(gl.TRIANGLES,0,6);
  // Copy synchronously before the browser clears the shared drawing buffer.
  entry.context.drawImage(surface,0,0);
  entry.last={x:pose.x,y:pose.y};
  card.classList.add('material-ready');drawCount++;totalDrawTime+=performance.now()-started;
 }
 const observer=new IntersectionObserver(entries=>{
  for(const item of entries){
   const entry=records.get(item.target);if(!entry)continue;
   entry.visible=item.isIntersecting;
   if(item.isIntersecting)draw(item.target,entry.pose);
   else{entry.canvas.width=entry.canvas.height=1;entry.last=null;item.target.classList.remove('material-ready');}
  }
 });
 const resize=new ResizeObserver(entries=>{
  for(const item of entries){const entry=records.get(item.target);if(entry?.visible&&!document.hidden)draw(item.target,entry.pose);}
 });
 surface.addEventListener('webglcontextlost',event=>{
  event.preventDefault();program=null;
  for(const card of records.keys())card.classList.remove('material-ready');
 });
 surface.addEventListener('webglcontextrestored',()=>{
  failed=false;program=null;
  if(initialise())for(const [card,entry] of records)if(entry.visible)draw(card,entry.pose);
 });
 return {
  supports:style=>Object.hasOwn(modes,style),
  attach(card,style='prismatic',portrait=card.querySelector('.portrait')||card){
   if(records.has(card))return;
   const canvas=portrait.querySelector('.foil-material')||document.createElement('canvas');canvas.className='foil-material';canvas.setAttribute('aria-hidden','true');
   canvas.width=canvas.height=1;
   portrait.append(canvas);
   records.set(card,{canvas,context:canvas.getContext('2d',{alpha:false}),mode:modes[style],pose:{x:.5,y:.38},visible:false});
   observer.observe(card);resize.observe(card);
  },
  clone(source){
   const copy=source.cloneNode(true),originals=source.querySelectorAll('.foil-material'),canvases=copy.querySelectorAll('.foil-material');
   // DOM cloning omits canvas pixels. A flight/drag is a still photograph of its
   // source card, including the reflection at the moment it was picked up.
   originals.forEach((canvas,i)=>canvases[i].getContext('2d',{alpha:false}).drawImage(canvas,0,0));
   const cards=[...(copy.matches('.foil')?[copy]:[]),...copy.querySelectorAll('.foil')];
   for(const card of cards)if(!card.classList.contains('material-ready'))this.attach(card);
   return copy;
  },
  draw,
  prune(){for(const card of records.keys())if(!card.isConnected){observer.unobserve(card);resize.unobserve(card);records.delete(card);}},
  diagnostics(){return {backend:program&&!gl.isContextLost()?'webgl':'css',contexts:gl?1:0,cards:records.size,draws:drawCount,meanDrawMs:drawCount?totalDrawTime/drawCount:0};},
  get context(){return gl;}
 };
})();
