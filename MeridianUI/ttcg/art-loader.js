'use strict';
// Meridian may reject a valid MO2 asset after resolving it outside the physical
// UI mod folder. Read that image through Tessera's native VFS bridge on demand.
const artImages=(()=>{
 const cache=new Map(),pending=new Map(),requests=new Map(),queue=[],nativeFolders=new Set(),bindings=new WeakMap();
 const maxBytes=32*1024*1024,maxEntries=96;
 let serial=0,cacheBytes=0,inFlight=0;
 const bridge=()=>typeof window.ttcgReadArt==='function';
 const folder=path=>path.slice(0,path.lastIndexOf('/')+1);
 const key=(path,version)=>path+'\n'+version;
 function remember(id,data){
  if(!data||data.length>maxBytes)return;
  const old=cache.get(id);if(old){cacheBytes-=old.length;cache.delete(id);}
  cache.set(id,data);cacheBytes+=data.length;
  while(cacheBytes>maxBytes||cache.size>maxEntries){const first=cache.keys().next().value;cacheBytes-=cache.get(first).length;cache.delete(first);}
 }
 function cached(id){const data=cache.get(id);if(data){cache.delete(id);cache.set(id,data);}return data;}
 function finish(request,data){
  if(!requests.delete(request.id))return;
  clearTimeout(request.timer);inFlight--;pending.delete(request.key);
  if(data){remember(request.key,data);nativeFolders.add(folder(request.path));}
  request.resolve(data);pump();
 }
 function pump(){
  while(inFlight<2&&queue.length){
   const request=queue.shift();requests.set(request.id,request);inFlight++;
   request.timer=setTimeout(()=>finish(request,''),5000);
   try{window.ttcgReadArt(request.id+'\n'+request.path);}catch{finish(request,'');}
  }
 }
 function request(path,version){
  const id=key(path,version),hit=cached(id);if(hit)return Promise.resolve(hit);
  if(!bridge())return Promise.resolve('');
  if(pending.has(id))return pending.get(id);
  let resolve;const promise=new Promise(done=>{resolve=done;});pending.set(id,promise);
  queue.push({id:++serial,key:id,path,resolve});pump();return promise;
 }
 window.ttcgArtResult=raw=>{
  let result;try{result=typeof raw==='string'?JSON.parse(raw):raw;}catch{return;}
  const request=requests.get(result?.id);if(!request)return;
  const data=typeof result.data==='string'&&/^data:image\/(png|jpeg|webp);base64,[A-Za-z0-9+/]*={0,2}$/.test(result.data)?result.data:'';
  finish(request,data);
 };
 function set(image,path,{version='',fallback='',failed=()=>{}}={}){
  const binding={};bindings.set(image,binding);
  const current=()=>bindings.get(image)===binding;
  function attempt(candidate){
   if(!current())return;
   let nativeTried=false;
   const next=()=>{if(!current())return;if(fallback&&candidate!==fallback)attempt(fallback);else{image.onerror=null;failed();}};
   const fromNative=()=>{
    nativeTried=true;
    request(candidate,version).then(data=>{if(!current())return;if(data)image.src=data;else next();});
   };
   image.onerror=()=>{if(!current())return;if(!nativeTried&&bridge())fromNative();else next();};
   const hit=cached(key(candidate,version));
   if(hit){nativeTried=true;image.src=hit;}
   else if(bridge()&&nativeFolders.has(folder(candidate)))fromNative();
   else image.src=artUrl(candidate)+(version?'?back='+encodeURIComponent(version):'');
  }
  attempt(path);
 }
 return {set,stats:()=>({entries:cache.size,bytes:cacheBytes,pending:pending.size,inFlight})};
})();
