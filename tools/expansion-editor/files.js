'use strict';
function createExpansionFiles(core) {
 const MAX_ARCHIVE = 512*1024*1024, MAX_IMAGE = 32*1024*1024, MAX_JSON = 2*1024*1024;
 const enc = new TextEncoder(), dec = new TextDecoder('utf-8',{fatal:true});
 const imageName = name => /\.(png|jpe?g|webp)$/i.test(name);
 const yieldFrame = () => new Promise(resolve=>setTimeout(resolve,0));
 const canvasBlob = (canvas,quality=.94) => new Promise((resolve,reject)=>canvas.toBlob(b=>b&&b.type==='image/webp'?resolve(b):reject(Error('This browser cannot export WebP. Use a current Chrome, Edge or Firefox.')),'image/webp',quality));
 async function importImage(file,path=file.name) {
  if (!imageName(path)) throw Error('Use PNG, JPEG or WebP artwork.');
  if (file.size>MAX_IMAGE) throw Error(file.name+' exceeds 32 MiB.');
  const bitmap = await createImageBitmap(file);
  try {
   if(bitmap.width*bitmap.height>40_000_000)throw Error(file.name+' is larger than 40 megapixels.');
   const ratio=Math.min(1,2048/Math.max(bitmap.width,bitmap.height)), canvas=document.createElement('canvas');
   canvas.width=Math.round(bitmap.width*ratio);canvas.height=Math.round(bitmap.height*ratio);
   const ctx=canvas.getContext('2d');ctx.imageSmoothingQuality='high';ctx.drawImage(bitmap,0,0,canvas.width,canvas.height);
   return {id:core.uid(),name:path.split('/').pop(),path,width:canvas.width,height:canvas.height,blob:await canvasBlob(canvas,.97)};
  } finally {bitmap.close();}
 }
 function drawCrop(ctx,bitmap,crop,width,height) {
  const scale=Math.max(width/bitmap.width,height/bitmap.height)*crop.zoom;
  const w=bitmap.width*scale,h=bitmap.height*scale;
  ctx.imageSmoothingQuality='high';ctx.drawImage(bitmap,(width-w)*crop.x/100,(height-h)*crop.y/100,w,h);
 }
 async function renderArt(asset,crop) {
  const bitmap=await createImageBitmap(asset.blob);
  try {
   const result=[];
   for(const [width,height] of [[768,1024],[240,320]]) {
    const canvas=document.createElement('canvas');canvas.width=width;canvas.height=height;
    drawCrop(canvas.getContext('2d'),bitmap,crop,width,height);
    result.push(new Uint8Array(await (await canvasBlob(canvas)).arrayBuffer()));
   }
   return result;
  } finally {bitmap.close();}
 }
 function safePath(path) {
  if(typeof path!=='string'||path.length>512||/^[\/]|\\|[\x00-\x1f:]/.test(path)||path.split('/').some(p=>p==='..'||p==='.') ) throw Error('Unsafe archive path.');
  return path;
 }
 async function readZIP(file) {
  if(file.size>MAX_ARCHIVE)throw Error('Archive exceeds 512 MiB.');
  let total=0, count=0;const names=new Set();
  const files=fflate.unzipSync(new Uint8Array(await file.arrayBuffer()),{filter:entry=>{
   safePath(entry.name);
   const lower=entry.name.toLowerCase();if(names.has(lower))throw Error('Duplicate archive path: '+entry.name);names.add(lower);
   if(++count>5000)throw Error('Too many files in archive.');
   if(!imageName(entry.name)&&!entry.name.endsWith('.json'))return false;
   if(entry.originalSize>(imageName(entry.name)?MAX_IMAGE:MAX_JSON*4))throw Error('An archive entry is too large: '+entry.name);
   total+=entry.originalSize;if(total>MAX_ARCHIVE)throw Error('Expanded archive exceeds 512 MiB.');
   return true;
  }});
  return new Map(Object.entries(files).map(([name,bytes])=>[name,new Blob([bytes],{type:imageName(name)?'application/octet-stream':'application/json'})]));
 }
 async function fileMap(input) {
  const list=[...input];if(list.length===1&&/\.zip$/i.test(list[0].name))return readZIP(list[0]);
  if(list.length>5000)throw Error('Too many files in this folder.');
  const map=new Map();for(const f of list){const name=safePath(f.webkitRelativePath||f.path||f.name);if(map.has(name))throw Error('Duplicate input path.');map.set(name,f);}return map;
 }
 async function readJSON(blob,limit=MAX_JSON) {
  if(blob.size>limit)throw Error('JSON file exceeds its size limit.');return core.parseJSON(dec.decode(await blob.arrayBuffer()));
 }
 function zip(entries) {
  const total=Object.values(entries).reduce((n,b)=>n+b.length,0);
  if(total>MAX_ARCHIVE)throw Error('Export exceeds 512 MiB. Split it into smaller expansion sets.');
  // WebP is already compressed; stored ZIP entries keep large rosters responsive.
  return new Blob([fflate.zipSync(entries,{level:0})],{type:'application/zip'});
 }
 async function exportMod(project,assets,progress) {
  const errors=core.issues(project).filter(e=>e.level==='error');if(errors.length)throw Error(errors[0].message);
  const manifest=core.manifest(project);core.validateManifest(manifest);
  const entries=Object.create(null);entries['SKSE/Plugins/Tessera/Expansions/'+project.setId+'.json']=enc.encode(JSON.stringify(manifest,null,2)+'\n');
  if(entries[Object.keys(entries)[0]].length>MAX_JSON)throw Error('The manifest exceeds 2 MiB.');
  for(const [i,c] of project.cards.entries()) {
   progress('Preparing '+(c.name||'card'),i,project.cards.length);await yieldFrame();
   if(!c.asset)continue;const asset=assets.get(c.asset);if(!asset)throw Error('Artwork is missing for '+c.name+'.');
   const [face,thumb]=await renderArt(asset,c.crop),base='TesseraUI/art/expansions/'+project.setId+'/';
   entries[base+'faces/'+c.id+'.webp']=face;entries[base+'thumbs/'+c.id+'.webp']=thumb;
  }
  return zip(entries);
 }
 async function exportProject(project,assets,progress) {
  const entries=Object.create(null);entries['project.json']=enc.encode(JSON.stringify(project,null,2)+'\n');
  for(const [i,a] of project.assets.entries()) {
   progress('Saving artwork',i,project.assets.length);await yieldFrame();const asset=assets.get(a.id);
   if(!asset)throw Error('Project artwork is missing: '+a.name);
   entries['artwork/'+a.id+'.webp']=new Uint8Array(await asset.blob.arrayBuffer());
  }
  return zip(entries);
 }
 async function readImport(input,progress) {
  const map=await fileMap(input), jsons=[...map].filter(([p])=>p.endsWith('.json'));
  let projectEntry=jsons.find(([p])=>p==='project.json'||p.endsWith('/project.json'));
  if(projectEntry) {
   const raw=await readJSON(projectEntry[1],MAX_JSON*4);
   if(raw.kind==='tessera-expansion-project') {
    const project=core.readProject(raw), prefix=projectEntry[0].slice(0,-'project.json'.length),assets=new Map();
    for(const [i,a] of project.assets.entries()) {
     progress('Restoring '+a.name,i,project.assets.length);await yieldFrame();
     const blob=map.get(prefix+'artwork/'+a.id+'.webp');if(!blob)throw Error('Project backup is missing '+a.name+'.');
     const bitmap=await createImageBitmap(blob);if(bitmap.width!==a.width||bitmap.height!==a.height){bitmap.close();throw Error('Artwork dimensions do not match the project.');}bitmap.close();
     assets.set(a.id,{...a,blob});
    }
    return {project,assets,missing:0};
   }
  }
  const candidates=jsons.filter(([p])=>/(^|\/)SKSE\/Plugins\/Tessera\/Expansions\/[^/]+\.json$/.test(p));
  const entry=candidates.length===1?candidates[0]:jsons.length===1?jsons[0]:null;
  if(!entry)throw Error('Choose a project ZIP or one expansion manifest with its artwork. Multiple sets must be opened separately.');
  const project=core.fromManifest(await readJSON(entry[1])),assets=new Map(),byPath=new Map();let missing=0;
  const marker='SKSE/Plugins/Tessera/Expansions/', at=entry[0].indexOf(marker),prefix=at<0?'':entry[0].slice(0,at);
  for(const [i,c] of project.cards.entries()) {
   if(!c.missingArt)continue;progress('Importing '+c.name,i,project.cards.length);await yieldFrame();
   const art=c.missingArt, expected=prefix+'TesseraUI/art/'+art;
   let matches=[...map].filter(([p])=>p===expected||p==='TesseraUI/art/'+art||p===art||p.endsWith('/TesseraUI/art/'+art));
   // Import old releases so authors can re-export to TesseraUI without changing
   // permanent card IDs. Exports and the game itself use only TesseraUI.
   if(!matches.length)matches=[...map].filter(([p])=>p===prefix+'MeridianUI/ttcg/art/'+art||p==='MeridianUI/ttcg/art/'+art||p.endsWith('/MeridianUI/ttcg/art/'+art));
   if(matches.length!==1){missing++;continue;}
   const [path,blob]=matches[0];let asset=byPath.get(path);
   if(!asset){asset=await importImage(blob,path);assets.set(asset.id,asset);byPath.set(path,asset);}
   c.asset=asset.id;c.placeholder=false;c.missingArt='';
  }
  project.assets=[...assets.values()].map(({blob,...meta})=>meta);
  return {project,assets,missing};
 }
 async function droppedFiles(transfer) {
  const entries=[...transfer.items].map(item=>item.webkitGetAsEntry?.()).filter(Boolean);
  if(!entries.length)return [...transfer.files];
  const out=[];
  async function walk(entry,prefix='') {
   if(out.length>5000)throw Error('Too many files in the dropped folder.');
   if(entry.isFile){const file=await new Promise((resolve,reject)=>entry.file(resolve,reject));Object.defineProperty(file,'path',{value:prefix+file.name});out.push(file);}
   else if(entry.isDirectory){const reader=entry.createReader();let batch;do{batch=await new Promise((resolve,reject)=>reader.readEntries(resolve,reject));for(const child of batch)await walk(child,prefix+entry.name+'/');}while(batch.length);}
  }
  for(const entry of entries)await walk(entry);return out;
 }
 return {importImage,drawCrop,renderArt,exportMod,exportProject,readImport,droppedFiles,imageName,yieldFrame};
}
