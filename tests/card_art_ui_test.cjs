const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process');
const fs=require('node:fs'),http=require('node:http'),path=require('node:path'),assert=require('node:assert/strict');
const root=path.resolve('TesseraUI');
const fixtures={png:fs.readFileSync('examples/card-expansion/TesseraUI/art/expansions/tessera-example/clockwork-envoy.png'),webp:fs.readFileSync(path.join(root,'art/thumbs/Enraged Mudcrab.webp')),jpg:fs.readFileSync('reference/legends/art/7ba86c7a3c43ec18d49c.jpg')};fixtures.jpeg=fixtures.jpg;
(async()=>{
 const server=http.createServer((req,res)=>{
  const name=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
  const ext=path.extname(name).slice(1).toLowerCase();
  if(/^\/art\/(cards|thumbs)\/Enraged Mudcrab\.(png|jpe?g|webp)$/i.test(name)){res.setHeader('Content-Type','image/'+(ext==='jpg'?'jpeg':ext));res.end(fixtures[ext]);return;}
  const file=path.resolve(root,'.'+name);if(!file.startsWith(root+path.sep)){res.writeHead(403).end();return;}
  try{res.end(fs.readFileSync(file));}catch{res.writeHead(404).end();}
 });
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 let browser;
 try{
  browser=await chromium.launch({headless:true,args:['--no-sandbox']});const page=await browser.newPage();const errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.exposeFunction('ttcgCommand',()=>{});
  await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
  const packet=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{encoding:'utf8',input:''}).stdout);
  await page.evaluate(s=>window.ttcgState(s),packet);
  for(const ext of ['png','jpg','jpeg','webp']){
   const result=await page.evaluate(async ext=>{
    const data={...state.cards[0],art:`cards/Enraged Mudcrab.${ext}`,thumbnail:`thumbs/Enraged Mudcrab.${ext}`};
    const full=new Image(),thumb=new Image();setCardImage(full,data);setCardImage(thumb,data,true);
    await Promise.all([full.decode(),thumb.decode()]);
    return {full:full.naturalWidth,thumb:thumb.naturalWidth,url:full.src};
   },ext);
   assert(result.full>0&&result.thumb>0);assert(result.url.endsWith(`/Enraged%20Mudcrab.${ext}`));
  }
  const punctuation=await page.evaluate(async()=>{
   const data=state.cards.find(c=>c.name==="Queen's Captain");const image=new Image();setCardImage(image,data);await image.decode();return image.naturalWidth;
  });assert(punctuation>0);
  const decoded=await page.evaluate(async()=>{
   const pending=state.cards.slice();let count=0;
   await Promise.all(Array.from({length:4},async()=>{
    while(pending.length){
     const data=pending.pop();
     for(const thumbnail of [false,true]){
      const image=new Image();setCardImage(image,data,thumbnail);await image.decode();
      if(!image.naturalWidth||!image.naturalHeight)throw Error(`Empty artwork: ${data.name}`);
      image.removeAttribute('src');count++;
     }
    }
   }));return count;
  });assert.equal(decoded,packet.cards.length*2);
  const fallback=await page.evaluate(async()=>{
   const image=new Image();setCardImage(image,{...state.cards[0],art:'cards/Enraged Mudcrab.png',thumbnail:'thumbs/intentionally-missing.webp'},true);
   await new Promise((resolve,reject)=>{image.onload=resolve;setTimeout(()=>reject(new Error('thumbnail fallback timed out')),3000);});return image.src;
  });assert(fallback.endsWith('/cards/Enraged%20Mudcrab.png'));
  assert.deepEqual(errors,[]);
  console.log(`Browser art: ${decoded} catalog faces/thumbnails decoded; PNG/JPG/JPEG/WebP, spaces/apostrophes and missing-thumbnail fallback passed`);
 }finally{if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(e=>{console.error(e);process.exitCode=1;});
