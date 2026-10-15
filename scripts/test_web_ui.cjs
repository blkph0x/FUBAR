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
assert(source.includes('api/sdr-town/inmarsat-sessions'),'Inmarsat named-session route must remain wired');
assert(script.includes('inmarsat-sessions'),'Inmarsat named-session client must remain wired');
assert(script.includes('sstvSessionId'),'SSTV named-session client must remain wired');
assert(script.includes("captureBandwidthMHz:rate"),'1090 tune must use the selected sample rate');
function element(id) {
  const classes=new Set();
  return {id, value:'', textContent:'', checked:true, disabled:false, hidden:false, dataset:{},
    replaceChildren(){this.options=[];this.value='';},appendChild(o){this.options.push(o);},
    options:['AUTO','NFM','WFM','AM'].map(value=>({value})),
    classList:{toggle(name,on){if(on)classes.add(name);else classes.delete(name);},
      add(name){classes.add(name);},remove(name){classes.delete(name);},contains(name){return classes.has(name);}}};
}
const elements = Object.fromEntries(ids.map(id=>[id,element(id)]));
const modes=['AUTO','NFM','WFM','P25'].map(mode=>({...element(mode),getAttribute:()=>mode}));
const context=vm.createContext({console, AbortSignal, Date, Map, Set,
  document:{getElementById:id=>elements[id],querySelectorAll:()=>modes,activeElement:null,createElement:()=>element('new')},
  sdrTownConfig:{enabled:true,allowTune:true,allowMode:true,allowP25Control:true,allowRfGain:true},
  sdrTownConnected:true,sdrControlSession:{canControl:true,extensionsUsed:0,maxExtensions:2},
  sdrCommandInFlight:false, sdrTownLiveState:{}, sdrControlSeededForLease:false,
  sdrControlWasActive:true,sdrplayDraftSeeded:true, sdrControlDeadlineMs:0,
  controlPollBusy:false,sdrDesiredMode:'',sdrControlClientId:'test',sdrControlName:'Test',
  sdrAdmissionTicket:'server-ticket',sessionStorage:{setItem(){},removeItem(){}},
  sdrSessionMutationBusy:false,sdrSessionEpoch:0,
  workflowViewRevision:{inm:0,ac:0,sat:0},
  renderDecodePanels(){},sdrRenderDevicePanels(){},
});
function load(name) {
  const match=script.match(new RegExp('^(?:async )?function '+name+'\\([^]*?^}', 'm'));
  assert(match, 'production function '+name);
  vm.runInContext(match[0],context);
}
for(const name of ['sdrEffectiveMode','sdrHighlightMode','sdrSetControlsEnabled','sdrTownMessage',
  'sdrControlMs','sdrRenderControlSession','sdrAdoptSession','sdrPopulateP25ControlChannels',
  'populateWorkflowDevices','sdrRadioSelection','sdrSeedFields','sdrControlAction','loadSdrTownControl','commandError','postSdrTown']) load(name);
