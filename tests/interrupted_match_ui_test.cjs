const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url');
const path=require('node:path'),assert=require('node:assert/strict');
function tournamentPacket(input){const run=spawnSync('/tmp/ttcg-tournament-host',{input,encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(run.status,0,run.stderr||run.error?.message);return JSON.parse(run.stdout.trim().split('\n').at(-1));}
const setup='test-final-setup 1 0 0 3\n',beforeFinal=tournamentPacket(setup);
const final=tournamentPacket(setup+`play ${beforeFinal.session} ${beforeFinal.revision} ${beforeFinal.testFinalMove.hand} ${beforeFinal.testFinalMove.square}\n`);
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try {for(const [width,height] of [[1440,900],[1280,720],[3840,2160]]){
  const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];
  const s=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'}).stdout.trim());
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',command=>commands.push(command));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  s.screen='album';s.matchContext=false;s.collection.staked=true;
  s.collection.packs=[1,1];
  s.interruptedMatch={opponent:'M’aiq the Liar',tournament:0,finished:false,venue:'',canResume:false};
  const update=()=>page.evaluate(s=>ttcgState(s),s);await update();
  const banner=page.locator('#interrupted-match');assert(await banner.isVisible());
  assert.equal(await page.locator('#interrupted-match-copy').innerText(),'Resume your game with M’aiq the Liar.');
  assert(await page.locator('#interrupted-resume').isHidden());
  const bounds=await banner.boundingBox();assert(bounds.x>=0&&bounds.x+bounds.width<=width&&bounds.y>=0);
  assert(await page.locator('#album-decks-tab').isDisabled());
  await page.locator('#album-card-pack').click();assert(await page.locator('#inspect-open-pack').isDisabled());
  await page.evaluate(()=>setAlbumTab('decks'));assert.equal(await page.locator('#album').getAttribute('data-view'),'cards');
  await page.locator('#interrupted-forfeit').click();assert(await page.locator('#forfeit').isVisible());
  const count=commands.length;await page.locator('#keep-playing').click();assert.equal(commands.length,count);
  await page.locator('#interrupted-forfeit').click();await page.locator('#confirm-forfeit').click();
  assert(commands.at(-1).endsWith('interrupted '+s.session+' '+s.revision+' forfeit'));
  assert(await page.locator('#forfeit').isHidden());
  await page.locator('#leave').click();assert(commands.at(-1).endsWith('close '+s.session+' '+s.revision),'Closing the album leaves the reserved game alone');
  s.session++;s.interruptedMatch={opponent:'Amren',tournament:1,finished:false,venue:'The Bannered Mare',canResume:true};await update();
  assert.equal(await page.locator('#interrupted-match-copy').innerText(),'Resume your game at The Bannered Mare.');
  await page.locator('#interrupted-resume').click();assert(commands.at(-1).endsWith(' resume'));
  s.interruptedMatch.finished=true;s.session++;await update();assert(await page.locator('#interrupted-forfeit').isHidden());
  assert.equal(await page.locator('#interrupted-resume').innerText(),'Collect rewards');
  s.interruptedMatch=null;s.screen='lobby';s.phase='match';s.collection.staked=false;s.session++;await update();
  assert(await banner.isHidden());assert(await page.locator('#game').isVisible());assert(await page.locator('#album-settings-tab').isHidden());
  // An interruption after the winning move must not strand the foil picker or
  // open it before resuming. Rehydrate a real native best-of-three final.
  const event=final.collection.tournaments.events.find(e=>e.champion===20);assert(event&&event.foilUpgrades===1);
  const interrupted=structuredClone(final);interrupted.session=s.session+1;interrupted.phase='ready';interrupted.screen='album';interrupted.albumSection='tournaments';interrupted.matchContext=false;interrupted.settling=false;interrupted.collection.tournamentID=0;
  interrupted.interruptedMatch={opponent:final.opponent,tournament:event.id,finished:true,venue:event.venue,canResume:false};
  await page.evaluate(s=>ttcgState(s),interrupted);
  assert(await page.locator('#tournament-outcome').isHidden());assert(await page.locator('#tournament-action').isDisabled());
  interrupted.interruptedMatch.canResume=true;interrupted.revision++;await page.evaluate(s=>ttcgState(s),interrupted);
  assert.equal(await page.locator('#tournament-action').innerText(),'Collect rewards');
  await page.locator('#tournament-action').click();assert(commands.at(-1).endsWith('interrupted '+interrupted.session+' '+interrupted.revision+' resume'));
  const resumed=structuredClone(final);resumed.session=interrupted.session+1;resumed.revision=0;resumed.albumSection='';resumed.interruptedMatch=null;resumed.stages=[];resumed.rule='';resumed.board.forEach(c=>{if(c)c.flip=false;});
  commands.length=0;await page.evaluate(s=>ttcgState(s),resumed);
  await page.waitForFunction(()=>document.getElementById('tournament-outcome').hidden&&document.getElementById('game').hidden===false);
  for(let i=0;i<100&&!commands.some(c=>c.startsWith('settled '));i++)await page.waitForTimeout(50);
  assert.equal(commands.filter(c=>c.startsWith('settled ')).length,1,'Restored result acknowledges completion exactly once');
  resumed.settling=false;await page.evaluate(s=>ttcgState(s),resumed);
  await page.waitForFunction(()=>!document.getElementById('tournament-outcome').hidden);
  assert.equal(await page.locator('#tournament-outcome-title').innerText(),'Tournament won');assert(await page.locator('#tournament-foil-reward').isVisible());
  assert.equal(await page.locator('#tournament-foil-remaining').innerText(),'1 foil upgrade remaining');
  await page.evaluate(()=>ttcgReset());assert(await banner.isHidden());assert.deepEqual(errors,[]);
  console.log(`${width}x${height}: interrupted-game notice, mutation locks, explicit forfeit, safe album close, restored final/foil reward and cleanup passed`);await page.close();
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
