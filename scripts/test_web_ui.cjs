// Exercise the embedded production JavaScript without network or radio hardware.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../src/web_server.cpp'), 'utf8');
const page = [...source.matchAll(/R"HTML\(([\s\S]*?)\)HTML"/g)].map(m=>m[1]).join('');
const script = page.slice(page.indexOf('<script>')+8, page.lastIndexOf('</script>'));
new vm.Script(script); // Includes every production handler, not a copied fixture.
const ids = [...page.matchAll(/\bid="([^"]+)"/g)].map(m=>m[1]);
assert.equal(ids.length, new Set(ids).size, 'duplicate element IDs');
for(const match of page.matchAll(/for="([^"]+)"/g)) assert(ids.includes(match[1]));
for(const match of page.matchAll(/data-tab="([^"]+)"/g)) assert(ids.includes('tab-'+match[1]));
function element(id) {
  return {id, value:'', textContent:'', checked:true, disabled:false, hidden:false,
    options:['AUTO','NFM','WFM','AM'].map(value=>({value})),
    classList:{toggle(){},add(){},remove(){}}};
}
const elements = Object.fromEntries(ids.map(id=>[id,element(id)]));
const modes=['AUTO','NFM','WFM','P25'].map(mode=>({...element(mode),getAttribute:()=>mode}));
const context=vm.createContext({console, AbortSignal, Date, Map, Set,
  document:{getElementById:id=>elements[id],querySelectorAll:()=>modes,activeElement:null},
  sdrTownConfig:{enabled:true,allowTune:true,allowMode:true,allowP25Control:true,allowRfGain:true},
  sdrTownConnected:true,sdrControlSession:{canControl:true,extensionsUsed:0,maxExtensions:2},
  sdrCommandInFlight:false, sdrTownLiveState:{}, sdrControlSeededForLease:false,
  sdrControlWasActive:true,sdrplayDraftSeeded:true, sdrControlDeadlineMs:0,
  controlPollBusy:false,sdrDesiredMode:'',sdrControlClientId:'test',sdrControlName:'Test',
  renderDecodePanels(){},sdrRenderDevicePanels(){},
});
function load(name) {
  const match=script.match(new RegExp('^(?:async )?function '+name+'\\([^]*?^}', 'm'));
  assert(match, 'production function '+name);
  vm.runInContext(match[0],context);
}
for(const name of ['sdrEffectiveMode','sdrHighlightMode','sdrSetControlsEnabled','sdrTownMessage',
  'sdrControlMs','sdrRenderControlSession','sdrAdoptSession','sdrPopulateP25ControlChannels',
  'sdrSeedFields','sdrControlAction','loadSdrTownControl','postSdrTown']) load(name);
elements.sdrMode.value='WFM';
context.sdrHighlightMode('AUTO');
assert.equal(elements.sdrMode.value,'WFM','poll must retain owner draft');
context.sdrControlSession.canControl=false;
context.sdrHighlightMode('AUTO');
assert.equal(elements.sdrMode.value,'AUTO','observer sees actual mode');
context.sdrSetControlsEnabled();
for(const id of ['sdrTuneBtn','sstvReceiveBtn','sstvFinishBtn','inmStartBtn','satStartBtn','acTuneBtn'])
  assert(elements[id].disabled,id+' must require lease');
