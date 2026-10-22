// Bake the approved procedural SVG grains once; the game only loads static PNGs.
// Run: node tools/build-theme-textures.cjs
const fs=require('node:fs'),path=require('node:path'),{chromium}=require('playwright');
const out=path.resolve(__dirname,'../TesseraUI/assets/themes');
function svgUrl(content){return "data:image/svg+xml,"+encodeURIComponent(content);}
function noise(frequency,octaves=3,lighting=false){return svgUrl(`<svg xmlns="http://www.w3.org/2000/svg" width="384" height="384"><filter id="grain" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency="${frequency}" numOctaves="${octaves}" stitchTiles="stitch" seed="17"/><feColorMatrix type="saturate" values="0"/>${lighting?'<feDiffuseLighting surfaceScale="3" diffuseConstant="1" lighting-color="white"><feDistantLight azimuth="235" elevation="48"/></feDiffuseLighting>':''}<feComponentTransfer><feFuncA type="linear" slope="0" intercept="1"/></feComponentTransfer></filter><rect width="100%" height="100%" fill="#808080" filter="url(#grain)"/></svg>`);}
function wood(){
 let lines='';for(let i=-10;i<95;i++){
  const x=i*5.8,w=Math.sin(i*.69)*2.2;
  lines+=`<path d="M${x} -20 C${x+11+w} 158 ${x-18} 220 ${x+7} 360 S${x+18+w} 520 ${x+9} 720" stroke="${i%3?'#211306':'#e8c597'}" stroke-width="${i%4===0?1.8:.65}" opacity="${i%3?.32:.17}"/>`;
 }
 return svgUrl(`<svg xmlns="http://www.w3.org/2000/svg" width="512" height="640" viewBox="0 0 512 640"><filter id="warp"><feTurbulence baseFrequency=".009 .02" numOctaves="2" seed="9"/><feDisplacementMap in="SourceGraphic" scale="12" xChannelSelector="R" yChannelSelector="G"/></filter><filter id="grain"><feTurbulence type="fractalNoise" baseFrequency=".15 .016" numOctaves="3" seed="9"/><feColorMatrix type="saturate" values="0"/></filter><rect width="512" height="640" fill="#898077"/><rect width="512" height="640" filter="url(#grain)" opacity=".45"/><g fill="none" filter="url(#warp)">${lines}</g></svg>`);
}
const materials={fine:noise('.73',3),paper:noise('.38 .28',4),leather:noise('.12',3,true),wood:wood(),slate:noise('.046 .063',4)};
(async()=>{
 fs.mkdirSync(out,{recursive:true});
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{
  const page=await browser.newPage({deviceScaleFactor:1});
  for(const [name,url] of Object.entries(materials)){
   const width=name==='wood'?512:384,height=name==='wood'?640:384;
   await page.setViewportSize({width,height});
   await page.setContent('<style>html,body{margin:0}img{display:block}</style><img>');
   await page.locator('img').evaluate(async(img,url)=>{img.src=url;await img.decode();},url);
   await page.screenshot({path:path.join(out,name+'.png')});
   console.log(name+'.png');
  }
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
