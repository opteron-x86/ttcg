// Developer preview: uses the shipped DOM, styles and cardElement renderer.
const fs=require('node:fs'),path=require('node:path'),{pathToFileURL}=require('node:url');
const {spawnSync}=require('node:child_process'),{chromium}=require('playwright');
(async()=>{
 const fixture=spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'});
 if(fixture.status!==0)throw new Error('Build tests/ui_host.cpp as /tmp/ttcg-ui-host first.');
 const packet=JSON.parse(fixture.stdout),out=path.resolve('build/previews');fs.mkdirSync(out,{recursive:true});
 const script=`
 window.ttcgCommand=()=>{};
 ttcgState(${JSON.stringify(packet).replaceAll('<','\\u003c')});
 const review=document.createElement('main');review.id='face-review';
 review.innerHTML='<header><span>T E S S E R A</span><h1>Card faces</h1><p>Choose value placement and show or hide names in Settings.</p></header><div class="review-grid"></div>';
 document.body.append(review);
 const grid=review.querySelector('.review-grid'),choices=[['edges','shown','Edge values'],['edges','hidden','Edge values · no names'],['classic','shown','Classic values'],['classic','hidden','Classic values · no names']];
 for(const [, ,label] of choices){const title=document.createElement('h2');title.textContent=label;grid.append(title);}
 for(const [name,foil,modifier] of [['Senche-raht Graveprowler',false,0],["M'aiq the Liar",true,2],['Azura',false,-2]]){
  const id=state.cards.findIndex(c=>c.name===name&&Boolean(c.foil)===foil);
  if(id<0)throw new Error('Missing review card: '+name);
  for(const [layout,names] of choices){
   const figure=document.createElement('figure');figure.dataset.rankLayout=layout;figure.dataset.cardNames=names;
   const mount=document.createElement('div');mount.className='review-card';mount.append(cardElement(id,0,modifier));
   figure.append(mount);grid.append(figure);
  }
 }
 `;
 const css=`
 #table,#backdrop{display:none!important} body{background:#101b16;overflow:auto;user-select:auto}
 #face-review{max-width:1440px;margin:0 auto;padding:32px 40px 40px;background:radial-gradient(ellipse at 50% 30%,#29382d,#121d17 80%);border:1px solid #bba16b66}
 #face-review header{height:auto;margin:0 0 22px;padding:0 0 22px;display:block;text-align:center;border-bottom:1px solid #bba16b44}
 #face-review header>span{font-size:17px;color:var(--gold)} #face-review h1{font-size:38px;font-weight:normal;margin:14px 0 8px}
 #face-review p{font-size:18px;margin:0;color:var(--muted)} .review-grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:22px 36px}
 .review-grid h2{font-size:18px;font-weight:normal;color:var(--gold);margin:0;text-align:center}.review-grid figure{margin:0;display:flex;justify-content:center}
 .review-card{position:relative;width:192px;aspect-ratio:.76}
 @media(max-width:1000px){#face-review{padding:24px 16px}.review-grid{gap:18px 12px}.review-card{width:100%;max-width:192px}.review-grid h2{font-size:15px}}
 `;
 let html=fs.readFileSync('TesseraUI/index.html','utf8');
 html=html.replace('<head>','<head><base href="'+pathToFileURL(path.resolve('TesseraUI')+path.sep).href+'">');
 html=html.replace('</head>','<style>'+css+'</style></head>').replace('</body>','<script>'+script+'</script></body>');
 const preview=path.join(out,'card-faces.html');fs.writeFileSync(preview,html);
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{
  const page=await browser.newPage({viewport:{width:1440,height:1100},deviceScaleFactor:1}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.goto(pathToFileURL(preview).href);
  await page.waitForFunction(()=>document.querySelectorAll('#face-review .card-face').length===12);
  await page.locator('#face-review img').evaluateAll(images=>Promise.all(images.map(i=>i.decode())));
  await page.waitForTimeout(600);await page.locator('#face-review').screenshot({path:path.join(out,'card-faces.png')});
  if(errors.length)throw new Error(errors.join('\n'));
  console.log(preview);console.log(path.join(out,'card-faces.png'));
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