context.sdrTownLiveState={capabilities:{selectListenDevice:true}};
context.sdrSeedFields({activeDeviceKey:'radio-one',devices:[{stableKey:'radio-one',label:'First'}]});
assert.equal(elements.sdrDevice.value,'radio-one');
elements.sdrDevice.value='radio-two';
context.sdrSeedFields({activeDeviceKey:'radio-one',devices:[{stableKey:'radio-one',label:'First'},{stableKey:'radio-two',label:'Second'}]});
assert.equal(elements.sdrDevice.value,'radio-two','poll must retain operator radio choice');
assert.equal(context.sdrRadioSelection().deviceKey,'radio-two');
context.populateWorkflowDevices([{stableKey:'radio-one',label:'First'}]);
assert.equal(elements.sdrDevice.value,'radio-two','unplug must not silently select another radio');
assert.equal(elements.sdrDevice.options.at(-1).textContent,'Unavailable selected radio');
context.sdrTownLiveState=null;
assert.equal(context.sdrRadioSelection().deviceKey,undefined,'older server must not silently ignore keyed tuning');
context.sdrTownLiveState={capabilities:{selectListenDevice:true}};
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
  assert(elements.commandFeedback.classList.contains('error'),'rejection styling must not depend on message wording');
  assert.equal(context.sdrCommandInFlight,false,'command lock released');
  assert.equal(context.commandError(JSON.stringify({error:'RF disabled in GUI dry-run'}),400),'RF disabled in GUI dry-run');
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
  context.L={divIcon:options=>options,marker:(coords,options)=>({coords,options,
    bindTooltip(text){this.text=text;return this;},addTo(){return this;},
    setLatLng(coords){this.coords=coords;return this;},setIcon(options){this.options=options;return this;},
    setTooltipContent(text){this.text=text;return this;}})};
  load('workflowSession');load('sessionView');load('fetchPagedView');load('aircraftMarkerIcon');load('loadInmarsatMap');
  elements.inmSessionId.value='qa-a';
  assert.equal(context.sessionView('inmarsat','inm','map','default'),'api/sdr-town/inmarsat-sessions?sessionId=qa-a&view=map');
  context.fetch=async()=>({json:async()=>{elements.inmSessionId.value='qa-b';return {ok:true,positions:[{aesId:7,latDeg:1,lonDeg:2}]};}});
  await context.loadInmarsatMap();
  assert.equal(context.inmMarkers.size,0,'late response from old receiver must not populate the selected receiver');
  elements.inmSessionId.value='';
  let mapData={ok:true,positions:[{aesId:74565,icaoHex:'ABC123',latDeg:1,lonDeg:2,voiceActive:true}],aircraft:[]};
  context.fetch=async()=>({json:async()=>mapData});
  await context.loadInmarsatMap();
  const marker=context.inmMarkers.get('74565');assert(marker);
  assert.match(marker.options.icon.html,/#88d6aa/,'active voice colour');
  mapData.positions[0].groundTrackDeg=null;
  await context.loadInmarsatMap();assert.match(marker.options.html,/unknown/,'missing heading is not north');
  mapData.positions[0].groundTrackDeg=0;
  await context.loadInmarsatMap();assert(!marker.options.html.includes('unknown'),'valid north heading is retained');
  mapData.positions[0].latDeg=2;
  await context.loadInmarsatMap();
  assert.equal(context.inmMarkers.get('74565'),marker,'marker must not flash on refresh');
  assert.equal(marker.coords[0],2);
  mapData.positions=[{icaoHex:'BAD',latDeg:null,lonDeg:3},{icaoHex:'BAD2',latDeg:91,lonDeg:0}];
  await context.loadInmarsatMap();assert.equal(context.inmMarkers.size,0);
  mapData={ok:false,error:'Disconnected'};
  await context.loadInmarsatMap();assert.match(elements.inmMapStatus.textContent,/unavailable/);
  // Real production paging: max supported Inmarsat population, atomic updates,
  // immutable identity and bounded progress even with a hostile continuation.
  const urls=[];
  context.fetch=async url=>{
    urls.push(url);const offset=Number(new URL(url,'http://localhost/').searchParams.get('offset'));
    return {ok:true,json:async()=>({ok:true,page:{snapshot:'stable-1',offset,total:512,nextOffset:offset+32<512?offset+32:null},
      aircraft:Array.from({length:32},(_,i)=>({aesId:offset+i+1,label:'\u822a'.repeat(200)})),
      positions:offset<256?Array.from({length:32},(_,i)=>({aesId:offset+i+1,latDeg:1,lonDeg:2})):[]})};
  };
  const full=await context.fetchPagedView('api/sdr-town/inmarsat-map-status');
  assert.equal(full.aircraft.length,512);assert.equal(full.positions.length,256);assert.equal(urls.length,16);
  assert(urls[1].includes('snapshot=stable-1'));
  await context.loadInmarsatMap();assert.equal(context.inmMarkers.size,256);
  let pages=0;
  context.fetch=async()=>({ok:true,json:async()=>({ok:true,page:{snapshot:++pages===1?'old':'new',offset:pages===1?0:32,total:64,nextOffset:pages===1?32:null},positions:Array.from({length:32},()=>({aesId:999,latDeg:2,lonDeg:3}))})});
  await context.loadInmarsatMap();assert.equal(context.inmMarkers.size,256,'failed continuation preserves the complete previous snapshot');
  context.fetch=async()=>({ok:true,json:async()=>({ok:true,page:{snapshot:'stuck',offset:0,total:64,nextOffset:0},positions:[]})});
  await assert.rejects(context.fetchPagedView('map'),/continuation/);
  context.fetch=async()=>({ok:true,json:async()=>({ok:true,page:{snapshot:'short',offset:0,total:64,nextOffset:null},positions:[]})});
  await assert.rejects(context.fetchPagedView('map'),/Incomplete/);
  let continued=0;
  context.fetch=async()=>{continued++;return {ok:true,json:async()=>({ok:true,page:{snapshot:'current',offset:0,total:128,nextOffset:64},positions:Array.from({length:64},()=>({}))})};};
  await assert.rejects(context.fetchPagedView('map',()=>continued===0),/View changed/);
  assert.equal(continued,1,'changed session must stop requesting continuation pages');
  context.fetch=async()=>({ok:true,json:async()=>({ok:true,positions:[]})});
  await context.loadInmarsatMap();assert.equal(context.inmMarkers.size,0,'complete empty snapshot removes old markers');
  load('populateWorkflowSessions');
  elements.inmSessionId.value='selected';
  context.populateWorkflowSessions('inm',[{sessionId:'other',label:'Other'}]);
  assert.equal(elements.inmSessionId.value,'selected','session removal must not select another receiver');
  assert.equal(elements.inmSessionId.options.at(-1).textContent,'Unavailable selected session');
  for(const prefix of ['inm','sat','ac','sstv'])assert(page.includes('<select id="'+prefix+'SessionId"'),'session selector '+prefix);
  let ticketBody;
  context.fetch=async(p,options)=>{ticketBody=JSON.parse(options.body);return {json:async()=>({ok:true,admissionTicket:'issued',session:{canControl:true}})};};
  await context.sdrControlAction('take');assert.equal(context.sdrAdmissionTicket,'issued');
  await context.sdrControlAction('status');assert.equal(ticketBody.admissionTicket,'issued');assert(!('clientId' in ticketBody));
  context.fetch=async()=>({json:async()=>({ok:false,code:'invalid_ticket',session:{canControl:false}})});
  await context.sdrControlAction('status');assert.equal(context.sdrAdmissionTicket,'');
  let resolveOld;
  context.fetch=async()=>new Promise(resolve=>{resolveOld=resolve;});
  const oldStatus=context.sdrControlAction('status');
  context.fetch=async()=>({json:async()=>({ok:true,admissionTicket:'new-ticket',session:{canControl:true}})});
  await context.sdrControlAction('take');
  resolveOld({json:async()=>({ok:false,code:'invalid_ticket',session:{canControl:false}})});
  await oldStatus;assert.equal(context.sdrAdmissionTicket,'new-ticket');assert(context.sdrControlSession.canControl,'late status cannot revoke a newer admission');
  // A completed mutation can have a too-large acknowledgement. Read back the
  // map, retain the refusal, and never repeat the mutation to obtain its data.
  const refreshHandler=script.match(/getElementById\('acRefreshBtn'\)\.addEventListener\('click', (async \(\) => \{[\s\S]*?\n\})\);/);
  assert(refreshHandler,'production aircraft refresh handler');
  const refreshAircraft=vm.runInContext('('+refreshHandler[1]+')',context);
  let mutations=0,readbacks=0;
  load('aircraftCommand');
  context.loadAircraft=async()=>{readbacks++;};
  context.fetch=async()=>{mutations++;return {status:413,text:async()=>JSON.stringify({ok:false,errorCode:'response_too_large',error:'Response too large; read paged view'})};};
  await refreshAircraft();
  assert.equal(mutations,1);assert.equal(readbacks,1);
  assert.equal(elements.commandFeedback.textContent,'Response too large; read paged view');
  for(const ack of [{ok:true,refreshRequired:true},{ok:true,stateOmitted:true,status:200}])for(const session of ['', 'named']){
    mutations=0;readbacks=0;elements.acSessionId.value=session;
    context.fetch=async(p,options)=>{
      if(p.endsWith('control-session'))return {json:async()=>({ok:true,session:{canControl:true}})};
      mutations++;
      assert.equal(p,session?'api/sdr-town/aircraft-sessions':'api/sdr-town/aircraft-map-control');
      assert.equal(JSON.parse(options.body).sessionId,session||undefined);
      return {status:200,text:async()=>JSON.stringify(ack)};
    };
    await refreshAircraft();
    assert.equal(mutations,1);assert.equal(readbacks,1,'compact ack requires one readback');
    assert.equal(elements.commandFeedback.textContent,'Command completed.');
  }
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
  // Stop is a pure stop; a running radio must not be reconfigured first.
  context.sdrControlSession={canControl:true};
  load('postSatcom');const savedPost=context.postSdrTown;
  const sent=[];context.postSdrTown=async(p,b)=>{sent.push({p,b});return {ok:true};};
  context.loadSatcom=async()=>{};elements.satSessionId.value='';
  await context.postSatcom('stop');
  assert.deepEqual(JSON.parse(JSON.stringify(sent[0].b)),{action:'stop'});
  sent.length=0;elements.satSessionId.value='named';
  context.postSdrTown=async(p,b)=>{sent.push({p,b});return undefined;};
  await context.postSatcom('start');
  assert.equal(sent.length,1,'failed configure must not start with old settings');
  assert.equal(sent[0].b.action,'configure');context.postSdrTown=savedPost;
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