context.sdrControlSession.canControl=true;
context.sdrSetControlsEnabled();
assert(!elements.sdrTuneBtn.disabled);
context.sdrTownConnected=false;context.sdrSetControlsEnabled();
assert(elements.sstvReceiveBtn.disabled,'offline must disable actions');
context.sdrTownConnected=true;
context.sdrTownMessage('Command failed');context.sdrTownMessage('Routine ready',true);
assert.equal(elements.commandFeedback.textContent,'Command failed','poll hides command error');
context.sdrControlSeededForLease=true;
elements.sdrFreq.value='98.1';
context.sdrSeedFields({frequencyMHz:100,mode:'AUTO'});
assert.equal(elements.sdrFreq.value,'98.1');
context.sdrAdoptSession({canControl:false});
assert.equal(context.sdrControlSeededForLease,false);
assert.equal(context.sdrplayDraftSeeded,false);
(async()=>{
  let requests=0;
  context.fetch=async()=>{requests++;throw new Error('offline');};
  context.controlPollBusy=true;
  await context.loadSdrTownControl();assert.equal(requests,0,'overlapping status poll');
  context.controlPollBusy=false;
  await context.loadSdrTownControl();
  assert.equal(context.controlPollBusy,false,'poll lock released after failure');
  assert.equal(context.sdrTownConnected,false,'network failure must not report ready');
  context.sdrControlSession={canControl:true};context.sdrTownConnected=true;
  context.fetch=async()=>({status:200,text:async()=>JSON.stringify({ok:false,error:'Test rejection'})});
  await context.postSdrTown('test',{});
  assert.equal(elements.commandFeedback.textContent,'Test rejection');
  assert.equal(context.sdrCommandInFlight,false,'command lock released');
  context.fetch=async()=>{throw new Error('timeout');};
  await context.postSdrTown('test',{});
  assert.equal(context.sdrCommandInFlight,false);
  assert.match(elements.commandFeedback.textContent,/timeout/);
  // Map markers retain identity across refreshes; an API error is not an empty map.
  context.inmMap={setView(){}};context.inmMapCentered=false;
  context.inmMarkers=new Map();
  const removed=[];
  context.inmLayer={removeLayer:m=>removed.push(m),clearLayers:()=>removed.push('all')};
  context.ensureInmarsatMap=()=>{};
  context.esc=s=>String(s).replaceAll('<','&lt;');
  context.L={circleMarker:(coords,style)=>({coords,style,
    bindTooltip(text){this.text=text;return this;},addTo(){return this;},
    setLatLng(coords){this.coords=coords;return this;},setStyle(style){this.style=style;return this;},
    setTooltipContent(text){this.text=text;return this;}})};
  load('loadInmarsatMap');
  let mapData={ok:true,positions:[{icaoHex:'ABC123',latDeg:1,lonDeg:2,voiceActive:true}],aircraft:[]};
  context.fetch=async()=>({json:async()=>mapData});
  await context.loadInmarsatMap();
  const marker=context.inmMarkers.get('ABC123');assert(marker);
  assert.equal(marker.style.color,'#88d6aa','active voice colour');
  mapData.positions[0].latDeg=2;
  await context.loadInmarsatMap();
  assert.equal(context.inmMarkers.get('ABC123'),marker,'marker must not flash on refresh');
  assert.equal(marker.coords[0],2);
  mapData.positions=[{icaoHex:'BAD',latDeg:null,lonDeg:3},{icaoHex:'BAD2',latDeg:91,lonDeg:0}];
  await context.loadInmarsatMap();assert.equal(context.inmMarkers.size,0);
  mapData={ok:false,error:'Disconnected'};
  await context.loadInmarsatMap();assert.match(elements.inmMapStatus.textContent,/unavailable/);
  // A disconnected bootstrap must recover its channel list without a page reload.
  for (const id of ['inmPlan','inmChannel']) {
    const el=elements[id];el.options=[];el.value='';
    Object.defineProperty(el,'innerHTML',{set(){this.options=[];this.value='';}});
    el.appendChild=function(o){this.options.push(o);if(this.options.length===1)this.value=o.value;};
  }
  context.document.createElement=()=>({dataset:{}});
  context.inmPlansCache=[];context.inmPlansBusy=false;
  load('fillInmarsatChannels');load('loadInmarsatPlans');
  context.fetch=async()=>({ok:false,json:async()=>({error:'offline'})});
  await context.loadInmarsatPlans();assert.equal(context.inmPlansBusy,false);
  assert.equal(context.inmPlansCache.length,0);
  context.fetch=async()=>({ok:true,json:async()=>({plans:[{id:'test',channels:[
    {freqHz:1546000000},{freqHz:1546100000}]}]})});
  await context.loadInmarsatPlans();assert.equal(elements.inmChannel.options.length,2);
  elements.inmChannel.value='1546100000';
  await context.loadInmarsatPlans();assert.equal(elements.inmChannel.value,'1546100000');
  assert(script.includes('if (!inmPlansCache.length) await loadInmarsatPlans();'));
  // Resume must not replace src (which resets the media position).
  context.items=[{id:'clip',name:'test.wav',started:'today',mode:'mono'}];
  context.current='';context.now=element('now');let sourceAssignments=0;
  context.audio={paused:true,play:async function(){this.paused=false;},pause(){this.paused=true;}};
  Object.defineProperty(context.audio,'src',{set(){sourceAssignments++;}});
  context.document.querySelector=()=>element('player');context.stopLive=()=>{};
  context.render=()=>{};load('play');
  await context.play('clip');assert.equal(sourceAssignments,1);
  await context.play('clip');assert(context.audio.paused);
  await context.play('clip');assert.equal(sourceAssignments,1,'resume must not restart');
  context.audio.paused=true;context.audio.play=async()=>{throw new Error('blocked');};
  await context.play('clip');assert.match(context.now.textContent,/Playback unavailable/);
  console.log('Web UI: syntax, IDs, tabs, drafts, permissions, polling and error recovery PASS');
})().catch(error=>{console.error(error);process.exitCode=1;});
