// Run only against a fixture's loopback pair, never an operator's running station.
const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const path=require('node:path');
const [base,output,session='fixture-a',mode='synthetic',fixture]=process.argv.slice(2);
assert(/^http:\/\/127\.0\.0\.1:\d+\/$/.test(base));
(async()=>{
  const browser=await chromium.launch({channel:'msedge',headless:true});
  try{
    const context=await browser.newContext();
    // All native requests still traverse FUBAR and the real DLL. External map
    // tiles, directory entries and CDNs are excluded from this isolated test.
    await context.route('**/*',route=>route.request().url().startsWith(base)?route.continue():route.abort());
    const page=await context.newPage();
    const nativeRequests=new Set(),nativeIdleWaiters=new Set();
    page.on('request',request=>{if(request.url().startsWith(base+'api/sdr-town/'))nativeRequests.add(request);});
    const requestDone=request=>{
      nativeRequests.delete(request);
      if(!nativeRequests.size)for(const resolve of nativeIdleWaiters)resolve();
    };
    page.on('requestfinished',requestDone);page.on('requestfailed',requestDone);
    const drainNativeRequests=()=>new Promise((resolve,reject)=>{
      if(!nativeRequests.size){resolve();return;}
      const done=()=>{clearTimeout(timer);nativeIdleWaiters.delete(done);resolve();};
      const timer=setTimeout(()=>{nativeIdleWaiters.delete(done);reject(new Error('Native requests did not drain'));},10000);
      nativeIdleWaiters.add(done);
    });
    const errors=[];page.on('pageerror',error=>errors.push(error.message));
    await page.goto(base,{waitUntil:'domcontentloaded'});
    await page.waitForFunction(()=>sdrTownConnected);
    await page.locator('#sdrTakeControlBtn').click();
    await page.waitForFunction(()=>sdrControlSession.canControl);
    const ticket=await page.evaluate(()=>sessionStorage.getItem('fubar.sdrTown.ticket'));
    assert.match(ticket,/^[a-f0-9]{32}$/);
    await page.locator('#sdrFreq').fill('98.10000');
    await page.locator('#sdrMode').selectOption('LSB');
    await page.waitForTimeout(2200);
    assert.equal(await page.locator('#sdrFreq').inputValue(),'98.10000');
    assert.equal(await page.locator('#sdrMode').inputValue(),'LSB');
    if(mode!=='paired'){
      await page.locator('#sdrModeBtns [data-set-mode="NFM"]').click();
      await page.waitForFunction(()=>!sdrCommandInFlight);
      assert.equal(await page.locator('#sdrMode').inputValue(),'LSB','immediate mode preserves Tune draft');
    }
    for(const width of [360,390,1280]){
      await page.setViewportSize({width,height:width===1280?900:844});
      for(const task of ['control','listen','fm','tones','sstv','satcom','inmarsat','aircraft']){
        if(width<520)await page.locator('#taskSelect').selectOption(task);
        else await page.locator('#siteTabs [data-tab="'+task+'"]').click();
        await page.waitForTimeout(100);
        assert(await page.locator('#tab-'+task).isVisible(),task+' visible');
        const size=await page.evaluate(()=>({scroll:document.documentElement.scrollWidth,client:document.documentElement.clientWidth}));
        assert(size.scroll<=size.client,`${width} ${task}: overflow ${JSON.stringify(size)}`);
        if(task==='control'||task==='inmarsat')await page.screenshot({path:path.join(output,`${width}-${task}.png`),fullPage:true});
      }
      assert.equal(await page.locator('#sdrFreq').inputValue(),'98.10000','tab changes preserve frequency draft');
      assert.equal(await page.locator('#sdrMode').inputValue(),'LSB','tab changes preserve mode draft');
    }
    await page.locator('#siteTabs [data-tab="inmarsat"]').click();
    await page.waitForFunction(id=>Array.from(document.getElementById('inmSessionId').options).some(o=>o.value===id),session);
    await page.locator('#inmSessionId').selectOption(session);
    await page.waitForTimeout(2200);
    assert.equal(await page.locator('#inmSessionId').inputValue(),session);
    if(fixture){
      assert(/^http:\/\/127\.0\.0\.1:\d+\/$/.test(fixture));
      const scenario=async name=>{
        const response=await fetch(fixture+'fixture-scenario',{
          method:name?'POST':'GET',headers:{Authorization:'Bearer '+process.env.FUBAR_SDR_TOWN_TOKEN,'Content-Type':'application/json'},
          body:name?JSON.stringify({name}):undefined});
        assert(response.ok);return response.json();
      };
      // Keep periodic map polls inactive while deliberately reversing response
      // completion order. The held response still comes from the real DLL.
      await page.locator('#siteTabs [data-tab="control"]').click();
      for(const [loader,statusId,workflow] of [
        ['loadAircraft','acStatus','aircraft'],['loadInmarsatMap','inmMapStatus','inmarsat']
      ])for(const olderSucceeds of [true,false]){
        await scenario(olderSucceeds?'ui':'browser-invalid-ok');
        // Complete a newer pipeline, then drain all superseded network reads.
        // With map tabs inactive, only the explicit fixture call can hit the gate.
        await page.evaluate(async()=>{await Promise.all([loadInmarsat(),loadAircraft()]);});
        await drainNativeRequests();
        let release,held;
        const gate=new Promise(resolve=>{release=resolve;});
        const ready=new Promise(resolve=>{held=resolve;});
        const match=url=>url.pathname.endsWith('/'+workflow+'-map-status')||
          (url.pathname.endsWith('/'+workflow+'-sessions')&&url.searchParams.get('view')==='map');
        let first=true;
        const handler=async route=>{
          if(!first){await route.continue();return;}
          first=false;
          const response=await route.fetch({timeout:10000});
          held();await gate;await route.fulfill({response});
        };
        await page.route(match,handler);
        try{
          await page.evaluate(name=>{
            globalThis.fixtureOlderSettled=false;
            globalThis.fixtureOlderMap=globalThis[name]().finally(()=>{globalThis.fixtureOlderSettled=true;});
          },loader);
          await ready;
          await scenario(olderSucceeds?'browser-invalid-ok':'ui');
          await page.evaluate(name=>globalThis[name](),loader);
          const latest=await page.locator('#'+statusId).textContent();
          assert.equal(latest.startsWith('Tracks '),!olderSucceeds,loader+' newest response');
          assert.equal(await page.evaluate(()=>globalThis.fixtureOlderSettled),false,'gate must hold the explicit older call');
          release();await page.evaluate(()=>globalThis.fixtureOlderMap);
          assert.equal(await page.locator('#'+statusId).textContent(),latest,loader+' stale response must not overwrite latest');
        }finally{release();await page.unroute(match,handler);}
      }
      console.log('PASS: delayed real-DLL responses, both maps, older success/newer failure and older failure/newer success');
      await scenario('browser-pages');
      await page.evaluate(()=>loadAircraft());
      assert.match(await page.locator('#acStatus').innerText(),/^Tracks 512/);
      for(const failure of ['invalid-ok','truncated','oversize','legacy-expired']){
        await scenario('browser-'+failure);
        await page.evaluate(()=>loadAircraft());
        const status=await page.locator('#acStatus').innerText();
        assert(!status.startsWith('Tracks '),failure+' must not display success');
        assert(status.length<512&&!status.includes('PRIVATE_RADIO_CONTENT'),failure+' bounded private error');
      }
      await page.locator('#siteTabs [data-tab="aircraft"]').click();
      for(const failure of ['browser-post-too-large','browser-post-oversize']){
        const before=await scenario(failure);
        await page.locator('#acRefreshBtn').click();
        await page.waitForFunction(()=>!sdrCommandInFlight&&document.getElementById('commandFeedback').classList.contains('error')&&document.getElementById('acStatus').textContent.startsWith('Tracks 512'));
        const after=await scenario();
        assert.equal(after.posted-before.posted,1,'no ambiguous mutation retry');
        const error=await page.locator('#commandFeedback').innerText();
        assert(error.length<512&&!error.includes('PRIVATE_RADIO_CONTENT'));
      }
      await page.screenshot({path:path.join(output,'bounded-map-error.png'),fullPage:true});
      for(const ack of ['browser-post-ack','browser-post-omitted'])for(const selected of ['',session]){
        await scenario('ui');
        await page.evaluate(()=>refreshWorkflowSessions());
        await page.locator('#acSessionId').selectOption(selected);
        const before=await scenario(ack);
        await page.locator('#acRefreshBtn').click();
        await page.waitForFunction(()=>!sdrCommandInFlight&&document.getElementById('commandFeedback').textContent==='Command completed.'&&document.getElementById('acStatus').textContent.startsWith('Tracks 512'));
        const after=await scenario();
        assert.equal(after.posted-before.posted,1,'compact ack must not repeat mutation');
      }
      await scenario('ui');
      console.log('PASS: production browser 512-row paging, malformed upstream, expired snapshot, oversized POST/413, compact primary/named ack, metadata-only catalogue, single mutation and paged readback');
    }
    await page.reload({waitUntil:'domcontentloaded'});
    await page.waitForFunction(()=>sdrControlSession.canControl);
    assert.equal(await page.evaluate(()=>sessionStorage.getItem('fubar.sdrTown.ticket')),ticket,'reload retains ticket');
    await page.locator('#sdrReleaseControlBtn').click();
    await page.waitForFunction(()=>!sdrControlSession.canControl);
    assert(await page.locator('#sdrTuneBtn').isDisabled());
    assert.deepEqual(errors,[]);
    console.log('PASS: production page/loopback, ticket reload, drafts, session lists, all 8 tasks at 360/390/1280, no page errors/overflow');
  }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
