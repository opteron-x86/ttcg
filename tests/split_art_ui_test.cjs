const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn,spawnSync}=require('node:child_process'),{createInterface}=require('node:readline');
const fs=require('node:fs'),http=require('node:http'),path=require('node:path'),assert=require('node:assert/strict');

(async()=>{
 fs.mkdirSync('build/split-art',{recursive:true});
 const temporary=fs.mkdtempSync(path.resolve('build/split-art/install-'));
 const main=path.join(temporary,'Main'),art=path.join(temporary,'Card Art'),ui=path.join(main,'TesseraUI');
 const selection=spawnSync('python3',['-c',"import sys,json;sys.path.insert(0,'tools');from release_files import collect_release;print(json.dumps([{k:str(v) for k,v in files.items() if k.startswith('TesseraUI/')} for files in collect_release()]))"],{encoding:'utf8'});
 assert.equal(selection.status,0,selection.stderr);
 const [mainFiles,artFiles]=JSON.parse(selection.stdout);
 for(const [files,root] of [[mainFiles,main],[artFiles,art]])for(const [name,source] of Object.entries(files)){
  const destination=path.join(root,name);fs.mkdirSync(path.dirname(destination),{recursive:true});
  fs.linkSync(source,destination);
 }
 // The native release intentionally contains no browser. This retained authoring
 // reference test supplies its own HTML/JS/CSS fixture, outside the release list.
 for(const name of fs.readdirSync('TesseraUI'))if(/\.(html|js|css|svg)$/.test(name))fs.copyFileSync(path.join('TesseraUI',name),path.join(ui,name));
 for(const folder of ['cards','thumbs'])fs.symlinkSync(path.join(art,'TesseraUI/art',folder),path.join(ui,'art',folder),'dir');
 const custom="backs/custom/Moon & Star's_夜 100%.WEBP";
 const customRoot=path.join(art,'TesseraUI/art/backs/custom');fs.mkdirSync(customRoot,{recursive:true});
 fs.copyFileSync(path.join(ui,'art/backs/dragon.webp'),path.join(customRoot,path.basename(custom)));
 fs.symlinkSync(customRoot,path.join(ui,'art/backs/custom'),'dir');
 const packet=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{encoding:'utf8',input:''}).stdout);
 packet.screen='album';packet.opponent='';packet.matchContext=false;
 Object.assign(packet.collection,{owned:packet.cards.map((_,i)=>i<30?1:0),known:packet.cards.map((_,i)=>i<30),packs:[0,0],revealed:[],revealID:0});
 let rejected=0;
 const server=http.createServer((req,res)=>{
  try{
   const name=decodeURIComponent(new URL(req.url,'http://localhost').pathname),file=path.resolve(ui,'.'+name),physical=fs.realpathSync(file);
   if(!file.startsWith(ui+path.sep)||!physical.startsWith(ui+path.sep)){rejected++;res.writeHead(404).end();return;}
   const mime={'.js':'text/javascript','.css':'text/css','.html':'text/html','.png':'image/png','.webp':'image/webp','.svg':'image/svg+xml'}[path.extname(file)]||'application/octet-stream';
   res.setHeader('Content-Type',mime);res.end(fs.readFileSync(file));
  }catch{res.writeHead(404).end();}
 });
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const address=`http://127.0.0.1:${server.address().port}/index.html`;
 let browser,host,page;
 try{
  browser=await chromium.launch({headless:true,args:['--no-sandbox']});
  // Control: complete files are visible in the virtual tree, but the old
  // browser-only path cannot render images from the separate physical root.
  const control=await browser.newPage();await control.exposeFunction('ttcgCommand',()=>{});
  await control.goto(address);await control.evaluate(s=>ttcgState(s),packet);
  await control.waitForFunction(()=>{const img=document.querySelector('#album-grid .collection-card img');return img?.complete;});
  assert.equal(await control.locator('#album-grid .collection-card img').first().evaluate(img=>img.naturalWidth),0);
  assert(rejected>0);await control.close();

  page=await browser.newPage({viewport:{width:1440,height:900},reducedMotion:'reduce'});
  const errors=[],sent=[],delivering=new Set();let nativeInFlight=0,peak=0,slow='';
  page.on('pageerror',e=>errors.push(e.message));
  host=spawn('/tmp/ttcg-art-transfer-host',[path.join(ui,'art')],{stdio:['pipe','pipe','inherit']});
  const requests=new Map();
  createInterface({input:host.stdout}).on('line',line=>{
   const reply=JSON.parse(line),request=requests.get(reply.id);requests.delete(reply.id);nativeInFlight--;
   if(reply.error&&request!=='thumbs/missing.webp')errors.push(`${request}: ${reply.error}`);
   const delay=request===slow?100:0;
   const task=new Promise(resolve=>setTimeout(resolve,delay)).then(()=>page.evaluate(reply=>ttcgArtResult(reply),reply));
   delivering.add(task);task.catch(e=>errors.push(e.message)).finally(()=>delivering.delete(task));
  });
  await page.exposeFunction('ttcgCommand',()=>{});
  await page.exposeFunction('ttcgReadArt',text=>{
   const [id,file]=text.split('\n');sent.push(file);requests.set(Number(id),file);nativeInFlight++;peak=Math.max(peak,nativeInFlight);host.stdin.write(text+'\n');
  });
  await page.goto(address);await page.evaluate(s=>ttcgState(s),packet);
  const first=page.locator('#album-grid .collection-card img').first();
  await first.waitFor();await page.waitForFunction(()=>document.querySelector('#album-grid .collection-card img')?.naturalWidth>0);
  assert((await first.getAttribute('src')).startsWith('data:image/webp;base64,'));
  await page.locator('#album-grid .collection-card').first().click();
  await page.waitForFunction(()=>document.querySelector('#inspect-card img')?.naturalWidth>0);
  assert((await page.locator('#inspect-card img').getAttribute('src')).startsWith('data:image/webp;base64,'));
  await page.locator('#inspect-card').click();await page.waitForFunction(()=>document.querySelector('.art-front img')?.naturalWidth>0);
  await page.keyboard.press('Escape');
  // Concurrent requests for the same file share one native read.
  const sample=packet.cards[400],before=sent.filter(p=>p===sample.art).length;
  await page.evaluate(async data=>{
   await Promise.all(Array.from({length:12},()=>new Promise((resolve,reject)=>{const img=new Image();img.onload=resolve;artImages.set(img,data.art,{failed:()=>reject(Error('image failed'))});})));
  },sample);
  assert.equal(sent.filter(p=>p===sample.art).length-before,1);
  // A delayed response cannot overwrite a newer binding on a reused image.
  slow=packet.cards[401].art;
  const current=await page.evaluate(async ([oldCard,newCard])=>{
   const img=new Image();artImages.set(img,oldCard.art);artImages.set(img,newCard.art);
   await new Promise(resolve=>setTimeout(resolve,250));await img.decode();return img.src;
  },[packet.cards[401],sample]);
  assert.equal(current,'data:image/webp;base64,'+fs.readFileSync(path.join(ui,'art',sample.art)).toString('base64'));
  slow='';
  // Missing thumbnails still recover to a full face through the same reader.
  await page.evaluate(async data=>{
   const img=new Image();const loaded=new Promise((resolve,reject)=>{img.onload=resolve;setTimeout(()=>reject(Error('fallback timeout')),2000);});
   setCardImage(img,{...data,thumbnail:'thumbs/missing.webp'},true);await loaded;
   if(!img.src.startsWith('data:image/webp;'))throw Error('Full-art fallback was not used');
  },packet.cards[402]);
  packet.cardBack=custom;packet.cardBackID='custom/moon';packet.cardBackVersion++;
  packet.cardBacks.push({id:packet.cardBackID,name:'Moon',art:custom,custom:true});packet.revision++;
  await page.evaluate(s=>ttcgState(s),packet);
  await page.waitForFunction(()=>getComputedStyle(document.documentElement).getPropertyValue('--card-back-image').includes('blob:')).catch(async error=>{
   console.error({errors,backs:sent.filter(p=>p.startsWith('backs/')),state:await page.evaluate(()=>({path:state.cardBack,error:cardBackImageError,key:cardBackImageKey,style:document.documentElement.style.getPropertyValue('--card-back-image').slice(0,100),stats:artImages.stats()}))});throw error;
  });
  assert.equal(await page.evaluate(async()=>{const img=new Image();img.src=cardBackObjectURL;await img.decode();return img.naturalWidth;}),1086);
  await page.locator('#album-settings-tab').click();
  await page.waitForFunction(()=>document.querySelector('[data-back="custom/moon"]')?.dataset.loaded==='true');
  await page.locator('#album-cards-tab').click();
  // Browsing more than the cache capacity keeps reads bounded and legal.
  await page.evaluate(async cards=>{
   await Promise.all(cards.map(data=>new Promise((resolve,reject)=>{const img=new Image();img.onload=resolve;artImages.set(img,data.thumbnail,{failed:()=>reject(Error('thumbnail failed'))});})));
  },packet.cards.slice(100,210));
  await page.waitForFunction(()=>artImages.stats().pending===0);
  const stats=await page.evaluate(()=>artImages.stats());assert(stats.entries<=96&&stats.bytes<=32*1024*1024);assert(peak<=2);
  await Promise.all([...delivering]);assert.deepEqual(errors,[]);
  await page.screenshot({path:'build/previews/split-card-art.png'});
  console.log('Split-art browser: reproduced physical-root rejection; native reader restores thumbnails, faces, inspection and Unicode backs; deduplication, stale responses, fallback and bounded cache passed');
 }finally{
  if(host){host.stdin.end();host.kill();}if(browser)await browser.close();
  await new Promise(resolve=>server.close(resolve));fs.rmSync(temporary,{recursive:true,force:true});
 }
})().catch(e=>{console.error(e);process.exitCode=1;});
