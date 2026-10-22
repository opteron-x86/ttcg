// Copy presentation data from the existing, shared frontend reference. No browser
// or JavaScript engine is shipped or needed at runtime.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const root=path.resolve(__dirname,'..'),base=path.join(root,'TesseraUI'),out=path.join(root,'TesseraUI');
fs.mkdirSync(out,{recursive:true});
const album=fs.readFileSync(path.join(base,'album.js'),'utf8');
const app=fs.readFileSync(path.join(base,'app.js'),'utf8');
const lesson=fs.readFileSync(path.join(base,'lesson.js'),'utf8');
const ctx=vm.createContext({});
vm.runInContext(fs.readFileSync(path.join(base,'rule-demos.js'),'utf8')+';this.demos=ruleDemos;this.guided=guidedDemos;',ctx);
vm.runInContext(album.slice(album.indexOf('const TRADE_RULES='),album.indexOf('function stopRulesDemo()'))+';this.pages=RULE_PAGES;this.trades=TRADE_RULES;',ctx);
vm.runInContext(app.slice(app.indexOf('const affinityNames='),app.indexOf('function affinityIcon('))+';this.icons=affinityArtwork;this.pigments=affinityPigments;',ctx);
vm.runInContext('var state={cards:[]};'+lesson.slice(lesson.indexOf('function lessonCardName('),lesson.indexOf('function resetLesson('))+lesson.slice(lesson.indexOf('function lessonPrompt(')).replace('selected=demoSelected','selected=false'),ctx);
// Card names/sides for captions come from the same generated canonical catalog.
ctx.state.cards=JSON.parse(fs.readFileSync(path.join(root,'data/cards.json'))).cards;
for(const frames of Object.values({...ctx.demos,...ctx.guided}))for(const frame of frames)if(!frame.caption)frame.caption=ctx.lessonPrompt(frame,false);
fs.writeFileSync(path.join(out,'rules.json'),JSON.stringify({pages:ctx.pages,trades:ctx.trades,demos:ctx.demos,guided:ctx.guided}));
const icons=path.join(out,'icons');fs.mkdirSync(icons,{recursive:true});
ctx.icons.forEach((body,i)=>fs.writeFileSync(path.join(icons,`affinity-${i}.svg`),`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" color="${ctx.pigments[i]}"><style>.affinity-silhouette{fill:currentColor;stroke:#eddfbe;stroke-width:2;stroke-linejoin:round}.affinity-detail{fill:none;stroke:#eddfbe;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}.affinity-cutout{fill:#17231b}</style>${body}</svg>`));
const {execFileSync}=require('node:child_process');
for(let i=0;i<5;++i)execFileSync('rsvg-convert',['-w','200','-h','200',path.join(icons,`affinity-${i}.svg`),'-o',path.join(icons,`affinity-${i}.png`)]);
const fonts=path.join(out,'fonts');fs.mkdirSync(fonts,{recursive:true});
for(const file of ['LiberationSerif-Regular.ttf','LiberationSerif-Bold.ttf','LiberationSans-Regular.ttf','Jost-Regular.ttf','Lato-Regular.ttf'])fs.copyFileSync(path.join(root,'native/assets/fonts',file),path.join(fonts,file));
console.log('Generated native tutorial data and shared affinity artwork.');
