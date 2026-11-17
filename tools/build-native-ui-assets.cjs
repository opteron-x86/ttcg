// Write the data the native interface reads: the Rules tab's text and demos
// (TesseraUI/rules.json), the affinity icons and the fonts. No browser or JavaScript
// engine is shipped or needed at runtime.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const root=path.resolve(__dirname,'..'),rules=path.join(root,'tools/rules'),out=path.join(root,'TesseraUI');
fs.mkdirSync(out,{recursive:true});
const read=file=>fs.readFileSync(file,'utf8');
const renderer=read(path.join(root,'tools/shared/card-renderer.js'));
const ctx=vm.createContext({});
// rule-demos.js is written by tests/rule_demo_host.cpp from the game's own engine.
vm.runInContext(read(path.join(rules,'rule-demos.js'))+';this.demos=ruleDemos;this.guided=guidedDemos;',ctx);
vm.runInContext(read(path.join(rules,'rule-pages.js'))+';this.pages=RULE_PAGES;this.trades=TRADE_RULES;',ctx);
vm.runInContext(renderer.slice(renderer.indexOf('const affinityNames='),renderer.indexOf('function affinityIcon('))+';this.icons=affinityArtwork;this.pigments=affinityPigments;',ctx);
vm.runInContext('var state={cards:[]};'+read(path.join(rules,'lesson-prompts.js')),ctx);
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
