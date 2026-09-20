#include "web_server.h"
#include "live_mp3.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {

constexpr const char* kPage = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>FUBAR Captures</title>
<meta http-equiv="Cache-Control" content="no-store, no-cache, must-revalidate">
<meta http-equiv="Pragma" content="no-cache">
<style>
:root { --ink:#e8f6c8; --muted:#8ea06a; --bg:#070807; --card:#111411; --line:#223018; --green:#b6ff2a; --blue:#3df0ff; --live:#ff3b3b; }
*{ box-sizing:border-box; }
html,body{ margin:0; min-height:100%; background:
  radial-gradient(900px 400px at 10% -10%, rgba(182,255,42,.08), transparent 50%),
  radial-gradient(700px 360px at 110% 0%, rgba(61,240,255,.08), transparent 46%),
  var(--bg); color:var(--ink); font-family:"Segoe UI",sans-serif; }
header{ max-width:920px; margin:0 auto; padding:28px 18px 8px; }
.kicker{ letter-spacing:.28em; font-size:11px; color:var(--green); text-transform:uppercase; }
h1{ margin:.2rem 0; font-size:clamp(2.2rem,7vw,4.4rem); letter-spacing:.04em; }
.sub{ color:var(--muted); margin:0 0 14px; }
.pill{ display:inline-flex; align-items:center; gap:8px; border:1px solid var(--line); background:#0c100c; border-radius:999px; padding:7px 12px; font-size:13px; }
.livebox{ display:flex; gap:12px; align-items:center; flex-wrap:wrap; margin:16px 0 8px; padding:14px; background:var(--card); border:1px solid var(--line); border-radius:16px; }
.livebox #liveBtn{ border:0; border-radius:999px; padding:10px 18px; font-weight:800; cursor:pointer; background:var(--live); color:#fff; }
.livebox #liveBtn.on{ background:var(--green); color:#111; }
.nowboard{ margin:16px 0 8px; padding:18px 20px; background:linear-gradient(180deg,#171e12,#10150f); border:1px solid var(--green); border-radius:18px; box-shadow:0 0 28px rgba(182,255,42,.14); }
.nowboard .kicker{ margin:0 0 8px; }
.nowboard .title{ margin:0; font-size:clamp(1.45rem,5vw,2.55rem); font-weight:800; letter-spacing:.03em; line-height:1.15; color:#f6ffd8; word-break:break-word; }
.nowboard .p25status{ margin:10px 0 0; font-size:clamp(0.92rem,2.6vw,1.12rem); font-weight:650; letter-spacing:.02em; color:var(--green); line-height:1.3; word-break:break-word; }
.nowboard .p25status[hidden]{ display:none !important; }
.nowboard.empty{ border-color:var(--line); box-shadow:none; background:var(--card); }
.nowboard.empty .title{ color:var(--muted); font-size:1.05rem; font-weight:600; letter-spacing:.02em; }
.nowboard.empty .p25status{ color:#9bb87a; }
.sdrtown{ display:none; margin:16px 0 8px; padding:14px; background:#0c100c; border:1px solid var(--line); border-radius:16px; }
.sdrtown.on{ display:block; }
.sdrgrid{ display:grid; grid-template-columns:repeat(4,minmax(0,1fr)); gap:8px; align-items:end; }
.sdrgrid label{ display:block; color:var(--muted); font-size:12px; margin-bottom:3px; }
.sdrgrid input,.sdrgrid select{ width:100%; border:1px solid var(--line); border-radius:10px; background:#070807; color:var(--ink); padding:9px 10px; }
.sdrgrid input[type=checkbox]{ width:auto; margin-right:6px; vertical-align:middle; }
.sdrgrid button{ border:0; border-radius:999px; padding:10px 14px; font-weight:800; cursor:pointer; background:var(--green); color:#111; }
.sdrgrid input:disabled,.sdrgrid select:disabled{ opacity:.62; cursor:not-allowed; }
.sdrgrid button:disabled,.sdrlease button:disabled{ opacity:.45; cursor:not-allowed; }
.sdrrow{ grid-column:1 / -1; display:grid; grid-template-columns:1.25fr 1fr auto; gap:8px; align-items:end; }
.sdrlease{ display:flex; align-items:center; gap:8px; flex-wrap:wrap; margin-top:10px; }
.sdrlease button{ border:1px solid var(--line); border-radius:999px; padding:8px 12px; font-weight:800; cursor:pointer; background:#172214; color:var(--ink); }
.sdrlease button.primary{ border-color:var(--green); background:var(--green); color:#111; }
.sdrlease button.warn{ border-color:#ffd166; color:#ffd166; }
.sdrstate{ color:var(--muted); font-size:13px; }
.sdrstate.warn{ color:#ffd166; }
.sdrmeta{ color:var(--muted); font-size:13px; margin-top:8px; min-height:18px; }
.sdrplaybox{ display:none; margin:12px 0 4px; padding:12px; background:#0a120a; border:1px solid #2a4a22; border-radius:14px; }
.sdrplaybox.on{ display:block; }
.sdrplaybox .lab{ color:var(--muted); font-size:12px; margin:0 0 8px; }
.sdrplaybox .lab b{ color:var(--ink); }
.sdrplaygrid{ display:grid; grid-template-columns:repeat(4,minmax(0,1fr)); gap:8px; align-items:end; }
.sdrplaygrid label{ display:block; color:var(--muted); font-size:12px; margin-bottom:3px; }
.sdrplaygrid input,.sdrplaygrid select{ width:100%; border:1px solid var(--line); border-radius:10px; background:#070807; color:var(--ink); padding:9px 10px; }
.sdrplaygrid input[type=checkbox]{ width:auto; margin-right:6px; vertical-align:middle; }
.sdrplaygrid button{ border:0; border-radius:999px; padding:10px 14px; font-weight:800; cursor:pointer; background:var(--green); color:#111; }
.sdrplaygrid input:disabled,.sdrplaygrid select:disabled,.sdrplaygrid button:disabled{ opacity:.45; cursor:not-allowed; }
.sdrplayhint{ color:var(--muted); font-size:12px; margin:8px 0 0; line-height:1.4; }
@media (max-width:820px){ .sdrplaygrid{ grid-template-columns:1fr 1fr; } }
@media (max-width:560px){ .sdrplaygrid{ grid-template-columns:1fr; } }
.modemenu{ margin:12px 0 4px; padding:12px; background:#0c100c; border:1px solid var(--line); border-radius:14px; }
.modemenu .lab{ color:var(--muted); font-size:12px; margin:0 0 8px; }
.modemenu .lab b{ color:var(--ink); }
.modebtns{ display:flex; flex-wrap:wrap; gap:6px; }
.modebtns button{ border:1px solid var(--line); background:#172214; color:var(--ink); border-radius:999px; padding:8px 12px; font-weight:800; cursor:pointer; }
.modebtns button.on{ background:var(--green); color:#111; border-color:var(--green); }
.modebtns button:disabled{ opacity:.45; cursor:not-allowed; }
.modebtns button.digital{ border-color:var(--blue); color:var(--blue); }
.modebtns button.digital.on{ background:var(--blue); color:#041018; border-color:var(--blue); }
.modehint{ color:var(--muted); font-size:12px; margin:8px 0 0; line-height:1.4; }
.quickmode{ margin:12px 0 0; display:flex; flex-wrap:wrap; gap:8px; align-items:center; }
.quickmode button{ border:1px solid var(--line); background:#172214; color:var(--ink); border-radius:999px; padding:8px 14px; font-weight:800; cursor:pointer; }
.quickmode button:disabled{ opacity:.45; cursor:not-allowed; }
.quickmode .hint{ color:var(--muted); font-size:12px; }
.tabs{ display:flex; flex-wrap:wrap; gap:8px; margin:16px 0 10px; }
.tabs button{ border:1px solid var(--line); background:#0c100c; color:var(--ink); border-radius:999px; padding:9px 14px; font-weight:700; cursor:pointer; }
.tabs button.on{ background:var(--green); color:#111; border-color:var(--green); }
.tabpanel{ display:none; margin:0 0 12px; }
.tabpanel.on{ display:block; }
.infocard{ padding:16px 18px; background:var(--card); border:1px solid var(--line); border-radius:16px; }
.infocard h3{ margin:0 0 8px; font-size:1.15rem; }
.infocard .howto{ color:var(--muted); font-size:14px; line-height:1.45; margin:0 0 14px; }
.infocard .howto ol{ margin:8px 0 0; padding-left:1.2rem; }
.infocard .big{ font-size:clamp(1.2rem,3.5vw,1.8rem); font-weight:800; color:#f6ffd8; margin:0 0 8px; word-break:break-word; }
.infocard .meta{ color:var(--muted); font-size:13px; line-height:1.5; }
.infocard .meta b{ color:var(--ink); font-weight:700; }
.heardlist{ margin-top:12px; padding:10px 12px; background:#070807; border:1px solid var(--line); border-radius:12px; max-height:180px; overflow:auto; font-family:ui-monospace,Consolas,monospace; font-size:13px; line-height:1.45; color:#d7ecc0; white-space:pre-wrap; }
.heardlist:empty::before{ content:'Heard list appears when SDR Town Repeater Control Monitor is enabled.'; color:var(--muted); font-family:inherit; }
.sstvgrid{ display:grid; grid-template-columns:repeat(auto-fill,minmax(160px,1fr)); gap:10px; margin-top:12px; }
.sstvgrid a{ display:block; border:1px solid var(--line); border-radius:12px; overflow:hidden; background:#0c100c; color:inherit; text-decoration:none; }
.sstvgrid img{ display:block; width:100%; aspect-ratio:4/3; object-fit:contain; background:#000; }
.sstvgrid .cap{ padding:8px 10px; font-size:12px; color:var(--muted); white-space:pre-wrap; }
.satcomneon{ margin:0; padding:14px; background:#000; border:1px solid #39FF14; border-radius:16px; color:#39FF14; font-family:ui-monospace,Consolas,monospace; }
.satcomneon .kicker{ color:#39FF14; letter-spacing:.28em; }
.satgrid{ display:grid; grid-template-columns:repeat(4,minmax(0,1fr)); gap:8px; margin:10px 0; }
.satbox{ border:1px solid #39FF14; border-radius:10px; padding:8px; background:#050805; }
.satbox label{ display:block; font-size:11px; margin-bottom:4px; color:#39FF14; }
.satbox input,.satbox select{ width:100%; background:#0a0f0a; color:#39FF14; border:1px solid #1f3d1f; border-radius:6px; padding:8px; font-weight:800; }
#satSpectrum,#satWaterfall{ width:100%; max-width:100%; border:1px solid #1f3d1f; border-radius:8px; background:#000; margin:6px 0; display:block; }
.satbtns{ display:flex; flex-wrap:wrap; gap:8px; align-items:center; margin:10px 0; }
.satbtns button{ border:2px solid #39FF14; background:#0c160c; color:#39FF14; border-radius:10px; padding:10px 14px; font-weight:800; cursor:pointer; }
.satbtns button.primary{ background:#39FF14; color:#000; }
.satbtns button:disabled{ opacity:.4; cursor:not-allowed; }
.satstatus{ font-size:13px; margin:6px 0; min-height:18px; }
.satlog{ margin:0; max-height:160px; overflow:auto; background:#050805; border:1px solid #1f3d1f; border-radius:8px; padding:8px; color:#9dff9d; font-size:12px; white-space:pre-wrap; }
@media (max-width:820px){ .satgrid{ grid-template-columns:1fr 1fr; } }
@media (max-width:560px){ .satgrid{ grid-template-columns:1fr; } }
#acMap{ z-index:0; }
.mix{ display:flex; gap:6px; }
.mix button{ border:1px solid var(--line); background:#0c100c; color:var(--ink); border-radius:999px; padding:7px 12px; font-weight:700; cursor:pointer; }
.mix button.on{ background:var(--green); color:#111; border-color:var(--green); }
#liveMedia.radio{ position:static; width:min(100%,420px); height:40px; opacity:1; pointer-events:auto; flex:1 1 240px; }
.level{ width:160px; height:8px; background:#1a2218; border-radius:99px; overflow:hidden; }
.level > span{ display:block; height:100%; width:0; background:var(--green); }
.dot{ width:9px; height:9px; border-radius:50%; background:var(--muted); }
.dot.live{ background:var(--live); box-shadow:0 0 12px var(--live); }
.dot.on{ background:var(--green); box-shadow:0 0 12px var(--green); }
main{ max-width:920px; margin:0 auto; padding:8px 18px 120px; }
.row{ display:flex; justify-content:space-between; gap:12px; align-items:end; margin:18px 0 10px; }
.row h2{ margin:0; font-size:1.05rem; color:var(--green); letter-spacing:.08em; text-transform:uppercase; }
.meta{ color:var(--muted); font-size:13px; }
.card{ display:grid; grid-template-columns:auto 1fr auto; gap:12px; align-items:center; background:var(--card); border:1px solid var(--line); border-radius:16px; padding:12px 14px; margin:0 0 10px; }
button.play{ width:46px; height:46px; border:0; border-radius:50%; background:var(--green); color:#111; font-weight:800; cursor:pointer; }
button.play.playing{ background:var(--blue); }
.name{ font-weight:650; }
.when,.stats{ color:var(--muted); font-size:13px; margin-top:2px; }
.player{ position:fixed; left:0; right:0; bottom:0; background:rgba(8,10,8,.94); border-top:1px solid var(--line); padding:12px 18px 16px; }
.player audio{ width:100%; }
.empty{ padding:28px 8px; color:var(--muted); }
.station{ display:grid; grid-template-columns:1fr auto; gap:8px 12px; align-items:center; background:var(--card); border:1px solid var(--line); border-radius:16px; padding:12px 14px; margin:0 0 10px; color:inherit; text-decoration:none; }
.station:hover{ border-color:var(--green); }
.visit{ border:0; border-radius:999px; padding:8px 14px; font-weight:700; background:#1a2618; color:var(--green); }
@media (max-width:700px){ .card{ grid-template-columns:auto 1fr; } .stats{ grid-column:1 / -1; } }
@media (max-width:820px){ .sdrgrid,.sdrrow{ grid-template-columns:1fr 1fr; } .sdrrow{ grid-column:1 / -1; } }
@media (max-width:560px){ .sdrgrid,.sdrrow{ grid-template-columns:1fr; } }
</style>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
</head>
<body>
<header>
  <p class="kicker">Communication is key</p>
  <h1>FUBAR</h1>
  <p class="sub">Listen live to what FUBAR hears, or play back saved captures.</p>
  <div class="pill"><span id="dot" class="dot"></span><span id="live">Connecting…</span></div>
  <section class="nowboard empty" id="nowBoard" aria-live="polite">
    <p class="kicker">Now playing</p>
    <p class="title" id="nowPlayingText">Waiting for the operator</p>
    <p class="p25status" id="p25StatusText" hidden></p>
  </section>
  <nav class="tabs" id="siteTabs" aria-label="Station sections">
    <button type="button" class="on" data-tab="listen">Listen</button>
    <button type="button" data-tab="fm">FM station</button>
    <button type="button" data-tab="tones">UHF tones</button>
    <button type="button" data-tab="sstv">SSTV pictures</button>
    <button type="button" data-tab="satcom">Satcom</button>
    <button type="button" data-tab="inmarsat">Inmarsat</button>
    <button type="button" data-tab="aircraft">Aircraft</button>
    <button type="button" data-tab="control">Radio control</button>
  </nav>
  <section class="tabpanel on" id="tab-listen">
  <div class="livebox">
    <button id="liveBtn" type="button">Listen live</button>
    <div>
      <div class="name">Live monitor</div>
      <div class="when" id="liveHint">Same audio the app is capturing right now</div>
    </div>
    <div class="mix" id="liveMix" title="Mono is usually cleaner for WFM on speakers. Stereo is 1:1 left/right.">
      <button type="button" data-mix="mono" class="on">Mono</button>
      <button type="button" data-mix="stereo">Stereo</button>
      <button type="button" id="radioModeBtn" title="Force the Samsung-style radio player (needed for Chrome notifications)">Phone radio</button>
    </div>
    <div class="level" title="Live level"><span id="liveLevel"></span></div>
  </div>
  </section>
  <section class="tabpanel" id="tab-fm">
    <div class="infocard">
      <h3>FM station (RDS)</h3>
      <div class="howto">
        Easy steps:
        <ol>
          <li>Open <b>Radio control</b>, click <b>Take control</b>.</li>
          <li>Tap <b>Switch to WFM</b> below (or pick WFM on Radio control).</li>
          <li>Tune an FM broadcast station (around 88–108 MHz).</li>
          <li>Wait a few seconds — the station name and text appear here automatically.</li>
        </ol>
        Mode changes only run while you hold control. FUBAR does not touch the SDR Town DSP thread beyond the normal control API.
      </div>
      <p class="big" id="rdsSummary">Waiting for FM station data</p>
      <div class="meta" id="rdsMeta">Enable SDR Town control in FUBAR settings so this page can read live RDS.</div>
      <div class="quickmode">
        <button type="button" data-set-mode="WFM" class="sdrModeAction">Switch to WFM</button>
        <span class="hint">Wide FM for broadcast radio / RDS</span>
      </div>
    </div>
  </section>
  <section class="tabpanel" id="tab-tones">
    <div class="infocard">
      <h3>UHF / VHF tones (CTCSS, DCS &amp; DTMF)</h3>
      <div class="howto">
        Easy steps:
        <ol>
          <li>Open <b>Radio control</b>, click <b>Take control</b>.</li>
          <li>Tap <b>Switch to NFM</b> below.</li>
          <li>Tune the channel you want to identify.</li>
          <li>Live CTCSS/DCS show below. For DTMF / a heard list, enable <b>Repeater Control Monitor</b> in SDR Town (receive-only).</li>
        </ol>
      </div>
      <p class="big" id="tonesSummary">Waiting for CTCSS / DCS</p>
      <div class="meta" id="tonesMeta">Enable SDR Town control in FUBAR settings so this page can read live tones.</div>
      <div class="heardlist" id="tonesHeard"></div>
      <div class="quickmode">
        <button type="button" data-set-mode="NFM" class="sdrModeAction">Switch to NFM</button>
        <span class="hint">Narrow FM for UHF/VHF voice channels</span>
      </div>
    </div>
  </section>
  <section class="tabpanel" id="tab-sstv">
    <div class="infocard">
      <h3>SSTV pictures</h3>
      <div class="howto">
        Easy steps:
        <ol>
          <li>Take control, then tap <b>Switch to NFM</b> and tune the SSTV frequency.</li>
          <li>In SDR Town open <b>Tools → SSTV Images</b>.</li>
          <li>Choose <b>Live NFM - main receiver</b>, pick a <b>new empty output folder</b>, then <b>Receive</b>.</li>
          <li>When the picture looks done, click <b>Finish and save</b>. Images show up here.</li>
        </ol>
        FUBAR never starts or stops the SSTV decoder — it only shows saved pictures and status.
      </div>
      <p class="big" id="sstvSummary">Open Tools → SSTV Images in SDR Town</p>
      <div class="meta" id="sstvMeta">Enable SDR Town control in FUBAR settings to refresh this panel.</div>
      <div class="quickmode">
        <button type="button" data-set-mode="NFM" class="sdrModeAction">Switch to NFM</button>
        <span class="hint">SSTV listens to the main NFM audio</span>
      </div>
      <div class="sstvgrid" id="sstvImages"></div>
    </div>
  </section>
  <section class="tabpanel" id="tab-satcom">
    <div class="satcomneon" id="satcomPanel">
      <p class="kicker">SATCOM SCANNER</p>
      <div class="satgrid">
        <div class="satbox"><label>LOW FREQUENCY</label><input id="satLow" inputmode="decimal" value="420.000"></div>
        <div class="satbox"><label>HIGH FREQUENCY</label><input id="satHigh" inputmode="decimal" value="430.000"></div>
        <div class="satbox"><label>BANDWIDTH kHz</label><input id="satBw" inputmode="decimal" value="250"></div>
        <div class="satbox"><label>MODE</label>
          <select id="satMode"><option>NFM</option><option>WFM</option><option>AM</option><option>USB</option><option>APT</option><option>APRS</option></select>
        </div>
      </div>
      <div class="satgrid">
        <div class="satbox"><label>HOME LAT (+N/-S)</label><input id="satLat" value="-33.87"></div>
        <div class="satbox"><label>HOME LON (+E/-W)</label><input id="satLon" value="151.21"></div>
        <div class="satbox"><label>ALT m</label><input id="satAlt" inputmode="decimal" value="50"></div>
        <div class="satbox"><label>MIN EL °</label><input id="satMinEl" inputmode="decimal" value="10"></div>
      </div>
      <div class="satbtns">
        <button type="button" id="satApplyObsBtn">Apply location</button>
        <button type="button" id="satRefreshTleBtn">Refresh TLE</button>
        <button type="button" id="satArmSstvBtn">Arm ISS SSTV</button>
        <button type="button" id="satDisarmBtn">Disarm</button>
        <label><input type="checkbox" id="satAutoTrack" checked> Auto-track Doppler</label>
        <span id="satTleAge" style="opacity:.8">TLE: —</span>
      </div>
      <div class="satbox" style="margin:8px 0">
        <label>SELECTED SATS</label>
        <div id="satCatChecks" style="display:flex;flex-wrap:wrap;gap:8px;margin-top:6px"></div>
        <button type="button" id="satSaveCatBtn" style="margin-top:8px">Save satellite selection</button>
      </div>
      <pre class="satlog" id="satPassList" style="max-height:140px">Passes appear after location + TLE refresh.</pre>
      <div class="satstatus" id="satPassArmed">No pass armed</div>
      <canvas id="satSpectrum" width="900" height="140"></canvas>
      <canvas id="satWaterfall" width="900" height="160"></canvas>
      <div class="satbtns">
        <button type="button" id="satStartBtn" class="primary">START SCAN</button>
        <button type="button" id="satSkipBtn">FREQUENCY SKIP</button>
        <button type="button" id="satRecordBtn">RECORD</button>
        <button type="button" id="satStopBtn">STOP</button>
        <label>SQUELCH <input id="satSquelch" inputmode="decimal" value="-90" style="width:72px"></label>
      </div>
      <div class="satstatus" id="satStatusLine">Satcom idle — enable SDR Town control and Take control to operate.</div>
      <pre class="satlog" id="satDecodeLog"></pre>
    </div>
  </section>
  <section class="tabpanel" id="tab-inmarsat">
    <div class="satcomneon" id="inmarsatPanel">
      <p class="kicker">INMARSAT AERO / EGC</p>
      <p class="satstatus">Experimental prototype via SDR Town 0.2.71+ (band plans + ACARS/ADS-C parse). No unique-word/FEC/Aero AMBE claim. ADS-C positions appear on the Aircraft map (orange).</p>
      <div class="satgrid">
        <div class="satbox"><label>BAND PLAN</label><select id="inmPlan"></select></div>
        <div class="satbox"><label>CHANNEL</label><select id="inmChannel"></select></div>
        <div class="satbox"><label><input type="checkbox" id="inmVoiceFollow" checked> Voice follow</label></div>
        <div class="satbox"><label><input type="checkbox" id="inmRecord"> Record voice</label></div>
      </div>
      <div class="satbtns">
        <button type="button" id="inmStartBtn" class="primary">START</button>
        <button type="button" id="inmStopBtn">STOP</button>
        <button type="button" id="inmRefreshPlansBtn">Reload plans</button>
      </div>
      <div class="satstatus" id="inmStatus">Inmarsat idle — enable SDR Town control.</div>
      <pre class="satlog" id="inmMsgLog" style="max-height:280px"></pre>
    </div>
  </section>
  <section class="tabpanel" id="tab-aircraft">
    <div class="satcomneon" id="aircraftPanel">
      <p class="kicker">AIRCRAFT MAP</p>
      <p class="satstatus" id="acStatus">ADS-B / OpenSky / Inmarsat ADS-C tracks from SDR Town. Green=local 1090, blue=OpenSky, orange=ADS-C.</p>
      <div id="acMap" style="height:420px;width:100%;border:1px solid #1f3d1f;border-radius:8px;background:#0a1014"></div>
      <div class="satbtns" style="margin-top:8px">
        <button type="button" id="acRefreshBtn">Refresh network</button>
        <button type="button" id="acTuneBtn">Tune 1090 (needs control)</button>
      </div>
      <div class="satbox" id="acPopout" style="display:none;margin-top:8px">
        <div id="acPopText"></div>
        <img id="acPopImg" alt="" style="max-width:100%;max-height:160px;margin-top:8px;display:none">
      </div>
    </div>
  </section>
  <section class="tabpanel" id="tab-control">
  <section class="sdrtown" id="sdrTownPanel">
    <p class="kicker">SDR Town control</p>
    <div class="sdrlease">
      <button id="sdrTakeControlBtn" type="button" class="primary">Take control</button>
      <button id="sdrExtendControlBtn" type="button">Extend</button>
      <button id="sdrReleaseControlBtn" type="button">Release</button>
      <span class="sdrstate" id="sdrControlState">No one has control yet.</span>
    </div>
    <div class="modemenu">
      <p class="lab"><b>Demod / mode</b> — take control, then tap a mode. Keeps the current frequency.</p>
      <div class="modebtns" id="sdrModeBtns">
        <button type="button" data-set-mode="AUTO" class="sdrModeAction" title="Automatic mode pick">AUTO</button>
        <button type="button" data-set-mode="NFM" class="sdrModeAction" title="Narrow FM — UHF/VHF, tones, SSTV">NFM</button>
        <button type="button" data-set-mode="WFM" class="sdrModeAction" title="Wide FM — broadcast radio / RDS">WFM</button>
        <button type="button" data-set-mode="AM" class="sdrModeAction">AM</button>
        <button type="button" data-set-mode="USB" class="sdrModeAction">USB</button>
        <button type="button" data-set-mode="LSB" class="sdrModeAction">LSB</button>
        <button type="button" data-set-mode="CW" class="sdrModeAction">CW</button>
        <button type="button" data-set-mode="P25" class="sdrModeAction digital" title="Digital P25 trunking — uses Monitor CC at the frequency below">P25</button>
      </div>
      <p class="modehint" id="sdrModeHint">Analog demods change instantly. P25 starts control-channel monitoring at the frequency in the form below.</p>
    </div>
    <div class="sdrgrid">
      <div><label for="sdrFreq">Frequency MHz</label><input id="sdrFreq" inputmode="decimal" value="420.35000" placeholder="e.g. 7.10000 or 95.70000" title="0.1–6000 MHz. Take control, type the frequency, then Tune."></div>
      <div><label for="sdrMode">Mode (with Tune)</label><select id="sdrMode"><option>AUTO</option><option>NFM</option><option>WFM</option><option>AM</option><option>USB</option><option>LSB</option><option>CW</option><option>P25</option></select></div>
      <div><label for="sdrBw">Bandwidth kHz</label><input id="sdrBw" inputmode="decimal" placeholder="auto"></div>
      <div><label for="sdrLpf"><input id="sdrLpfEnabled" type="checkbox" checked>LPF kHz</label><input id="sdrLpf" inputmode="decimal" placeholder="leave"></div>
      <div><label for="sdrGain" id="sdrGainLabel">RF gain dB</label><input id="sdrGain" inputmode="decimal" placeholder="leave"></div>
      <div><label for="sdrVolume">Volume %</label><input id="sdrVolume" inputmode="decimal" placeholder="85"></div>
      <div id="sdrRtlBox"><label for="sdrDirectSamp">RTL direct sampling (HF)</label>
        <select id="sdrDirectSamp" title="Enable Q-ADC or I-ADC to receive ~500 kHz–28 MHz on RTL-SDR">
          <option value="0">Off (normal tuner)</option>
          <option value="2">Q-ADC (HF ~500 kHz+)</option>
          <option value="1">I-ADC (HF)</option>
        </select>
      </div>
      <button id="sdrTuneBtn" type="button">Tune</button>
      <div class="sdrrow">
        <div><label for="sdrP25Known">Known P25 control channel</label><select id="sdrP25Known"><option value="">Manual / detected list empty</option></select></div>
        <div><label for="sdrP25Freq">P25 CC MHz</label><input id="sdrP25Freq" inputmode="decimal" placeholder="420.35000"></div>
        <button id="sdrP25Btn" type="button">Monitor CC</button>
      </div>
    </div>
    <div class="sdrplaybox" id="sdrplayBox">
      <p class="lab"><b>SDRplay</b> — <span id="sdrplayModel">device</span>. Shown only while an SDRplay receiver is active.</p>
      <div class="sdrplaygrid">
        <div id="sdrplayAntWrap"><label for="sdrplayAntenna">Antenna</label><select id="sdrplayAntenna"></select></div>
        <div id="sdrplayAgcWrap"><label for="sdrplayAgc"><input id="sdrplayAgc" type="checkbox">AGC</label></div>
        <div id="sdrplayIfgrWrap"><label for="sdrplayIfgr">IFGR dB</label><input id="sdrplayIfgr" inputmode="decimal"></div>
        <div id="sdrplayRfgrWrap"><label for="sdrplayRfgr">RFGR dB</label><input id="sdrplayRfgr" inputmode="decimal"></div>
        <div id="sdrplayBwWrap"><label for="sdrplayBw">IF bandwidth</label><select id="sdrplayBw"><option value="0">Driver default</option></select></div>
        <div id="sdrplayBiasWrap"><label for="sdrplayBiasT"><input id="sdrplayBiasT" type="checkbox">Bias-T</label></div>
        <div id="sdrplayRfNotchWrap"><label for="sdrplayRfNotch"><input id="sdrplayRfNotch" type="checkbox">MW/FM notch</label></div>
        <div id="sdrplayDabWrap"><label for="sdrplayDabNotch"><input id="sdrplayDabNotch" type="checkbox">DAB notch</label></div>
        <div id="sdrplayExtRefWrap"><label for="sdrplayExtRef"><input id="sdrplayExtRef" type="checkbox">Clock OUT</label></div>
        <div id="sdrplayHdrWrap"><label for="sdrplayHdr"><input id="sdrplayHdr" type="checkbox">HDR</label></div>
        <div id="sdrplayIqWrap"><label for="sdrplayIqCorr"><input id="sdrplayIqCorr" type="checkbox" checked>IQ correction</label></div>
        <div id="sdrplayDivWrap"><label for="sdrplayDivMode">Diversity</label>
          <select id="sdrplayDivMode">
            <option value="off">Off</option>
            <option value="sum">Equal-gain sum</option>
            <option value="null">Null-steer</option>
          </select>
        </div>
        <div id="sdrplayPhaseWrap"><label for="sdrplayDivPhase">B phase °</label><input id="sdrplayDivPhase" inputmode="decimal" value="0"></div>
        <div id="sdrplayAmpWrap"><label for="sdrplayDivAmp">B amplitude</label><input id="sdrplayDivAmp" inputmode="decimal" value="1"></div>
        <button id="sdrplayApplyBtn" type="button">Apply SDRplay</button>
      </div>
      <p class="sdrplayhint" id="sdrplayHint">Port and feature set follow the active RSP model.</p>
    </div>
    <div class="sdrmeta" id="sdrTownStatus">Checking SDR Town bridge...</div>
  </section>
  </section>
</header>
<main>
  <div class="row"><h2>Public servers</h2><div class="meta" id="netCount"></div></div>
  <div id="stations" class="empty">Looking up stations on gearsqueens.online…</div>
  <div class="row"><h2>Captures</h2><div class="meta" id="count"></div></div>
  <div id="list" class="empty">Loading captures…</div>
</main>
<div class="player">
  <div class="meta" id="now" style="margin-bottom:6px">Nothing playing</div>
  <audio id="audio" controls preload="none"></audio>
  <audio id="liveMedia" playsinline webkit-playsinline style="position:absolute;width:1px;height:1px;opacity:0;pointer-events:none"></audio>
  <video id="liveVideo" playsinline webkit-playsinline style="position:absolute;width:1px;height:1px;opacity:0;pointer-events:none"></video>
</div>
<script>
const audio = document.getElementById('audio');
const list = document.getElementById('list');
const live = document.getElementById('live');
const dot = document.getElementById('dot');
const count = document.getElementById('count');
const now = document.getElementById('now');
let items = [];
let current = '';
let nowPlayingTitle = '';
let p25StatusLine = '';
let sdrTownConfig = {enabled:false};
let sdrTownLiveState = null;
let sdrControlSession = {role:'idle', canControl:false, remainingMs:0};
let sdrControlDeadlineMs = 0;
let sdrControlSeededForLease = false;
let sdrControlWasActive = false;
let sdrCommandInFlight = false;
let sdrDesiredMode = '';
const sdrControlClientId = (() => {
  const key = 'fubar.sdrTown.clientId';
  let id = localStorage.getItem(key);
  if (!id) {
    id = (window.crypto && window.crypto.randomUUID) ? window.crypto.randomUUID() :
      ('client-' + Date.now().toString(36) + '-' + Math.random().toString(36).slice(2));
    localStorage.setItem(key, id);
  }
  return id;
})();
const sdrControlName = (() => {
  const key = 'fubar.sdrTown.clientName';
  let name = localStorage.getItem(key);
  if (!name) {
    name = 'Operator ' + sdrControlClientId.slice(-4).toUpperCase();
    localStorage.setItem(key, name);
  }
  return name;
})();

function fmt(sec){
  sec = Math.max(0, Number(sec)||0);
  const m = Math.floor(sec/60);
  const s = Math.floor(sec%60).toString().padStart(2,'0');
  return m + ':' + s;
}
function render(){
  if (!items.length){
    list.className = 'empty';
    list.textContent = 'No captures yet. When the admin records, clips appear here.';
    count.textContent = '';
    return;
  }
  list.className = '';
  count.textContent = items.length + ' clip' + (items.length===1?'':'s');
  list.innerHTML = items.map(item => `
    <article class="card">
      <button class="play ${current===item.id?'playing':''}" data-id="${item.id}" aria-label="Play ${item.name}">${current===item.id?'❚❚':'▶'}</button>
      <div>
        <div class="name">${item.name}</div>
        <div class="when">${item.started} · ${item.mode}</div>
      </div>
      <div class="stats">${fmt(item.durationSeconds)} · ${(item.bytes/1024).toFixed(0)} KB</div>
    </article>`).join('');
}
function play(id){
  const item = items.find(x => x.id === id);
  if (!item) return;
  if (current === id && !audio.paused){ audio.pause(); return; }
  stopLive();
  current = id;
  now.textContent = 'Playing ' + item.name;
  audio.src = 'audio/' + encodeURIComponent(item.id);
  audio.play();
  render();
}
list.addEventListener('click', e => {
  const button = e.target.closest('[data-id]');
  if (button) play(button.getAttribute('data-id'));
});
audio.addEventListener('pause', render);
audio.addEventListener('play', render);
const liveBtn = document.getElementById('liveBtn');
const liveHint = document.getElementById('liveHint');
const liveLevel = document.getElementById('liveLevel');
const liveMedia = document.getElementById('liveMedia');
const liveVideo = document.getElementById('liveVideo');
let livePlaying = false;
let liveWanted = false;
let liveAbort = null;
let liveAc = null;
let liveNode = null;
let liveStreamDest = null;
let liveWake = null;
let livePeak = 0;
let meterRaf = 0;
let liveMix = 'mono';
let liveRate = 0;
let liveCh = 1;
const AudioCtx = window.AudioContext || window.webkitAudioContext;
const SILENT_WAV = 'data:audio/wav;base64,UklGRigAAABXQVZFZm10IBIAAAABAAEARKwAAIhYAQACABAAAABkYXRhAgAAAAEA';
function detectHost(){
  const ua = navigator.userAgent || '';
  const ch = navigator.userAgentData;
  const platform = (ch && ch.platform) || '';
  const mobileCH = !!(ch && ch.mobile);
  const iOS = /iP(hone|od|ad)/.test(ua) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
  const firefox = /Firefox|FxiOS/i.test(ua);
  const samsung = /SamsungBrowser/i.test(ua);
  const chrome = /Chrome|CriOS|Chromium|EdgA|Edg\//i.test(ua) && !firefox;
  const safari = /Safari/i.test(ua) && !/Chrome|CriOS|Chromium|FxiOS|EdgiOS|OPiOS|Android/i.test(ua);
  const coarse = !!(window.matchMedia && matchMedia('(pointer: coarse)').matches);
  const touch = (navigator.maxTouchPoints || 0) > 0 || 'ontouchstart' in window;
  const android = /Android/i.test(ua) || /Android/i.test(platform) ||
    (!iOS && chrome && (mobileCH || /Mobile/i.test(ua) || (coarse && touch)));
  let name = 'Browser';
  if (iOS && firefox) name = 'iOS Firefox';
  else if (iOS && chrome) name = 'iOS Chrome';
  else if (iOS) name = 'iPhone Safari';
  else if (android && firefox) name = 'Android Firefox';
  else if (android && samsung) name = 'Samsung Internet';
  else if (android) name = 'Android Chrome';
  else if (firefox) name = 'Firefox';
  else if (safari) name = 'Safari';
  else if (chrome) name = 'Chrome';
  return { iOS, android, firefox, samsung, chrome, safari, mobile: iOS || android, name };
}
function pickStrategy(h){
  if (h.iOS) return { hold:'video-stream', unlock:true, avoidSampleRate:true, avoidWorklet:true, silentWav:false, label:'hidden video hold' };
  if (h.android && h.firefox) return { hold:'webaudio', unlock:true, avoidSampleRate:false, avoidWorklet:false, silentWav:false, label:'Web Audio' };
  if (h.android) return { hold:'media-url', unlock:true, avoidSampleRate:true, avoidWorklet:true, silentWav:false, label:'Android radio stream' };
  return { hold:'silent-wav', unlock:false, avoidSampleRate:false, avoidWorklet:false, silentWav:true, label:'Web Audio' };
}
const host = detectHost();
let strategy = pickStrategy(host);
let forceRadio = false;
try { forceRadio = localStorage.getItem('fubarForceRadio') === '1'; } catch {}
function applyStrategy(){
  const next = pickStrategy(host);
  if (forceRadio) {
    next.hold = 'media-url';
    next.unlock = true;
    next.silentWav = false;
    next.avoidSampleRate = true;
    next.avoidWorklet = true;
    next.label = 'forced radio stream';
  }
  strategy = next;
  const btn = document.getElementById('radioModeBtn');
  if (btn) btn.classList.toggle('on', forceRadio || strategy.hold === 'media-url');
}
applyStrategy();
if (navigator.userAgentData && navigator.userAgentData.getHighEntropyValues) {
  navigator.userAgentData.getHighEntropyValues(['platform']).then((d) => {
    if (/Android/i.test(d.platform || '')) {
      host.android = true;
      if (!host.samsung) host.name = 'Android Chrome';
      applyStrategy();
    }
  }).catch(()=>{});
}
function holdEl(){ return strategy.hold === 'video-stream' ? liveVideo : liveMedia; }
function armHold(el){
  try {
    el.muted = strategy.silentWav;
    el.volume = strategy.silentWav ? 0 : 1;
    el.playsInline = true;
    el.setAttribute('playsinline', '');
    el.setAttribute('webkit-playsinline', '');
  } catch {}
}
function resetHold(el){
  try {
    el.pause();
    el.srcObject = null;
    el.removeAttribute('src');
    el.load();
  } catch {}
}
function showRadioPlayer(on){
  liveMedia.classList.toggle('radio', !!on);
  liveMedia.controls = !!on;
  if (!on) liveMedia.removeAttribute('controls');
}
function bindMediaSession(el, ac){
  if (!navigator.mediaSession) return;
  try {
    navigator.mediaSession.metadata = new MediaMetadata({
      title: nowPlayingTitle || 'FUBAR Live',
      artist: p25StatusLine || 'FUBAR',
      album: nowPlayingTitle ? 'Now playing' : 'Listen live',
      artwork: [{src:'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAVUlEQVR4nO3SMQEAIAwDsZ3/p7N5gAQkmTt3S5K0/wMwM8/cPQGYmQEwMwNgZgbAzAyAmRkAMzMAZmYAzMwAmJkBMDMDYGYGwMwMgJkZADMzAGZmAMzMAJiZAfhpC+sDEp6nH5sAAAAASUVORK5CYII=', sizes:'64x64', type:'image/png'}]
    });
    navigator.mediaSession.playbackState = 'playing';
    navigator.mediaSession.setActionHandler('play', () => {
      try { if (ac) ac.resume(); } catch {}
      try { el.play(); } catch {}
    });
    navigator.mediaSession.setActionHandler('pause', () => { try { el.pause(); } catch {} });
    navigator.mediaSession.setActionHandler('stop', () => stopLive());
  } catch {}
}
function startRadioFromGesture(){
  const el = liveMedia;
  showRadioPlayer(true);
  armHold(el);
  el.muted = false;
  el.volume = 1;
  el.loop = false;
  el.preload = 'auto';
  try { el.srcObject = null; } catch {}
  el.src = 'live.mp3?v=125&t=' + Date.now();
  bindMediaSession(el, null);
  document.title = 'FUBAR Live';
  const playAttempt = el.play();
  if (playAttempt && playAttempt.catch) playAttempt.catch(()=>{});
  return el;
}
function unlockLiveAudio(){
  try {
    if (AudioCtx && (!liveAc || liveAc.state === 'closed')) {
      liveAc = new AudioCtx({latencyHint:'playback'});
    }
    if (liveAc && liveAc.resume) liveAc.resume();
  } catch {}
  if (strategy.hold === 'media-url') {
    startRadioFromGesture();
    return;
  }
  const el = holdEl();
  armHold(el);
  try {
    if (!el.srcObject && strategy.unlock) {
      el.src = SILENT_WAV;
      el.loop = true;
      el.play();
    }
  } catch {}
}
try { liveMix = localStorage.getItem('fubarLiveMix') === 'stereo' ? 'stereo' : 'mono'; } catch {}
function applyMixButtons(){
  document.querySelectorAll('#liveMix [data-mix]').forEach(btn => {
    btn.classList.toggle('on', btn.getAttribute('data-mix') === liveMix);
  });
}
function livePlayingText(){
  const fmt = liveRate
    ? (liveRate + ' Hz 16-bit PCM' + (liveCh>1?' stereo':''))
    : (strategy.hold === 'media-url' ? 'MP3 radio (PCM 1:1 is PC-only; Android needs this to stay playing)' : '16-bit PCM');
  return 'Live · ' + fmt +
    ' · 1:1 · ' + (liveMix==='stereo'?'stereo speakers':'mono to both speakers') +
    ' · ' + host.name + ' · ' + strategy.label;
}
function setLiveMix(mode){
  liveMix = mode === 'stereo' ? 'stereo' : 'mono';
  try { localStorage.setItem('fubarLiveMix', liveMix); } catch {}
  applyMixButtons();
  try { if (liveNode && liveNode.port) liveNode.port.postMessage({mix: liveMix}); } catch {}
  if (livePlaying) setLiveUi(true, livePlayingText());
}
document.getElementById('liveMix').addEventListener('click', e => {
  const btn = e.target.closest('[data-mix]');
  if (btn) setLiveMix(btn.getAttribute('data-mix'));
});
applyMixButtons();
document.getElementById('radioModeBtn').addEventListener('click', (e) => {
  e.preventDefault();
  e.stopPropagation();
  forceRadio = !forceRadio;
  try { localStorage.setItem('fubarForceRadio', forceRadio ? '1' : '0'); } catch {}
  applyStrategy();
  if (livePlaying) setLiveUi(true, livePlayingText());
});
function queueLabel(status){
  const limit = Math.max(1, Number(status && status.listenerLimit) || 5);
  const n = Number(status && status.listeners) || 0;
  const q = Number(status && status.queued) || 0;
  if (q > 0) return n + '/' + limit + ' listening · ' + q + ' waiting';
  return n + '/' + limit + ' listening';
}
function startMeter(){
  if (meterRaf) return;
  const tick = () => {
    meterRaf = 0;
    liveLevel.style.width = Math.min(100, Math.round(livePeak * 140)) + '%';
    livePeak *= 0.82;
    if (liveWanted) meterRaf = requestAnimationFrame(tick);
  };
  meterRaf = requestAnimationFrame(tick);
}
function stopMeter(){
  if (meterRaf) cancelAnimationFrame(meterRaf);
  meterRaf = 0;
  livePeak = 0;
  liveLevel.style.width = '0';
}
function setLiveUi(on, text){
  livePlaying = on;
  liveBtn.textContent = on ? 'Stop live' : 'Listen live';
  liveBtn.classList.toggle('on', on);
  liveHint.textContent = text;
  if (!on) stopMeter();
}
function closeLiveAudio(){
  try { if (liveNode) liveNode.disconnect(); } catch {}
  liveNode = null;
  try { if (liveStreamDest) liveStreamDest.disconnect(); } catch {}
  liveStreamDest = null;
  try { if (liveAc && liveAc.state !== 'closed' && liveAc.close) liveAc.close(); } catch {}
  liveAc = null;
}
function stopLive(){
  liveWanted = false;
  livePlaying = false;
  if (liveAbort) { liveAbort.abort(); liveAbort = null; }
  closeLiveAudio();
  showRadioPlayer(false);
  resetHold(liveMedia);
  resetHold(liveVideo);
  try { document.title = 'FUBAR Captures'; } catch {}
  try { if (liveWake) liveWake.release(); } catch {}
  liveWake = null;
  try { if (navigator.mediaSession) navigator.mediaSession.playbackState = 'none'; } catch {}
  setLiveUi(false, 'Same audio the app is capturing right now');
}
async function keepLiveAlive(){
  if (!liveWanted) return;
  try { if (liveAc && liveAc.state !== 'running') await liveAc.resume(); } catch {}
  try {
    const el = holdEl();
    armHold(el);
    if (strategy.silentWav) {
      liveMedia.muted = true;
      liveMedia.volume = 0;
      if (liveMedia.paused && !liveMedia.srcObject) {
        liveMedia.src = SILENT_WAV;
        liveMedia.loop = true;
        await liveMedia.play();
      }
    } else if (el.paused) {
      await el.play();
    }
  } catch {}
  try {
    if (navigator.audioSession) navigator.audioSession.type = 'playback';
  } catch {}
  try {
    if (navigator.wakeLock && document.visibilityState === 'visible') {
      liveWake = await navigator.wakeLock.request('screen');
    }
  } catch {}
}
function makeResampler(inRate, outRate, ch){
  const scale = 1 / 32768;
  if (Math.abs(inRate - outRate) < 0.5) {
    return (pcm) => {
      const f = new Float32Array(pcm.length);
      for (let i = 0; i < pcm.length; i++) f[i] = pcm[i] * scale;
      return f;
    };
  }
  let hold = new Float32Array(0);
  let phase = 0;
  const step = inRate / outRate;
  const downsample = step > 1.01;
  return (pcm) => {
    const merged = new Float32Array(hold.length + pcm.length);
    merged.set(hold);
    for (let i = 0; i < pcm.length; i++) merged[hold.length + i] = pcm[i] * scale;
    const frames = Math.floor(merged.length / ch);
    const out = [];
    const need = downsample ? step : 1;
    while (phase + need < frames) {
      if (downsample) {
        const start = phase;
        const end = Math.min(frames, phase + step);
        const i0 = start | 0;
        const i1 = Math.min(frames - 1, (end - 0.0001) | 0);
        for (let c = 0; c < ch; c++) {
          let s = 0, n = 0;
          for (let i = i0; i <= i1; i++) { s += merged[i * ch + c]; n++; }
          out.push(n ? s / n : 0);
        }
      } else {
        const i0 = phase | 0;
        const frac = phase - i0;
        const i1 = Math.min(frames - 1, i0 + 1);
        for (let c = 0; c < ch; c++) {
          const a = merged[i0 * ch + c];
          const b = merged[i1 * ch + c];
          out.push(a + (b - a) * frac);
        }
      }
      phase += step;
    }
    const consumed = Math.min(frames, phase | 0);
    hold = merged.subarray(consumed * ch).slice();
    phase -= consumed;
    return Float32Array.from(out);
  };
}
function writeSpeakers(outs, i, left, right, mix){
  if (mix !== 'stereo') {
    const m = (left + right) * 0.5;
    for (let c = 0; c < outs.length; c++) outs[c][i] = m;
    return;
  }
  if (outs.length === 1) {
    outs[0][i] = (left + right) * 0.5;
    return;
  }
  outs[0][i] = left;
  outs[1][i] = right;
  for (let c = 2; c < outs.length; c++) outs[c][i] = left;
}
function connectLiveOut(ac, node){
  try { if (liveStreamDest) liveStreamDest.disconnect(); } catch {}
  liveStreamDest = null;
  if ((strategy.hold === 'audio-stream' || strategy.hold === 'video-stream') &&
      ac.createMediaStreamDestination) {
    liveStreamDest = ac.createMediaStreamDestination();
    node.connect(liveStreamDest);
    const el = holdEl();
    armHold(el);
    try {
      el.srcObject = liveStreamDest.stream;
      el.loop = false;
      el.play().catch(() => node.connect(ac.destination));
    } catch {
      node.connect(ac.destination);
    }
    return;
  }
  node.connect(ac.destination);
}
function attachScriptRing(ac, channels){
  const sr = ac.sampleRate;
  const srcCh = Math.max(1, channels);
  const n = Math.max(16384, Math.floor(sr * 8) * srcCh);
  const ring = new Float32Array(n);
  let w = 0, r = 0, primed = false;
  const avail = () => { let a = w - r; if (a < 0) a += n; return a; };
  const primeNeed = Math.floor(sr * 1.0) * srcCh;
  const lowNeed = Math.floor(sr * 0.2) * srcCh;
  let node;
  try { node = ac.createScriptProcessor(4096, 1, 2); }
  catch { node = ac.createScriptProcessor(4096, 0, 2); }
  node.onaudioprocess = (ev) => {
    const outs = [];
    for (let c = 0; c < ev.outputBuffer.numberOfChannels; c++) outs[c] = ev.outputBuffer.getChannelData(c);
    const frames = ev.outputBuffer.length;
    const need = frames * srcCh;
    let a = avail();
    if (!primed) {
      if (a < primeNeed) {
        for (let c = 0; c < outs.length; c++) outs[c].fill(0);
        return;
      }
      primed = true;
    }
    if (a < need || a < lowNeed) {
      primed = false;
      for (let c = 0; c < outs.length; c++) outs[c].fill(0);
      return;
    }
    let peak = 0;
    for (let i = 0; i < frames; i++) {
      const left = ring[r];
      r++; if (r >= n) r = 0;
      let right = left;
      if (srcCh > 1) {
        right = ring[r];
        r++; if (r >= n) r = 0;
        for (let extra = 2; extra < srcCh; extra++) {
          r++; if (r >= n) r = 0;
        }
      }
      const absL = left < 0 ? -left : left;
      const absR = right < 0 ? -right : right;
      if (absL > peak) peak = absL;
      if (absR > peak) peak = absR;
      writeSpeakers(outs, i, left, right, liveMix);
    }
    livePeak = peak;
  };
  connectLiveOut(ac, node);
  return {
    node,
    buffered(){ return avail() / srcCh / sr; },
    push(f32){
      let used = w - r; if (used < 0) used += n;
      const space = n - used - srcCh;
      const count = Math.min(f32.length, Math.max(0, space));
      const aligned = count - (count % srcCh);
      for (let i = 0; i < aligned; i++) {
        ring[w] = f32[i];
        w++; if (w >= n) w = 0;
      }
      return aligned;
    }
  };
}
async function attachWorkletRing(ac, channels){
  const srcCh = Math.max(1, channels);
  const src = `
    registerProcessor('fubar-play', class extends AudioWorkletProcessor {
      constructor() {
        super();
        this.ch = 1;
        this.mix = 'mono';
        this.n = Math.max(16384, sampleRate * 8 * 2);
        this.ring = new Float32Array(this.n);
        this.w = 0; this.r = 0; this.primed = false; this.tick = 0;
        this.port.onmessage = (e) => {
          const d = e.data || {};
          if (d.ch) this.ch = d.ch;
          if (d.mix) this.mix = d.mix;
          const s = d.s;
          if (!s) return;
          let used = this.w - this.r; if (used < 0) used += this.n;
          let space = this.n - used - this.ch;
          let i = 0;
          while (i < s.length && space < this.ch) {
            this.r += this.ch; if (this.r >= this.n) this.r -= this.n;
            used = this.w - this.r; if (used < 0) used += this.n;
            space = this.n - used - this.ch;
          }
          const aligned = (s.length - i) - ((s.length - i) % this.ch);
          const end = i + Math.min(aligned, Math.max(0, space));
          for (; i < end; i++) {
            this.ring[this.w] = s[i];
            this.w++; if (this.w >= this.n) this.w = 0;
          }
        };
      }
      avail(){ let a = this.w - this.r; if (a < 0) a += this.n; return a; }
      process(_, outputs){
        const out = outputs[0];
        const frames = out[0].length;
        const srcCh = this.ch;
        const need = frames * srcCh;
        const primeNeed = sampleRate * 1.0 * srcCh;
        const lowNeed = sampleRate * 0.2 * srcCh;
        let a = this.avail();
        if (!this.primed) {
          if (a < primeNeed) {
            for (let c = 0; c < out.length; c++) out[c].fill(0);
            return true;
          }
          this.primed = true;
        }
        if (a < need || a < lowNeed) {
          this.primed = false;
          for (let c = 0; c < out.length; c++) out[c].fill(0);
          return true;
        }
        let peak = 0;
        for (let i = 0; i < frames; i++) {
          let sum = 0;
          const left = this.ring[this.r];
          this.r++; if (this.r >= this.n) this.r = 0;
          sum += left;
          let right = left;
          if (srcCh > 1) {
            right = this.ring[this.r];
            this.r++; if (this.r >= this.n) this.r = 0;
            sum += right;
            for (let extra = 2; extra < srcCh; extra++) {
              this.r++; if (this.r >= this.n) this.r = 0;
            }
          }
          const absL = left < 0 ? -left : left;
          const absR = right < 0 ? -right : right;
          if (absL > peak) peak = absL;
          if (absR > peak) peak = absR;
          if (this.mix !== 'stereo' || out.length === 1) {
            const m = srcCh > 1 ? sum / 2 : left;
            out[0][i] = m;
            for (let c = 1; c < out.length; c++) out[c][i] = m;
          } else {
            out[0][i] = left;
            out[1][i] = right;
            for (let c = 2; c < out.length; c++) out[c][i] = left;
          }
        }
        this.tick++;
        if ((this.tick & 15) === 0) {
          this.port.postMessage({p: peak, f: this.avail() / srcCh / sampleRate});
        }
        return true;
      }
    });`;
  const url = URL.createObjectURL(new Blob([src], {type:'application/javascript'}));
  await ac.audioWorklet.addModule(url);
  URL.revokeObjectURL(url);
  const node = new AudioWorkletNode(ac, 'fubar-play', {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [2],
    channelCount: 2,
    channelCountMode: 'explicit'
  });
  let fillSec = 0;
  node.port.onmessage = (e) => {
    const d = e.data || {};
    livePeak = Number(d.p) || 0;
    if (d.f != null) fillSec = Number(d.f) || 0;
  };
  node.port.postMessage({ch: srcCh, mix: liveMix});
  connectLiveOut(ac, node);
  const sr = ac.sampleRate;
  return {
    node,
    buffered(){ return fillSec; },
    push(f32){
      fillSec += (f32.length / srcCh) / sr;
      node.port.postMessage({s: f32});
      return f32.length;
    }
  };
}
async function playLiveWavUrl(){
  liveAbort = new AbortController();
  if (navigator.audioSession) { try { navigator.audioSession.type = 'playback'; } catch {} }
  const el = liveMedia;
  showRadioPlayer(true);
  armHold(el);
  el.muted = false;
  el.volume = 1;
  el.loop = false;
  el.preload = 'auto';
  if (!/live\.mp3/i.test(el.currentSrc || el.src || '')) {
    el.src = 'live.mp3?v=125&t=' + Date.now();
  }
  bindMediaSession(el, null);
  document.title = 'FUBAR Live';
  startMeter();
  setLiveUi(true, livePlayingText());
  try {
    navigator.mediaSession.setPositionState({duration: 86400, playbackRate: 1, position: 0});
  } catch {}
  try { await el.play(); } catch { await el.play(); }
  await new Promise((resolve, reject) => {
    const done = (err) => {
      el.onended = null;
      el.onerror = null;
      clearInterval(tick);
      if (err) reject(err); else resolve();
    };
    el.onended = () => done(new Error('live ended'));
    el.onerror = () => done(new Error('live media error'));
    const tick = setInterval(() => {
      if (!liveWanted) { done(); return; }
      if (el.paused && document.visibilityState === 'hidden') el.play().catch(()=>{});
      if (!el.paused) livePeak = Math.max(livePeak, 0.28);
      try { navigator.mediaSession.playbackState = el.paused ? 'paused' : 'playing'; } catch {}
    }, 1000);
  });
}
async function playLiveSession(){
  audio.pause();
  try { if (liveNode) liveNode.disconnect(); } catch {}
  liveNode = null;
  liveAbort = new AbortController();
  if (navigator.audioSession) { try { navigator.audioSession.type = 'playback'; } catch {} }
  if (strategy.hold === 'media-url') {
    await playLiveWavUrl();
    return;
  }
  if (strategy.silentWav) {
    liveMedia.muted = true;
    liveMedia.volume = 0;
    liveMedia.loop = true;
    liveMedia.src = SILENT_WAV;
    try { await liveMedia.play(); } catch {}
  }
  await keepLiveAlive();
  const res = await fetch('live.pcm?v=122&t=' + Date.now(), {signal: liveAbort.signal, cache:'no-store'});
  if (!res.ok || !res.body) {
    if (res.status === 503) throw new Error('queue');
    throw new Error('live unavailable');
  }
  const reader = res.body.getReader();
  let buf = new Uint8Array(0);
  const readMore = async () => {
    const {done, value} = await reader.read();
    if (done) return false;
    const next = new Uint8Array(buf.length + value.length);
    next.set(buf);
    next.set(value, buf.length);
    buf = next;
    return true;
  };
  const take = async (n) => {
    while (buf.length < n) {
      if (!await readMore()) throw new Error('live ended');
    }
    const out = buf.slice(0, n);
    buf = buf.slice(n);
    return out;
  };
  const hdr = await take(16);
  if (new TextDecoder().decode(hdr.slice(0,8)) !== 'FUBARPCM') throw new Error('bad live header');
  const view = new DataView(hdr.buffer, hdr.byteOffset, 16);
  const rate = view.getUint32(8, true);
  const channels = Math.max(1, view.getUint16(12, true) || 1);
  let ac = (liveAc && liveAc.state !== 'closed') ? liveAc : null;
  if (!ac) {
    if (strategy.avoidSampleRate) {
      try { ac = new AudioCtx({latencyHint:'playback'}); }
      catch { ac = new AudioCtx(); }
    } else {
      try { ac = new AudioCtx({sampleRate: rate, latencyHint:'playback'}); }
      catch { ac = new AudioCtx({latencyHint:'playback'}); }
    }
  }
  liveAc = ac;
  await ac.resume();
  let tap;
  try {
    if (!strategy.avoidWorklet && ac.audioWorklet) tap = await attachWorkletRing(ac, channels);
    else tap = attachScriptRing(ac, channels);
  } catch {
    tap = attachScriptRing(ac, channels);
  }
  liveNode = tap.node;
  liveRate = rate;
  liveCh = channels;
  const resample = makeResampler(rate, ac.sampleRate, channels);
  startMeter();
  if (navigator.mediaSession) {
    try {
      navigator.mediaSession.metadata = new MediaMetadata({title:'FUBAR Live', artist:'FUBAR'});
      navigator.mediaSession.playbackState = 'playing';
      navigator.mediaSession.setActionHandler('pause', () => {
        try { holdEl().pause(); } catch {}
        try { ac.suspend(); } catch {}
      });
      navigator.mediaSession.setActionHandler('play', () => {
        try { ac.resume(); } catch {}
        try { holdEl().play(); } catch {}
      });
      navigator.mediaSession.setActionHandler('stop', () => stopLive());
    } catch {}
  }
  setLiveUi(true, livePlayingText());
  const keep = setInterval(keepLiveAlive, 1500);
  try {
    while (liveWanted) {
      while (liveWanted && tap.buffered() > 1.8) {
        await new Promise(r => setTimeout(r, 25));
      }
      if (!liveWanted) break;
      const frameBytes = 2 * channels;
      if (buf.length < 2048 && !await readMore()) break;
      let bytes = buf.length - (buf.length % frameBytes);
      const cap = Math.max(frameBytes * 512, Math.floor(rate / 5) * frameBytes);
      if (bytes > cap) bytes = cap;
      if (bytes < frameBytes) continue;
      const chunk = buf.slice(0, bytes);
      buf = buf.slice(bytes);
      const aligned = (chunk.byteOffset % 2 === 0) ? chunk : chunk.slice();
      const samples = new Int16Array(aligned.buffer, aligned.byteOffset, aligned.byteLength / 2);
      tap.push(resample(samples));
    }
  } finally {
    clearInterval(keep);
    try { tap.node.disconnect(); } catch {}
    liveNode = null;
  }
}
async function startLive(){
  liveWanted = true;
  setLiveUi(true, 'Connecting to live capture…');
  while (liveWanted) {
    try {
      await playLiveSession();
      if (!liveWanted) return;
      setLiveUi(true, 'Live paused — reconnecting…');
      await new Promise(r => setTimeout(r, 400));
    } catch (err) {
      if (!liveWanted) return;
      if (err && err.message === 'queue') {
        liveWanted = false;
        setLiveUi(false, 'Live queue is full — tap Listen live to wait again');
        return;
      }
      setLiveUi(true, 'Live dropped — reconnecting…');
      await new Promise(r => setTimeout(r, 600));
    }
  }
}
liveBtn.addEventListener('click', () => {
  if (liveWanted || livePlaying) { stopLive(); return; }
  if (strategy.hold === 'media-url') startRadioFromGesture();
  else if (strategy.unlock) unlockLiveAudio();
  startLive();
});
document.addEventListener('visibilitychange', () => { if (liveWanted) keepLiveAlive(); });
window.addEventListener('pageshow', () => { if (liveWanted) keepLiveAlive(); });
window.addEventListener('focus', () => { if (liveWanted) keepLiveAlive(); });
document.addEventListener('resume', () => { if (liveWanted) keepLiveAlive(); });
function esc(t){
  return String(t||'').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}
function safeUrl(u){
  return /^https?:\/\//i.test(u||'') ? u : '';
}
async function loadStations(){
  const box = document.getElementById('stations');
  const netCount = document.getElementById('netCount');
  try {
    const res = await fetch('https://gearsqueens.online/fubar-net/servers', {cache:'no-store'});
    const data = await res.json();
    const servers = data.servers || [];
    netCount.textContent = servers.length ? (servers.length + ' on air') : 'none listed';
    if (!servers.length){
      box.className = 'empty';
      box.textContent = 'No public FUBAR stations right now. Tick Public Server in the app to list this one.';
      return;
    }
    box.className = '';
    box.innerHTML = servers.map(s => {
      const url = safeUrl(s.url);
      if (!url) return '';
      const freq = s.frequencyMhz ? Number(s.frequencyMhz).toFixed(3) + ' MHz' : '';
      const state = s.recording ? 'recording' : (s.live ? 'on air' : 'idle');
      const people = (s.listeners||0) + '/' + (s.listenerLimit||5);
      const playing = s.nowPlaying ? `<div class="when">Now playing · ${esc(s.nowPlaying)}</div>` : '';
      return `<a class="station" href="${esc(url)}" target="_blank" rel="noopener">
        <div><div class="name">${esc(s.name||'FUBAR')}</div>
        <div class="when">${esc(freq)} · ${esc(state)} · ${esc(people)} listening</div>${playing}</div>
        <span class="visit">Open</span></a>`;
    }).join('');
  } catch {
    netCount.textContent = '';
    box.className = 'empty';
    box.textContent = 'Could not reach the public station list.';
  }
}
function showNowPlaying(text){
  const board = document.getElementById('nowBoard');
  const title = document.getElementById('nowPlayingText');
  const value = String(text || '').trim();
  nowPlayingTitle = value;
  if (!value){
    title.textContent = 'Waiting for the operator';
    document.title = p25StatusLine ? (p25StatusLine + ' · FUBAR') : 'FUBAR Captures';
  } else {
    title.textContent = value;
    document.title = value + ' · FUBAR';
  }
  board.classList.toggle('empty', !value && !p25StatusLine);
}
function showP25Status(text){
  const board = document.getElementById('nowBoard');
  const el = document.getElementById('p25StatusText');
  const value = String(text || '').trim();
  p25StatusLine = value;
  if (!el) return;
  if (!value){
    el.hidden = true;
    el.textContent = '';
  } else {
    el.hidden = false;
    el.textContent = value;
  }
  board.classList.toggle('empty', !nowPlayingTitle && !value);
  if (!nowPlayingTitle) {
    document.title = value ? (value + ' · FUBAR') : 'FUBAR Captures';
  }
}
function switchTab(name){
  document.querySelectorAll('#siteTabs button').forEach(btn => {
    btn.classList.toggle('on', btn.getAttribute('data-tab') === name);
  });
  document.querySelectorAll('.tabpanel').forEach(panel => {
    panel.classList.toggle('on', panel.id === ('tab-' + name));
  });
}
document.querySelectorAll('#siteTabs button').forEach(btn => {
  btn.addEventListener('click', () => switchTab(btn.getAttribute('data-tab')));
});
function formatHeardEvent(ev){
  const where = ev.channel === 'output' ? 'output' : (ev.channel === 'input' ? 'input' : 'tuned');
  const detail = String(ev.detail || '');
  switch (String(ev.kind || '')) {
    case 'dtmf_sequence': return 'DTMF  ' + detail + '  (' + where + ')';
    case 'dtmf_digit': return '';
    case 'ctcss':
      return detail === 'clear' ? ('CTCSS  clear  (' + where + ')') : ('CTCSS  ' + detail + '  (' + where + ')');
    case 'dcs':
      return detail === 'clear' ? ('DCS  clear  (' + where + ')') : ('DCS  ' + detail + '  (' + where + ')');
    case 'carrier_open': return 'Carrier open  (' + where + ')';
    case 'carrier_close': return 'Carrier closed  (' + where + ')';
    default: return '';
  }
}
function renderDecodePanels(state){
  const s = state || {};
  const rds = s.rds || {};
  const tones = s.tones || {};
  const repeater = s.repeater || {};
  const sstv = s.sstv || {};
  const rdsSummary = document.getElementById('rdsSummary');
  const rdsMeta = document.getElementById('rdsMeta');
  const tonesSummary = document.getElementById('tonesSummary');
  const tonesMeta = document.getElementById('tonesMeta');
  const tonesHeard = document.getElementById('tonesHeard');
  const sstvSummary = document.getElementById('sstvSummary');
  const sstvMeta = document.getElementById('sstvMeta');
  const sstvImages = document.getElementById('sstvImages');
  if (rdsSummary) rdsSummary.textContent = rds.summary || 'Waiting for FM station data';
  if (rdsMeta) {
    if (!sdrTownConfig.enabled) {
      rdsMeta.textContent = 'Turn on SDR Town control in FUBAR Tools → Settings, keep SDR Town running, then tune WFM.';
    } else {
      const bits = [];
      if (rds.programmeService) bits.push('<b>Name</b> ' + esc(rds.programmeService));
      if (rds.pi) bits.push('<b>PI</b> ' + esc(rds.pi));
      if (rds.pty != null) bits.push('<b>PTY</b> ' + esc(rds.pty));
      if (rds.trafficAnnouncement) bits.push('<b>Traffic announcement</b>');
      if (rds.radioText) bits.push('<b>Text</b> ' + esc(rds.radioText));
      if (rds.modeHint) bits.push(esc(rds.modeHint));
      rdsMeta.innerHTML = bits.join('<br>') || 'Listening…';
    }
  }
  if (tonesSummary) tonesSummary.textContent = tones.summary || 'Waiting for CTCSS / DCS';
  if (tonesMeta) {
    if (!sdrTownConfig.enabled) {
      tonesMeta.textContent = 'Turn on SDR Town control in FUBAR Tools → Settings, keep SDR Town running, then tune NFM.';
    } else {
      const bits = [];
      if (tones.ctcssFresh && Number(tones.ctcssHz) > 0) bits.push('<b>CTCSS</b> ' + Number(tones.ctcssHz).toFixed(1) + ' Hz');
      const aliases = Array.isArray(tones.dcsAliases) ? tones.dcsAliases : [];
      if (aliases.length) bits.push('<b>DCS</b> ' + aliases.map(esc).join(' / '));
      const dtmf = tones.dtmf || {};
      if (dtmf.enabled) {
        if (dtmf.toneActive && dtmf.digit) bits.push('<b>DTMF</b> ' + esc(dtmf.digit) + ' (active)');
        else if (dtmf.sequence) bits.push('<b>DTMF</b> ' + esc(dtmf.sequence) + ' …');
        else if (dtmf.lastSequence) bits.push('<b>Last DTMF</b> ' + esc(dtmf.lastSequence));
        else bits.push('<b>DTMF</b> listening');
      } else {
        bits.push('Enable <b>Repeater Control Monitor</b> in SDR Town for DTMF + heard list');
      }
      if (repeater.enabled) {
        const outMHz = Number(repeater.outputHz || 0) / 1e6;
        const inMHz = Number(repeater.inputHz || 0) / 1e6;
        bits.push('<b>Repeater monitor</b> out ' + outMHz.toFixed(5) + ' / in ' + inMHz.toFixed(5) +
          (repeater.dualWatchActive ? ' · dual-watch on' : ''));
      }
      if (tones.modeHint) bits.push(esc(tones.modeHint));
      tonesMeta.innerHTML = bits.join('<br>') || 'Searching…';
    }
  }
  if (tonesHeard) {
    const events = Array.isArray(repeater.events) ? repeater.events : [];
    const lines = events.map(formatHeardEvent).filter(Boolean);
    tonesHeard.textContent = lines.slice(-40).join('\n');
  }
  if (sstvSummary) sstvSummary.textContent = sstv.summary || 'Open Tools → SSTV Images in SDR Town';
  if (sstvMeta) {
    if (!sdrTownConfig.enabled) {
      sstvMeta.textContent = 'Turn on SDR Town control in FUBAR Tools → Settings to show live SSTV status and pictures.';
    } else {
      const bits = [];
      if (sstv.status) bits.push('<b>Status</b> ' + esc(sstv.status));
      if (sstv.outputDirectory) bits.push('<b>Folder</b> ' + esc(sstv.outputDirectory));
      if (sstv.modeHint) bits.push(esc(sstv.modeHint));
      sstvMeta.innerHTML = bits.join('<br>');
    }
  }
  if (sstvImages) {
    const images = Array.isArray(sstv.images) ? sstv.images : [];
    sstvImages.innerHTML = images.map((img, index) => {
      const path = encodeURIComponent(String(img.path || ''));
      const label = esc(img.label || img.name || ('Picture ' + (index + 1)));
      return '<a href="api/sdr-town/sstv-file?path=' + path + '" target="_blank" rel="noopener">' +
        '<img src="api/sdr-town/sstv-file?path=' + path + '" alt="' + label + '">' +
        '<div class="cap">' + label + '</div></a>';
    }).join('');
  }
}
function sdrTownMessage(text){
  const el = document.getElementById('sdrTownStatus');
  if (el) el.textContent = text || '';
}
function sdrControlMs(ms){
  ms = Math.max(0, Number(ms)||0);
  const total = Math.ceil(ms / 1000);
  const m = Math.floor(total / 60);
  const s = (total % 60).toString().padStart(2,'0');
  return m + ':' + s;
}
function sdrEffectiveMode(state){
  const s = state || sdrTownLiveState || {};
  const p25 = s.p25 || {};
  if (Number(p25.controlFrequencyHz || 0) > 0 && !p25.monitorDisabledReason) return 'P25';
  return String(s.mode || '').toUpperCase();
}
function sdrHighlightMode(mode){
  const current = String(mode || '').toUpperCase();
  document.querySelectorAll('.sdrModeAction').forEach(btn => {
    const value = String(btn.getAttribute('data-set-mode') || '').toUpperCase();
    btn.classList.toggle('on', !!current && value === current);
  });
  const select = document.getElementById('sdrMode');
  if (select && current && current !== 'P25') {
    const opt = Array.from(select.options).find(o => String(o.value).toUpperCase() === current);
    if (opt) select.value = opt.value;
  }
}
function sdrSetControlsEnabled(){
  const active = !!(sdrControlSession && sdrControlSession.canControl);
  const enabled = !!sdrTownConfig.enabled;
  const canMode = enabled && active && !!sdrTownConfig.allowMode;
  const canP25 = enabled && active && !!sdrTownConfig.allowP25Control;
  const canSdrplay = enabled && active && (!!sdrTownConfig.allowTune || !!sdrTownConfig.allowRfGain);
  document.getElementById('sdrFreq').disabled = !enabled || !active || !sdrTownConfig.allowTune;
  document.getElementById('sdrMode').disabled = !canMode;
  document.getElementById('sdrBw').disabled = !enabled || !active || !sdrTownConfig.allowTune;
  document.getElementById('sdrLpfEnabled').disabled = !enabled || !active || !sdrTownConfig.allowTune;
  document.getElementById('sdrLpf').disabled = !enabled || !active || !sdrTownConfig.allowTune ||
    !document.getElementById('sdrLpfEnabled').checked;
  document.getElementById('sdrGain').disabled = !enabled || !active || !sdrTownConfig.allowRfGain;
  document.getElementById('sdrVolume').disabled = !enabled || !active || !sdrTownConfig.allowTune;
  document.getElementById('sdrDirectSamp').disabled = !enabled || !active || !sdrTownConfig.allowTune;
  document.getElementById('sdrTuneBtn').disabled = !enabled || !active || !sdrTownConfig.allowTune;
  document.getElementById('sdrP25Known').disabled = !canP25;
  document.getElementById('sdrP25Freq').disabled = !canP25;
  document.getElementById('sdrP25Btn').disabled = !canP25;
  document.getElementById('sdrTakeControlBtn').disabled =
    !enabled || active || (sdrControlSession && sdrControlSession.role === 'queued');
  document.getElementById('sdrExtendControlBtn').disabled =
    !enabled || !active || Number(sdrControlSession.extensionsUsed || 0) >= Number(sdrControlSession.maxExtensions || 0);
  document.getElementById('sdrReleaseControlBtn').disabled =
    !enabled || !(active || (sdrControlSession && sdrControlSession.role === 'queued'));
  document.querySelectorAll('.sdrModeAction').forEach(btn => {
    const mode = String(btn.getAttribute('data-set-mode') || '').toUpperCase();
    btn.disabled = mode === 'P25' ? !canP25 : !canMode;
  });
  const hint = document.getElementById('sdrModeHint');
  if (hint) {
    if (!enabled) hint.textContent = 'Enable SDR Town control in FUBAR settings to switch modes from the website.';
    else if (!active) hint.textContent = 'Click Take control first, then tap a demod/mode. P25 uses Monitor CC at the frequency below.';
    else if (!sdrTownConfig.allowMode && !sdrTownConfig.allowP25Control) hint.textContent = 'Mode switching is disabled by the FUBAR admin.';
    else hint.textContent = 'Analog demods leave P25 Monitor CC and stick. P25 starts control-channel monitoring at the frequency below.';
  }
  [
    'sdrplayAntenna','sdrplayAgc','sdrplayIfgr','sdrplayRfgr','sdrplayBw','sdrplayBiasT','sdrplayRfNotch',
    'sdrplayDabNotch','sdrplayExtRef','sdrplayHdr','sdrplayIqCorr','sdrplayDivMode','sdrplayDivPhase',
    'sdrplayDivAmp','sdrplayApplyBtn'
  ].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.disabled = !canSdrplay;
  });
}
function sdrSettingBool(settings, key, fallback){
  if (!settings || settings[key] == null) return !!fallback;
  const v = String(settings[key]).toLowerCase();
  return v === 'true' || v === '1' || v === 'on';
}
function sdrRenderDevicePanels(state){
  const s = state || {};
  const rtlBox = document.getElementById('sdrRtlBox');
  if (rtlBox) rtlBox.style.display = (s.rtlDirectSamplingAvailable === false) ? 'none' : '';
  const gainLabel = document.getElementById('sdrGainLabel');
  const sp = s.sdrplay;
  if (gainLabel) gainLabel.textContent = (sp && sp.active) ? 'RFGR dB' : 'RF gain dB';
  const box = document.getElementById('sdrplayBox');
  if (!box) return;
  if (!sp || !sp.active) {
    box.classList.remove('on');
    return;
  }
  box.classList.add('on');
  const model = document.getElementById('sdrplayModel');
  if (model) {
    model.textContent = (sp.model || 'SDRplay') +
      (sp.duoModeLabel ? (' · ' + sp.duoModeLabel) : '') +
      (sp.rxChannel != null ? (' · ch' + sp.rxChannel) : '');
  }
  const features = sp.features || {};
  const show = (wrapId, on) => {
    const el = document.getElementById(wrapId);
    if (el) el.style.display = on ? '' : 'none';
  };
  show('sdrplayAntWrap', features.antenna !== false);
  show('sdrplayAgcWrap', features.agc !== false);
  show('sdrplayIfgrWrap', features.ifgr !== false);
  show('sdrplayRfgrWrap', features.rfgr !== false);
  show('sdrplayBwWrap', features.bandwidth !== false);
  show('sdrplayBiasWrap', !!features.biasT);
  show('sdrplayRfNotchWrap', !!features.rfNotch);
  show('sdrplayDabWrap', !!features.dabNotch);
  show('sdrplayExtRefWrap', !!features.extRef);
  show('sdrplayHdrWrap', !!features.hdr);
  show('sdrplayIqWrap', !!features.iqCorr);
  show('sdrplayDivWrap', !!features.diversity);
  show('sdrplayPhaseWrap', !!features.diversity);
  show('sdrplayAmpWrap', !!features.diversity);

  const ant = document.getElementById('sdrplayAntenna');
  if (ant && document.activeElement !== ant) {
    const ants = sp.antennas || [];
    ant.innerHTML = ants.map(a => '<option value="' + String(a).replace(/"/g,'&quot;') + '">' + a + '</option>').join('');
    if (sp.antenna) ant.value = sp.antenna;
  }
  const focusedSdrplay = document.activeElement && String(document.activeElement.id || '').indexOf('sdrplay') === 0;
  if (!focusedSdrplay) {
    const setChk = (id, on) => { const el = document.getElementById(id); if (el) el.checked = !!on; };
    const setVal = (id, v) => { const el = document.getElementById(id); if (el && v != null && v !== '') el.value = v; };
    setChk('sdrplayAgc', sp.agcEnabled);
    setVal('sdrplayIfgr', sp.ifgrDb != null ? Number(sp.ifgrDb).toFixed(0) : '');
    setVal('sdrplayRfgr', sp.rfgrDb != null ? Number(sp.rfgrDb).toFixed(0) : '');
    const bw = document.getElementById('sdrplayBw');
    if (bw) {
      const list = sp.bandwidthsHz || [];
      bw.innerHTML = '<option value="0">Driver default</option>' +
        list.map(hz => '<option value="' + hz + '">' + (hz >= 1e6 ? (hz/1e6).toFixed(3) + ' MHz' : (hz/1e3).toFixed(0) + ' kHz') + '</option>').join('');
      bw.value = String(sp.bandwidthHz || 0);
    }
    const settings = sp.settings || {};
    setChk('sdrplayBiasT', sdrSettingBool(settings, 'biasT_ctrl', false));
    setChk('sdrplayRfNotch', sdrSettingBool(settings, 'rfnotch_ctrl', false));
    setChk('sdrplayDabNotch', sdrSettingBool(settings, 'dabnotch_ctrl', false));
    setChk('sdrplayExtRef', sdrSettingBool(settings, 'extref_ctrl', false));
    setChk('sdrplayHdr', sdrSettingBool(settings, 'hdr_ctrl', false));
    setChk('sdrplayIqCorr', sdrSettingBool(settings, 'iqcorr_ctrl', true));
    const div = sp.diversity || {};
    setVal('sdrplayDivMode', div.mode || 'off');
    setVal('sdrplayDivPhase', div.phaseDeg != null ? Number(div.phaseDeg).toFixed(1) : '0');
    setVal('sdrplayDivAmp', div.amplitudeB != null ? Number(div.amplitudeB).toFixed(2) : '1');
  }
  const hintEl = document.getElementById('sdrplayHint');
  if (hintEl) {
    hintEl.textContent = (sp.antennaDescription || 'SDRplay controls for the active device.') +
      (features.diversity ? ' Dual Tuner diversity/null-steer is host DSP.' : '');
  }
  sdrSetControlsEnabled();
}
function sdrRenderControlSession(){
  const el = document.getElementById('sdrControlState');
  if (!el) return;
  const session = sdrControlSession || {};
  const active = !!session.canControl;
  const remaining = active && sdrControlDeadlineMs
    ? Math.max(0, sdrControlDeadlineMs - Date.now())
    : Number(session.remainingMs || 0);
  const warningMs = Number(session.warningMs || sdrTownConfig.warningMs || 60000);
  el.classList.toggle('warn', active && remaining > 0 && remaining <= warningMs);
  if (!sdrTownConfig.enabled) {
    el.textContent = 'SDR Town website control is disabled.';
  } else if (active) {
    const ext = Number(session.extensionsUsed || 0);
    const max = Number(session.maxExtensions || sdrTownConfig.maxExtensions || 0);
    el.textContent = (remaining <= warningMs && remaining > 0 ? 'Warning: ' : '') +
      'You have control - ' + sdrControlMs(remaining) + ' left - extensions ' + ext + '/' + max;
  } else if (session.role === 'queued') {
    el.textContent = 'Queued #' + Number(session.queuePosition || 0) +
      ' - current operator ' + (session.activeName || 'operator') +
      ' has ' + sdrControlMs(session.remainingMs || 0) + ' left.';
  } else if (session.active) {
    el.textContent = (session.activeName || 'Another operator') +
      ' has control for ' + sdrControlMs(session.remainingMs || 0) + '.';
  } else {
    el.textContent = 'No one has control. Click Take control to tune SDR Town.';
  }
  sdrSetControlsEnabled();
}
function sdrAdoptSession(session){
  const wasActive = sdrControlWasActive;
  sdrControlSession = session || {role:'idle', canControl:false, remainingMs:0};
  sdrControlWasActive = !!sdrControlSession.canControl;
  if (!sdrControlWasActive || !wasActive) sdrControlSeededForLease = false;
  sdrControlDeadlineMs = sdrControlWasActive
    ? Date.now() + Number(sdrControlSession.remainingMs || 0)
    : 0;
  sdrRenderControlSession();
}
function sdrPopulateP25ControlChannels(channels){
  const select = document.getElementById('sdrP25Known');
  if (!select) return;
  const current = select.value;
  const rows = (channels || []).filter(ch => Number(ch.frequencyMHz || ch.frequencyHz || 0) > 0);
  select.innerHTML = '<option value="">Manual / choose known CC</option>' + rows.map(ch => {
    const mhz = Number(ch.frequencyMHz || (Number(ch.frequencyHz || 0) / 1000000));
    const label = (ch.label ? String(ch.label) + ' - ' : '') + mhz.toFixed(5) + ' MHz';
    return '<option value="' + mhz.toFixed(5) + '">' + label.replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])) + '</option>';
  }).join('');
  if (current && Array.from(select.options).some(o => o.value === current)) select.value = current;
}
async function sdrControlAction(action){
  try {
    const res = await fetch('api/sdr-town/control-session', {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({action, clientId:sdrControlClientId, name:sdrControlName})
    });
    const data = await res.json();
    if (data.session) sdrAdoptSession(data.session);
    if (!data.ok && data.error) sdrTownMessage(data.error);
    return data;
  } catch (error) {
    sdrTownMessage('Control session failed - ' + (error && error.message ? error.message : 'network error'));
    return {ok:false};
  }
}
function sdrSeedFields(state){
  const active = !!(sdrControlSession && sdrControlSession.canControl);
  if (active && sdrControlSeededForLease) return;
  const focused = document.activeElement && ['sdrFreq','sdrMode','sdrBw','sdrLpf','sdrGain','sdrVolume','sdrDirectSamp','sdrP25Freq','sdrP25Known'].includes(document.activeElement.id);
  // Never overwrite while the operator is typing — otherwise HF entries like 7 MHz
  // snap back to the live status frequency (often 100 MHz) every poll.
  if (focused) return;
  const s = state || {};
  if (s.frequencyMHz) document.getElementById('sdrFreq').value = Number(s.frequencyMHz).toFixed(5);
  if (s.mode && sdrTownConfig.allowMode) document.getElementById('sdrMode').value = s.mode;
  if (s.bandwidthHz) document.getElementById('sdrBw').value = Number(s.bandwidthHz / 1000).toFixed(1);
  if (s.lpfHz) document.getElementById('sdrLpf').value = Number(s.lpfHz / 1000).toFixed(1);
  if (s.audioLpfEnabled != null) document.getElementById('sdrLpfEnabled').checked = !!s.audioLpfEnabled;
  if (s.rfGainDb != null && sdrTownConfig.allowRfGain) document.getElementById('sdrGain').value = Number(s.rfGainDb).toFixed(1);
  if (s.volume != null) document.getElementById('sdrVolume').value = Number(s.volume * 100).toFixed(0);
  if (s.directSampling != null) document.getElementById('sdrDirectSamp').value = String(s.directSampling);
  if (s.p25 && s.p25.controlFrequencyHz) document.getElementById('sdrP25Freq').value = Number(s.p25.controlFrequencyHz / 1000000).toFixed(5);
  sdrPopulateP25ControlChannels(s.knownControlChannels || []);
  if (!sdrCommandInFlight) sdrHighlightMode(sdrEffectiveMode(s));
  if (active) sdrControlSeededForLease = true;
  sdrSetControlsEnabled();
}
async function loadSdrTownControl(){
  const panel = document.getElementById('sdrTownPanel');
  try {
    const res = await fetch('api/sdr-town/config', {cache:'no-store'});
    sdrTownConfig = await res.json();
    panel.classList.toggle('on', !!sdrTownConfig.enabled);
    if (!sdrTownConfig.enabled) {
      sdrAdoptSession({role:'idle', canControl:false, remainingMs:0});
      renderDecodePanels({});
      sdrRenderDevicePanels({});
      return;
    }
    await sdrControlAction('status');
    const statusRes = await fetch('api/sdr-town/status', {cache:'no-store'});
    const status = await statusRes.json();
    if (status.ok && status.state) {
      const s = status.state;
      sdrTownLiveState = s;
      sdrSeedFields(s);
      if (!sdrCommandInFlight) sdrHighlightMode(sdrDesiredMode || sdrEffectiveMode(s));
      renderDecodePanels(s);
      sdrRenderDevicePanels(s);
      const p25Label = (s.p25 && s.p25.talkgroupStatusLabel) ? String(s.p25.talkgroupStatusLabel).trim() : '';
      const p25Note = s.p25 && s.p25.monitorDisabledReason ? (' · P25 monitor disabled: ' + s.p25.monitorDisabledReason) : '';
      const p25Live = p25Label ? (' · ' + p25Label) : (s.p25 && s.p25.followTalkgroupId ? (' · TG ' + s.p25.followTalkgroupId) : '');
      const lpfNote = s.audioLpfEnabled ? (' · LPF ' + Number((s.lpfHz || 0) / 1000).toFixed(1) + ' kHz') : ' · LPF off';
      const volNote = s.volume != null ? (' · Vol ' + Number(s.volume * 100).toFixed(0) + '%') : '';
      const modeShown = sdrEffectiveMode(s) || (s.mode || '');
      sdrTownMessage('SDR Town ready · ' + Number(s.frequencyMHz || 0).toFixed(5) + ' MHz · ' + modeShown + ' · BW ' + Number((s.bandwidthHz || 0) / 1000).toFixed(1) + ' kHz' + lpfNote + volNote + p25Live + p25Note);
    } else {
      renderDecodePanels({});
      sdrRenderDevicePanels({});
      sdrTownMessage(status.error || 'SDR Town not reachable. Start SDR Town with --control-server.');
    }
  } catch {
    panel.classList.remove('on');
    renderDecodePanels({});
    sdrRenderDevicePanels({});
  }
  sdrSetControlsEnabled();
}
async function postSdrTown(path, payload){
  if (!sdrControlSession || !sdrControlSession.canControl) {
    sdrTownMessage('Click Take control before sending SDR Town commands.');
    return;
  }
  sdrTownMessage('Sending command...');
  sdrCommandInFlight = true;
  try {
    payload = Object.assign({}, payload || {}, {clientId:sdrControlClientId, force:true});
    const res = await fetch(path, {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify(payload || {})
    });
    const text = await res.text();
    let data = {};
    try { data = text ? JSON.parse(text) : {}; }
    catch { data = {ok:false, error:text || ('HTTP ' + res.status)}; }
    if (data.ok) {
      const s = data.state || {};
      sdrTownLiveState = s;
      const appliedMhz = Number(s.frequencyMHz || payload.frequencyMHz || 0);
      const requestedMhz = Number(payload.frequencyMHz || 0);
      if (requestedMhz > 0 && Math.abs(appliedMhz - requestedMhz) > 0.00005) {
        sdrTownMessage('Tune reported OK but RF stayed on ' + appliedMhz.toFixed(5) + ' MHz (wanted ' + requestedMhz.toFixed(5) + ').');
      } else {
        const bw = s.bandwidthHz || (payload.bandwidthKHz ? payload.bandwidthKHz * 1000 : 0);
        const modeShown = sdrDesiredMode || sdrEffectiveMode(s) || (s.mode || payload.mode || '');
        sdrTownMessage('Applied · ' + appliedMhz.toFixed(5) + ' MHz · ' + modeShown + (bw ? (' · BW ' + Number(bw / 1000).toFixed(1) + ' kHz') : ''));
      }
      sdrDesiredMode = '';
      sdrHighlightMode(sdrEffectiveMode(s));
      if (s && Object.keys(s).length) {
        renderDecodePanels(s);
        sdrRenderDevicePanels(s);
      }
      await sdrControlAction('status');
    } else {
      if (data.session) sdrAdoptSession(data.session);
      sdrDesiredMode = '';
      sdrHighlightMode(sdrEffectiveMode(sdrTownLiveState));
      sdrTownMessage(data.error || ('Command failed · HTTP ' + res.status));
    }
  } catch (error) {
    sdrDesiredMode = '';
    sdrHighlightMode(sdrEffectiveMode(sdrTownLiveState));
    sdrTownMessage('Command failed · ' + (error && error.message ? error.message : 'network error'));
  } finally {
    sdrCommandInFlight = false;
  }
}
document.getElementById('sdrTakeControlBtn').addEventListener('click', async () => {
  await sdrControlAction('take');
  await loadSdrTownControl();
});
document.getElementById('sdrExtendControlBtn').addEventListener('click', async () => {
  await sdrControlAction('extend');
});
document.getElementById('sdrReleaseControlBtn').addEventListener('click', async () => {
  await sdrControlAction('release');
  await loadSdrTownControl();
});
document.getElementById('sdrLpfEnabled').addEventListener('change', () => {
  sdrSetControlsEnabled();
});
document.getElementById('sdrDirectSamp').addEventListener('change', () => {
  const mode = Number(document.getElementById('sdrDirectSamp').value);
  postSdrTown('api/sdr-town/direct-sampling', {directSampling: mode});
});
document.getElementById('sdrplayApplyBtn').addEventListener('click', () => {
  const payload = {
    antenna: document.getElementById('sdrplayAntenna').value,
    agc: document.getElementById('sdrplayAgc').checked,
    ifgrDb: Number(document.getElementById('sdrplayIfgr').value),
    rfgrDb: Number(document.getElementById('sdrplayRfgr').value),
    bandwidthHz: Number(document.getElementById('sdrplayBw').value || 0),
    biasT: document.getElementById('sdrplayBiasT').checked,
    rfNotch: document.getElementById('sdrplayRfNotch').checked,
    dabNotch: document.getElementById('sdrplayDabNotch').checked,
    extRef: document.getElementById('sdrplayExtRef').checked,
    hdr: document.getElementById('sdrplayHdr').checked,
    iqCorr: document.getElementById('sdrplayIqCorr').checked,
    diversity: {
      mode: document.getElementById('sdrplayDivMode').value,
      phaseDeg: Number(document.getElementById('sdrplayDivPhase').value || 0),
      amplitudeB: Number(document.getElementById('sdrplayDivAmp').value || 1)
    }
  };
  postSdrTown('api/sdr-town/sdrplay', payload);
});
document.getElementById('sdrP25Known').addEventListener('change', () => {
  const value = document.getElementById('sdrP25Known').value;
  if (value) document.getElementById('sdrP25Freq').value = value;
});
document.getElementById('sdrTuneBtn').addEventListener('click', () => {
  const frequencyMHz = Number(document.getElementById('sdrFreq').value);
  const mode = document.getElementById('sdrMode').value;
  const bwText = document.getElementById('sdrBw').value.trim();
  const lpfText = document.getElementById('sdrLpf').value.trim();
  const gainText = document.getElementById('sdrGain').value.trim();
  const volumeText = document.getElementById('sdrVolume').value.trim();
  const payload = {frequencyMHz, mode};
  if (bwText) payload.bandwidthKHz = Number(bwText);
  payload.audioLpfEnabled = document.getElementById('sdrLpfEnabled').checked;
  if (lpfText) payload.lpfKHz = Number(lpfText);
  if (gainText) payload.rfGainDb = Number(gainText);
  if (volumeText) payload.volume = Math.max(0, Math.min(1, Number(volumeText) / 100));
  postSdrTown('api/sdr-town/tune', payload);
});
document.getElementById('sdrP25Btn').addEventListener('click', () => {
  const p25Text = document.getElementById('sdrP25Freq').value.trim() || document.getElementById('sdrP25Known').value || document.getElementById('sdrFreq').value;
  const frequencyMHz = Number(p25Text);
  postSdrTown('api/sdr-town/p25-control', {frequencyMHz, autoFollow:true});
});
async function sdrApplyMode(mode){
  mode = String(mode || '').toUpperCase();
  if (!mode) return;
  if (!sdrTownConfig.enabled) {
    sdrTownMessage('Enable SDR Town control in FUBAR Tools → Settings first.');
    return;
  }
  if (!sdrControlSession || !sdrControlSession.canControl) {
    switchTab('control');
    sdrTownMessage('Click Take control first, then tap a demod/mode.');
    return;
  }
  const select = document.getElementById('sdrMode');
  if (select) {
    const opt = Array.from(select.options).find(o => String(o.value).toUpperCase() === mode);
    if (opt) select.value = opt.value;
  }
  sdrDesiredMode = mode;
  sdrHighlightMode(mode);
  if (mode === 'P25') {
    if (!sdrTownConfig.allowP25Control) {
      sdrDesiredMode = '';
      sdrTownMessage('P25 control is disabled by the FUBAR admin.');
      return;
    }
    const p25Text = document.getElementById('sdrP25Freq').value.trim() ||
      document.getElementById('sdrP25Known').value ||
      document.getElementById('sdrFreq').value;
    const frequencyMHz = Number(p25Text);
    if (!(frequencyMHz > 0)) {
      sdrDesiredMode = '';
      switchTab('control');
      sdrTownMessage('Enter a P25 control-channel frequency first, then tap P25.');
      return;
    }
    await postSdrTown('api/sdr-town/p25-control', {frequencyMHz, autoFollow:true});
    return;
  }
  if (!sdrTownConfig.allowMode) {
    sdrDesiredMode = '';
    sdrTownMessage('Mode switching is disabled by the FUBAR admin.');
    return;
  }
  await postSdrTown('api/sdr-town/mode', {mode});
}
document.querySelectorAll('.sdrModeAction').forEach(btn => {
  btn.addEventListener('click', () => sdrApplyMode(btn.getAttribute('data-set-mode')));
});

let satWaterfallRows = [];
function satDrawSpectrum(bins){
  const canvas = document.getElementById('satSpectrum');
  if (!canvas || !bins || !bins.length) return;
  const ctx = canvas.getContext('2d');
  const w = canvas.width, h = canvas.height;
  ctx.fillStyle = '#000';
  ctx.fillRect(0,0,w,h);
  ctx.strokeStyle = '#39FF14';
  ctx.beginPath();
  for (let i = 0; i < bins.length; ++i) {
    const x = i / (bins.length - 1) * (w - 1);
    const db = Number(bins[i]);
    const n = Math.max(0, Math.min(1, (db + 120) / 100));
    const y = h - 1 - n * (h - 2);
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  }
  ctx.stroke();
  satWaterfallRows.unshift(bins.slice());
  if (satWaterfallRows.length > 80) satWaterfallRows.length = 80;
  const wf = document.getElementById('satWaterfall');
  if (!wf) return;
  const wctx = wf.getContext('2d');
  const img = wctx.createImageData(wf.width, wf.height);
  for (let row = 0; row < satWaterfallRows.length && row < wf.height; ++row) {
    const line = satWaterfallRows[row];
    for (let x = 0; x < wf.width; ++x) {
      const bi = Math.min(line.length - 1, Math.floor(x * line.length / wf.width));
      const n = Math.max(0, Math.min(1, (Number(line[bi]) + 120) / 100));
      const o = (row * wf.width + x) * 4;
      img.data[o] = Math.floor(n * 255);
      img.data[o+1] = Math.floor(n * 180);
      img.data[o+2] = Math.floor((1 - n) * 180);
      img.data[o+3] = 255;
    }
  }
  wctx.putImageData(img, 0, 0);
}
async function loadSatcom(){
  const line = document.getElementById('satStatusLine');
  const logEl = document.getElementById('satDecodeLog');
  const passEl = document.getElementById('satPassList');
  const armedEl = document.getElementById('satPassArmed');
  const tleEl = document.getElementById('satTleAge');
  try {
    if (!sdrTownConfig || !sdrTownConfig.enabled) {
      if (line) line.textContent = 'Enable SDR Town control in FUBAR settings to use Satcom.';
      return;
    }
    const res = await fetch('api/sdr-town/satcom-status', {cache:'no-store'});
    const data = await res.json();
    const s = (data && data.satcom) ? data.satcom : null;
    if (!data.ok || !s) {
      if (line) line.textContent = (data && data.error) ? data.error : 'Satcom status unavailable (need SDR Town 0.2.69+).';
      return;
    }
    const cfg = s.config || {};
    const obs = s.observer || {};
    const focused = document.activeElement && String(document.activeElement.id||'').indexOf('sat') === 0;
    if (!focused) {
      if (cfg.lowMHz) document.getElementById('satLow').value = Number(cfg.lowMHz).toFixed(3);
      if (cfg.highMHz) document.getElementById('satHigh').value = Number(cfg.highMHz).toFixed(3);
      if (cfg.bandwidthHz) document.getElementById('satBw').value = Number(cfg.bandwidthHz / 1000).toFixed(1);
      if (cfg.mode) document.getElementById('satMode').value = cfg.mode;
      if (cfg.squelchDb != null) document.getElementById('satSquelch').value = Number(cfg.squelchDb).toFixed(1);
      if (obs.latDeg != null) document.getElementById('satLat').value = Number(obs.latDeg).toFixed(5);
      if (obs.lonDeg != null) document.getElementById('satLon').value = Number(obs.lonDeg).toFixed(5);
      if (obs.altM != null) document.getElementById('satAlt').value = Number(obs.altM).toFixed(0);
      if (obs.minElevationDeg != null) document.getElementById('satMinEl').value = Number(obs.minElevationDeg).toFixed(0);
    }
    if (s.spectrumDb) satDrawSpectrum(s.spectrumDb);
    const mhz = s.tunedMHz || s.lockMHz || s.currentMHz || 0;
    if (line) {
      line.textContent = (s.deviceConnected ? ('● ' + (s.deviceLabel || 'SDR')) : '○ no device') +
        ' · ' + (s.state || 'idle') +
        (s.passArmed ? ' · PASS' : '') +
        (mhz ? (' · ' + Number(mhz).toFixed(4) + ' MHz') : '') +
        (s.dopplerHz ? (' · doppler ' + Number(s.dopplerHz).toFixed(0) + ' Hz') : '') +
        (s.lastStatus ? (' · ' + s.lastStatus) : '');
    }
    if (logEl && Array.isArray(s.recentDecodes)) {
      logEl.textContent = s.recentDecodes.slice(-40).join('\n');
    }
    if (tleEl) {
      const age = s.tleAgeSec;
      tleEl.textContent = (age == null || age < 0) ? 'TLE: none — Refresh' :
        (age < 3600 ? ('TLE age: ' + Math.floor(age/60) + 'm') : ('TLE age: ' + (age/3600).toFixed(1) + 'h'));
    }
    const passes = Array.isArray(s.passes) ? s.passes : [];
    if (passEl) {
      if (!passes.length) passEl.textContent = 'No upcoming passes (set location, select sats, refresh TLE).';
      else passEl.textContent = passes.slice(0, 24).map(p => {
        const aos = new Date((p.aosUnix||0)*1000);
        return aos.toLocaleString() + '  ' + (p.satName||p.satId) + '  ' + (p.downlinkLabel||'') +
          '  maxEl ' + Number(p.maxElDeg||0).toFixed(1) + '°  ' + Number(p.freqMHz||0).toFixed(4) + ' MHz';
      }).join('\n');
    }
    const armed = s.armed || {};
    if (armedEl) {
      if (armed.armed) {
        armedEl.textContent = 'Armed ' + (armed.downlinkId||'') +
          (armed.role === 'sstv' ? ' (ISS SSTV — open SSTV in SDR Town)' : '') +
          ' · el ' + Number(armed.elevationDeg||0).toFixed(1) + '° · tuned ' + Number(armed.tunedMHz||0).toFixed(4) + ' MHz';
      } else armedEl.textContent = 'No pass armed';
    }
    const catBox = document.getElementById('satCatChecks');
    const sats = (s.catalogue && Array.isArray(s.catalogue.satellites)) ? s.catalogue.satellites :
      ((s.satellites) ? s.satellites : null);
    if (catBox && Array.isArray(sats) && !catBox.dataset.built) {
      catBox.innerHTML = sats.map(sat => {
        const id = sat.id;
        const checked = sat.selected ? 'checked' : '';
        return '<label style="margin-right:10px"><input type="checkbox" data-sat-id="'+id+'" '+checked+'> '+
          (sat.name||id)+'</label>';
      }).join('');
      catBox.dataset.built = '1';
    }
    const can = !!(sdrControlSession && sdrControlSession.canControl && sdrTownConfig.allowTune);
    ['satStartBtn','satSkipBtn','satRecordBtn','satStopBtn','satApplyObsBtn','satRefreshTleBtn',
     'satArmSstvBtn','satDisarmBtn','satSaveCatBtn'].forEach(id => {
      const el = document.getElementById(id);
      if (el) el.disabled = !can;
    });
  } catch (e) {
    if (line) line.textContent = 'Satcom poll failed';
  }
}
async function postSatcom(action, extra){
  if (!sdrControlSession || !sdrControlSession.canControl) {
    document.getElementById('satStatusLine').textContent = 'Take control on Radio control first.';
    return;
  }
  const payload = Object.assign({
    action: action,
    clientId: sdrControlClientId,
    lowMHz: Number(document.getElementById('satLow').value),
    highMHz: Number(document.getElementById('satHigh').value),
    bandwidthKHz: Number(document.getElementById('satBw').value),
    mode: document.getElementById('satMode').value,
    squelchDb: Number(document.getElementById('satSquelch').value)
  }, extra || {});
  await postSdrTown('api/sdr-town/satcom-control', payload);
  await loadSatcom();
}
async function postSatcomPath(path, body){
  if (!sdrControlSession || !sdrControlSession.canControl) {
    document.getElementById('satStatusLine').textContent = 'Take control on Radio control first.';
    return;
  }
  await postSdrTown(path, Object.assign({clientId: sdrControlClientId}, body || {}));
  const cat = document.getElementById('satCatChecks');
  if (cat) delete cat.dataset.built;
  await loadSatcom();
}
document.getElementById('satStartBtn').addEventListener('click', () => postSatcom('start'));
document.getElementById('satStopBtn').addEventListener('click', () => postSatcom('stop'));
document.getElementById('satSkipBtn').addEventListener('click', () => postSatcom('skip'));
document.getElementById('satRecordBtn').addEventListener('click', () => postSatcom('record'));
document.getElementById('satApplyObsBtn').addEventListener('click', () => postSatcomPath('api/sdr-town/satcom-observer', {
  lat: document.getElementById('satLat').value,
  lon: document.getElementById('satLon').value,
  altM: Number(document.getElementById('satAlt').value),
  minElevationDeg: Number(document.getElementById('satMinEl').value)
}));
document.getElementById('satRefreshTleBtn').addEventListener('click', () => postSatcomPath('api/sdr-town/satcom-tle-refresh', {}));
document.getElementById('satArmSstvBtn').addEventListener('click', () => postSatcomPath('api/sdr-town/satcom-arm', {
  satId: 'iss', downlinkId: 'iss-sstv', autoTrack: !!document.getElementById('satAutoTrack').checked
}));
document.getElementById('satDisarmBtn').addEventListener('click', () => postSatcomPath('api/sdr-town/satcom-arm', {action:'disarm'}));
document.getElementById('satSaveCatBtn').addEventListener('click', () => {
  const selected = Array.from(document.querySelectorAll('#satCatChecks input[data-sat-id]:checked')).map(el => el.getAttribute('data-sat-id'));
  postSatcomPath('api/sdr-town/satcom-catalogue', {selected});
});
document.getElementById('satAutoTrack').addEventListener('change', (e) => {
  postSatcomPath('api/sdr-town/satcom-arm', {satId:'', downlinkId:'', autoTrack: !!e.target.checked, action: 'autotrack_only'});
});

let inmPlansCache = [];
async function loadInmarsatPlans(){
  const planSel = document.getElementById('inmPlan');
  if (!planSel) return;
  try {
    const res = await fetch('api/sdr-town/inmarsat-bandplans', {cache:'no-store'});
    const data = await res.json();
    inmPlansCache = data.plans || [];
    planSel.innerHTML = '';
    inmPlansCache.forEach(p => {
      const o = document.createElement('option');
      o.value = p.id;
      o.textContent = (p.name||p.id) + (p.region ? (' — ' + p.region) : '');
      planSel.appendChild(o);
    });
    fillInmarsatChannels();
  } catch (e) {
    planSel.innerHTML = '<option value="">Plans unavailable (need SDR Town 0.2.71+)</option>';
  }
}
function fillInmarsatChannels(){
  const planSel = document.getElementById('inmPlan');
  const chSel = document.getElementById('inmChannel');
  if (!planSel || !chSel) return;
  const p = inmPlansCache.find(x => x.id === planSel.value);
  chSel.innerHTML = '';
  (p && p.channels || []).forEach(c => {
    const o = document.createElement('option');
    o.value = String(c.freqHz);
    o.dataset.mode = c.mode || 'aero_oqpsk';
    o.dataset.baud = String(c.baud || 10500);
    const mhz = (c.freqMHz != null) ? c.freqMHz : (c.freqHz/1e6);
    o.textContent = (c.label||'') + ' ' + Number(mhz).toFixed(3) + ' MHz';
    chSel.appendChild(o);
  });
}
async function loadInmarsat(){
  const st = document.getElementById('inmStatus');
  const log = document.getElementById('inmMsgLog');
  try {
    if (!sdrTownConfig || !sdrTownConfig.enabled) {
      if (st) st.textContent = 'Enable SDR Town control to use Inmarsat.';
      return;
    }
    const res = await fetch('api/sdr-town/inmarsat-status', {cache:'no-store'});
    const data = await res.json();
    const s = (data && data.inmarsat) ? data.inmarsat : null;
    if (!s) {
      if (st) st.textContent = (data && data.error) || 'Inmarsat status unavailable (need SDR Town 0.2.71+).';
      return;
    }
    if (st) {
      st.textContent = (s.state||'?') +
        (s.locked ? ' LOCK' : '') +
        ' · ' + (s.tunedMHz != null ? Number(s.tunedMHz).toFixed(3) : '?') + ' MHz' +
        ' · msgs ' + (s.messages||0) +
        ' · voice ' + (s.voiceFrames||0) +
        (s.followingVoice ? ' · VOICE FOLLOW' : '') +
        (s.lastStatus ? (' · ' + s.lastStatus) : '');
    }
    const vf = document.getElementById('inmVoiceFollow');
    const rec = document.getElementById('inmRecord');
    if (vf && s.config) vf.checked = !!s.config.voiceFollow;
    if (rec && s.config) rec.checked = !!s.config.recordVoice;
    const msgRes = await fetch('api/sdr-town/inmarsat-messages', {cache:'no-store'});
    const msgData = await msgRes.json();
    if (log && msgData && msgData.messages) {
      log.textContent = (msgData.messages||[]).map(m =>
        '[' + (m.kind||'?') + '] ' + (m.label||'') + ' ' + String(m.text||'').slice(0,160)
      ).join('\\n');
    }
  } catch (e) {
    if (st) st.textContent = 'Inmarsat poll failed';
  }
}
async function postInmarsatControl(payload){
  await postSdrTown('api/sdr-town/inmarsat-control', payload);
  await loadInmarsat();
}
const inmPlanEl = document.getElementById('inmPlan');
if (inmPlanEl) inmPlanEl.addEventListener('change', fillInmarsatChannels);
const inmRefreshEl = document.getElementById('inmRefreshPlansBtn');
if (inmRefreshEl) inmRefreshEl.addEventListener('click', loadInmarsatPlans);
const inmStartEl = document.getElementById('inmStartBtn');
if (inmStartEl) inmStartEl.addEventListener('click', () => {
  const plan = document.getElementById('inmPlan').value;
  const ch = document.getElementById('inmChannel');
  const opt = ch.options[ch.selectedIndex];
  postInmarsatControl({
    action: 'start',
    bandPlanId: plan,
    channelHz: Number(ch.value||0),
    mode: opt ? (opt.dataset.mode||'aero_oqpsk') : 'aero_oqpsk',
    baud: opt ? Number(opt.dataset.baud||10500) : 10500,
    voiceFollow: !!document.getElementById('inmVoiceFollow').checked,
    recordVoice: !!document.getElementById('inmRecord').checked
  });
});
const inmStopEl = document.getElementById('inmStopBtn');
if (inmStopEl) inmStopEl.addEventListener('click', () => postInmarsatControl({action:'stop'}));
const inmVfEl = document.getElementById('inmVoiceFollow');
if (inmVfEl) inmVfEl.addEventListener('change', (e) => postInmarsatControl({voiceFollow: !!e.target.checked}));
const inmRecEl = document.getElementById('inmRecord');
if (inmRecEl) inmRecEl.addEventListener('change', (e) => postInmarsatControl({recordVoice: !!e.target.checked}));
loadInmarsatPlans();

let acMap=null, acLayer=null, acMarkers={};
function acEnsureMap(){
  if (acMap || typeof L === 'undefined') return;
  const el = document.getElementById('acMap');
  if (!el) return;
  acMap = L.map(el).setView([-33.87, 151.21], 9);
  L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 18, attribution: '&copy; OpenStreetMap'
  }).addTo(acMap);
  acLayer = L.layerGroup().addTo(acMap);
}
async function loadAircraft(){
  const st = document.getElementById('acStatus');
  try {
    if (!sdrTownConfig || !sdrTownConfig.enabled) {
      if (st) st.textContent = 'Enable SDR Town control to show aircraft tracks.';
      return;
    }
    acEnsureMap();
    const res = await fetch('api/sdr-town/aircraft-status', {cache:'no-store'});
    const data = await res.json();
    if (!data.ok) {
      if (st) st.textContent = data.error || 'Aircraft status unavailable (need SDR Town 0.2.70+).';
      return;
    }
    if (st) {
      st.textContent = 'Tracks ' + ((data.tracks&&data.tracks.length)||0) +
        ' · net ' + (data.networkOnline ? ('OK age ' + data.networkAgeSec + 's') : 'offline') +
        ' · local CRC ' + (data.localCrcOk||0) +
        (data.lastStatus ? (' · ' + data.lastStatus) : '');
    }
    if (acMap && data.centerLat != null) {
      if (!acMap._acCentered) {
        acMap.setView([data.centerLat, data.centerLon], 9);
        acMap._acCentered = true;
      }
    }
    if (acLayer) {
      acLayer.clearLayers();
      acMarkers = {};
      (data.tracks||[]).forEach(t => {
        if (t.lat == null || t.lon == null) return;
        const color = t.fromAdsc ? '#ffb428' : (t.fromLocal ? '#39FF14' : '#50a0ff');
        const m = L.circleMarker([t.lat, t.lon], {
          radius: 6,
          color: color,
          fillColor: color,
          fillOpacity: 0.85
        }).bindTooltip((t.callsign||t.icao||'?') + ' ' + Math.round(t.altFt||0) + 'ft');
        m.on('click', () => {
          const box = document.getElementById('acPopout');
          const txt = document.getElementById('acPopText');
          const img = document.getElementById('acPopImg');
          if (box) box.style.display = 'block';
          if (txt) txt.innerHTML = '<b>' + (t.callsign||'(no callsign)') + '</b><br>ICAO ' + (t.icao||'') +
            '<br>Alt ' + Math.round(t.altFt||0) + ' ft · ' + Math.round(t.gsKt||0) + ' kt · track ' + Math.round(t.trackDeg||0) + '°' +
            '<br>Squawk ' + (t.squawk||'—') +
            (t.route ? ('<br>Route ' + t.route) : '') +
            '<br>Source: ' + (t.fromLocal?'local ADS-B ':'') + (t.fromNetwork?'OpenSky ':'') + (t.fromAdsc?'ADS-C':'');
          if (img) {
            if (t.photoUrl) {
              img.style.display = 'block';
              img.onerror = () => { img.style.display = 'none'; };
              img.src = t.photoUrl;
            } else img.style.display = 'none';
          }
        });
        m.addTo(acLayer);
        acMarkers[t.icao] = m;
      });
    }
  } catch (e) {
    if (st) st.textContent = 'Aircraft poll failed';
  }
}
document.getElementById('acRefreshBtn').addEventListener('click', async () => {
  if (!sdrControlSession || !sdrControlSession.canControl) {
    await loadAircraft();
    return;
  }
  await postSdrTown('api/sdr-town/aircraft-refresh', {clientId: sdrControlClientId});
  await loadAircraft();
});
document.getElementById('acTuneBtn').addEventListener('click', async () => {
  if (!sdrControlSession || !sdrControlSession.canControl) {
    document.getElementById('acStatus').textContent = 'Take control to tune 1090.';
    return;
  }
  await postSdrTown('api/sdr-town/tune', {clientId: sdrControlClientId, frequencyMHz: 1090, mode: 'NFM'});
});
document.querySelectorAll('#siteTabs button').forEach(btn => {
  btn.addEventListener('click', () => {
    if (btn.getAttribute('data-tab') === 'aircraft') setTimeout(() => { acEnsureMap(); if (acMap) acMap.invalidateSize(); loadAircraft(); }, 50);
    if (btn.getAttribute('data-tab') === 'inmarsat') setTimeout(() => { loadInmarsatPlans(); loadInmarsat(); }, 50);
  });
});

async function refresh(){
  try {
    const [statusRes, listRes] = await Promise.all([
      fetch('api/status', {cache:'no-store'}),
      fetch('api/captures', {cache:'no-store'})
    ]);
    const status = await statusRes.json();
    items = (await listRes.json()).captures || [];
    const air = status.recording ? 'LIVE · recording' : (status.live ? 'On air · live stream ready' : (status.status || 'On air'));
    live.textContent = air + ' · ' + queueLabel(status);
    dot.className = 'dot ' + (status.recording ? 'live' : 'on');
    showNowPlaying(status.nowPlaying);
    showP25Status(status.p25Status);
    render();
  } catch {
    live.textContent = 'Website unreachable';
    dot.className = 'dot';
  }
}
refresh();
loadSdrTownControl();
loadSatcom();
loadAircraft();
loadStations();
setInterval(refresh, 2000);
setInterval(loadSdrTownControl, 2000);
setInterval(loadSatcom, 1000);
setInterval(loadInmarsat, 2000);
setInterval(loadAircraft, 3000);
setInterval(sdrRenderControlSession, 1000);
setInterval(loadStations, 15000);
</script>
</body>
</html>
)HTML";

std::string jsonEscape(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (unsigned char ch : value) {
    switch (ch) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      default:
        if (ch < 32) {
          std::ostringstream hex;
          hex << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch);
          out += hex.str();
        } else {
          out += static_cast<char>(ch);
        }
    }
  }
  return out;
}

std::string wideToUtf8(const std::wstring& value) {
  if (value.empty()) return {};
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                        nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<std::size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), bytes,
                      nullptr, nullptr);
  return out;
}

std::wstring utf8ToWide(const std::string& value) {
  if (value.empty()) return {};
  const int chars = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                        nullptr, 0);
  std::wstring out(static_cast<std::size_t>(chars), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), chars);
  return out;
}

std::string urlDecode(const std::string& value) {
  std::string out;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()) {
      const std::string hex = value.substr(i + 1, 2);
      out += static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
      i += 2;
    } else if (value[i] == '+') {
      out += ' ';
    } else {
      out += value[i];
    }
  }
  return out;
}

std::string modeFromName(const std::string& name) {
  if (name.find("_left") != std::string::npos) return "Left";
  if (name.find("_right") != std::string::npos) return "Right";
  if (name.find("_mono") != std::string::npos) return "Mono";
  if (name.find("_stereo") != std::string::npos) return "Stereo";
  return "Capture";
}

std::string rfc3339Local(const std::filesystem::file_time_type& fileTime) {
  const auto system = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
  const std::time_t time = std::chrono::system_clock::to_time_t(system);
  std::tm local{};
  localtime_s(&local, &time);
  std::ostringstream stream;
  stream << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
  return stream.str();
}

bool sendAll(SOCKET socket, const char* data, int length) {
  int sent = 0;
  while (sent < length) {
    const int chunk = send(socket, data + sent, length - sent, 0);
    if (chunk <= 0) return false;
    sent += chunk;
  }
  return true;
}

constexpr const char* kCors =
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n"
    "Access-Control-Max-Age: 600\r\n";

constexpr std::uint64_t kSdrControlLeaseMs = 5ull * 60ull * 1000ull;
constexpr std::uint64_t kSdrControlExtendMs = 3ull * 60ull * 1000ull;
constexpr std::uint64_t kSdrControlWarningMs = 60ull * 1000ull;
constexpr std::uint64_t kSdrControlQueueStaleMs = 90ull * 1000ull;
constexpr int kSdrControlMaxExtensions = 2;

std::string sanitizeSdrControlId(const std::string& value) {
  std::string out;
  out.reserve(std::min<std::size_t>(value.size(), 80));
  for (unsigned char ch : value) {
    if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.') {
      out += static_cast<char>(ch);
    }
    if (out.size() >= 80) break;
  }
  return out;
}

std::string sanitizeSdrControlName(const std::string& value, const std::string& clientId) {
  std::string out;
  bool pendingSpace = false;
  for (unsigned char ch : value) {
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
      pendingSpace = true;
      continue;
    }
    if (ch >= 32 && ch < 127) {
      if (pendingSpace && !out.empty()) out += ' ';
      pendingSpace = false;
      out += static_cast<char>(ch);
    }
    if (out.size() >= 48) break;
  }
  if (!out.empty()) return out;
  const std::string suffix =
      clientId.size() > 4 ? clientId.substr(clientId.size() - 4) : clientId;
  return suffix.empty() ? "Operator" : std::string("Operator ") + suffix;
}

std::uint64_t tickNow64() { return static_cast<std::uint64_t>(GetTickCount64()); }

std::string headerValue(const std::string& request, const char* name) {
  const std::string prefix = std::string("\r\n") + name + ":";
  auto pos = request.find(prefix);
  if (pos == std::string::npos) {
    const std::string first = std::string(name) + ":";
    if (request.rfind(first, 0) == 0) pos = 0;
    else return {};
  }
  pos += prefix.size();
  auto end = request.find("\r\n", pos);
  std::string value = request.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
  return value;
}

std::string peerIpv4(SOCKET socket) {
  sockaddr_in addr{};
  int len = sizeof(addr);
  if (getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &len) != 0) return {};
  char ip[INET_ADDRSTRLEN]{};
  inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
  return ip;
}

std::string observedPublicIp(SOCKET socket, const std::string& request) {
  std::string forwarded = headerValue(request, "X-Forwarded-For");
  if (forwarded.empty()) forwarded = headerValue(request, "X-Real-IP");
  if (!forwarded.empty()) {
    const auto comma = forwarded.find(',');
    if (comma != std::string::npos) forwarded.resize(comma);
    while (!forwarded.empty() && (forwarded.front() == ' ' || forwarded.front() == '\t')) {
      forwarded.erase(forwarded.begin());
    }
    return forwarded;
  }
  return peerIpv4(socket);
}

bool sendResponse(SOCKET socket, int status, const char* reason, const std::string& type,
                  const std::string& body, const std::string& extra = {}) {
  std::ostringstream header;
  header << "HTTP/1.1 " << status << " " << reason << "\r\n"
         << "Content-Type: " << type << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
         << "Pragma: no-cache\r\n"
         << "Connection: close\r\n";
  if (!extra.empty()) header << extra;
  header << "\r\n";
  const std::string head = header.str();
  return sendAll(socket, head.data(), static_cast<int>(head.size())) &&
         (body.empty() || sendAll(socket, body.data(), static_cast<int>(body.size())));
}

bool sendFile(SOCKET socket, const std::filesystem::path& path, const std::string& rangeHeader) {
  std::error_code error;
  const auto size = static_cast<std::uint64_t>(std::filesystem::file_size(path, error));
  if (error || size == 0) return sendResponse(socket, 404, "Not Found", "text/plain", "missing");
  std::uint64_t start = 0;
  std::uint64_t end = size - 1;
  bool partial = false;
  if (rangeHeader.rfind("bytes=", 0) == 0) {
    const std::string spec = rangeHeader.substr(6);
    const auto dash = spec.find('-');
    if (dash != std::string::npos) {
      if (dash > 0) start = std::strtoull(spec.c_str(), nullptr, 10);
      if (dash + 1 < spec.size()) {
        const auto parsedEnd = std::strtoull(spec.c_str() + dash + 1, nullptr, 10);
        if (parsedEnd > 0) end = parsedEnd;
      }
      if (end >= size) end = size - 1;
      if (start > end) start = 0;
      partial = start > 0 || end + 1 < size;
    }
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) return sendResponse(socket, 404, "Not Found", "text/plain", "missing");
  file.seekg(static_cast<std::streamoff>(start));
  const std::uint64_t length = end - start + 1;
  std::ostringstream header;
  header << "HTTP/1.1 " << (partial ? "206 Partial Content" : "200 OK") << "\r\n"
         << "Content-Type: audio/wav\r\n"
         << "Accept-Ranges: bytes\r\n"
         << "Content-Length: " << length << "\r\n"
         << "Content-Range: bytes " << start << "-" << end << "/" << size << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Connection: close\r\n\r\n";
  const std::string head = header.str();
  if (!sendAll(socket, head.data(), static_cast<int>(head.size()))) return false;
  std::vector<char> buffer(64 * 1024);
  std::uint64_t remaining = length;
  while (remaining > 0) {
    const auto want = static_cast<std::streamsize>(std::min<std::uint64_t>(buffer.size(), remaining));
    file.read(buffer.data(), want);
    const auto got = file.gcount();
    if (got <= 0) break;
    if (!sendAll(socket, buffer.data(), static_cast<int>(got))) return false;
    remaining -= static_cast<std::uint64_t>(got);
  }
  return true;
}

std::string lanIpv4() {
  char host[256]{};
  if (gethostname(host, sizeof(host)) != 0) return "127.0.0.1";
  addrinfo hints{};
  hints.ai_family = AF_INET;
  addrinfo* result = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &result) != 0 || !result) return "127.0.0.1";
  char ip[INET_ADDRSTRLEN]{};
  inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr, ip, sizeof(ip));
  freeaddrinfo(result);
  if (std::strcmp(ip, "127.0.0.1") == 0) return "127.0.0.1";
  return ip;
}

}  // namespace

CaptureWebServer::CaptureWebServer() {
  InitializeCriticalSection(&lock_);
  WSADATA data{};
  WSAStartup(MAKEWORD(2, 2), &data);
}

CaptureWebServer::~CaptureWebServer() {
  stop();
  DeleteCriticalSection(&lock_);
}

void CaptureWebServer::setRoot(const std::filesystem::path& directory) {
  EnterCriticalSection(&lock_);
  root_ = directory;
  LeaveCriticalSection(&lock_);
}

void CaptureWebServer::setLiveHub(LiveAudioHub* hub) {
  EnterCriticalSection(&lock_);
  liveHub_ = hub;
  LeaveCriticalSection(&lock_);
}

void CaptureWebServer::setLiveStatus(const std::wstring& status, bool recording) {
  EnterCriticalSection(&lock_);
  liveStatus_ = status;
  recording_ = recording;
  LeaveCriticalSection(&lock_);
}

void CaptureWebServer::setNowPlaying(const std::string& text) {
  EnterCriticalSection(&lock_);
  nowPlaying_ = FubarNetDirectory::sanitizeNowPlaying(text);
  LeaveCriticalSection(&lock_);
}

std::string CaptureWebServer::nowPlaying() const {
  EnterCriticalSection(&lock_);
  const std::string copy = nowPlaying_;
  LeaveCriticalSection(&lock_);
  return copy;
}

void CaptureWebServer::setP25Status(const std::string& text) {
  EnterCriticalSection(&lock_);
  p25Status_ = FubarNetDirectory::sanitizeNowPlaying(text);
  LeaveCriticalSection(&lock_);
}

std::string CaptureWebServer::p25Status() const {
  EnterCriticalSection(&lock_);
  const std::string copy = p25Status_;
  LeaveCriticalSection(&lock_);
  return copy;
}

void CaptureWebServer::setMaxLiveListeners(int limit) { liveSlots_.setLimit(limit); }
void CaptureWebServer::setSdrTownControlConfig(const SdrTownBridgeConfig& config) {
  EnterCriticalSection(&lock_);
  sdrTownControl_ = config;
  LeaveCriticalSection(&lock_);
}
int CaptureWebServer::maxLiveListeners() const { return liveSlots_.limit(); }
int CaptureWebServer::liveListeners() const { return liveSlots_.active(); }
int CaptureWebServer::liveQueued() const { return liveSlots_.queued(); }

bool CaptureWebServer::publishStation(const FubarNetStation& station, std::string* error) {
  return directory_.upsert(station, "127.0.0.1", error);
}

void CaptureWebServer::unpublishStation(const std::string& id) { directory_.leave(id); }

std::string CaptureWebServer::directoryJson() const { return directory_.listJson(); }

bool CaptureWebServer::running() const { return running_; }
std::uint16_t CaptureWebServer::port() const { return port_; }

std::wstring CaptureWebServer::lastError() const {
  EnterCriticalSection(&lock_);
  std::wstring value = lastError_;
  LeaveCriticalSection(&lock_);
  return value;
}

std::wstring CaptureWebServer::url() const {
  return L"http://127.0.0.1:" + std::to_wstring(port_) + L"/";
}

std::wstring CaptureWebServer::lanUrl() const {
  return L"http://" + utf8ToWide(lanIpv4()) + L":" + std::to_wstring(port_) + L"/";
}

std::filesystem::path CaptureWebServer::rootLocked() const {
  EnterCriticalSection(&lock_);
  std::filesystem::path value = root_;
  LeaveCriticalSection(&lock_);
  return value;
}

SdrTownBridgeConfig CaptureWebServer::sdrTownControlConfigLocked() const {
  EnterCriticalSection(&lock_);
  SdrTownBridgeConfig value = sdrTownControl_;
  LeaveCriticalSection(&lock_);
  return value;
}

void CaptureWebServer::sdrTownControlPromoteLocked(std::uint64_t nowTick) {
  if (!sdrControlActiveClient_.empty() && nowTick < sdrControlLeaseUntilTick_) return;
  if (!sdrControlActiveClient_.empty()) {
    sdrControlActiveClient_.clear();
    sdrControlActiveName_.clear();
    sdrControlLeaseUntilTick_ = 0;
    sdrControlExtensionsUsed_ = 0;
    ++sdrControlRevision_;
  }

  while (sdrControlActiveClient_.empty() && !sdrControlQueue_.empty()) {
    SdrControlQueueEntry next = sdrControlQueue_.front();
    sdrControlQueue_.pop_front();
    if (next.clientId.empty()) continue;
    if (nowTick > next.lastSeenTick && nowTick - next.lastSeenTick > kSdrControlQueueStaleMs) {
      continue;
    }
    sdrControlActiveClient_ = next.clientId;
    sdrControlActiveName_ = next.name;
    sdrControlLeaseUntilTick_ = nowTick + kSdrControlLeaseMs;
    sdrControlExtensionsUsed_ = 0;
    ++sdrControlRevision_;
  }
}

std::string CaptureWebServer::sdrTownControlSessionStateJsonLocked(
    const std::string& clientId, std::uint64_t nowTick) const {
  int queuePosition = 0;
  for (std::size_t i = 0; i < sdrControlQueue_.size(); ++i) {
    if (sdrControlQueue_[i].clientId == clientId) {
      queuePosition = static_cast<int>(i + 1);
      break;
    }
  }
  const bool active = !sdrControlActiveClient_.empty();
  const bool canControl = active && !clientId.empty() && sdrControlActiveClient_ == clientId &&
                          nowTick < sdrControlLeaseUntilTick_;
  const std::uint64_t remaining =
      active && sdrControlLeaseUntilTick_ > nowTick ? sdrControlLeaseUntilTick_ - nowTick : 0;
  const char* role = canControl ? "active" : (queuePosition > 0 ? "queued" : (active ? "waiting" : "idle"));

  std::ostringstream json;
  json << "{\"role\":\"" << role << "\",\"canControl\":" << (canControl ? "true" : "false")
       << ",\"active\":" << (active ? "true" : "false")
       << ",\"activeName\":\"" << jsonEscape(sdrControlActiveName_) << "\""
       << ",\"remainingMs\":" << remaining
       << ",\"queuePosition\":" << queuePosition
       << ",\"queueLength\":" << sdrControlQueue_.size()
       << ",\"extensionsUsed\":" << sdrControlExtensionsUsed_
       << ",\"maxExtensions\":" << kSdrControlMaxExtensions
       << ",\"leaseMs\":" << kSdrControlLeaseMs
       << ",\"extendMs\":" << kSdrControlExtendMs
       << ",\"warningMs\":" << kSdrControlWarningMs
       << ",\"revision\":" << sdrControlRevision_
       << ",\"queue\":[";
  for (std::size_t i = 0; i < sdrControlQueue_.size(); ++i) {
    if (i) json << ",";
    json << "{\"position\":" << (i + 1) << ",\"name\":\""
         << jsonEscape(sdrControlQueue_[i].name) << "\"}";
  }
  json << "]}";
  return json.str();
}

std::string CaptureWebServer::sdrTownControlSessionActionJson(const std::string& body) {
  const std::uint64_t now = tickNow64();
  std::string action = FubarNetDirectory::jsonGetString(body, "action");
  if (action.empty()) action = "status";
  std::transform(action.begin(), action.end(), action.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  const std::string clientId = sanitizeSdrControlId(FubarNetDirectory::jsonGetString(body, "clientId"));
  const std::string name =
      sanitizeSdrControlName(FubarNetDirectory::jsonGetString(body, "name"), clientId);

  bool ok = true;
  std::string error;
  std::string state;
  EnterCriticalSection(&lock_);
  sdrTownControlPromoteLocked(now);
  if (!sdrTownControl_.enabled) {
    ok = false;
    error = "SDR Town control is disabled in FUBAR.";
  } else if (clientId.empty()) {
    ok = false;
    error = "Missing control client id.";
  } else {
    if (sdrControlActiveClient_ == clientId) sdrControlActiveName_ = name;
    for (auto& queued : sdrControlQueue_) {
      if (queued.clientId == clientId) {
        queued.name = name;
        queued.lastSeenTick = now;
      }
    }

    if (action == "take") {
      if (sdrControlActiveClient_.empty()) {
        sdrControlActiveClient_ = clientId;
        sdrControlActiveName_ = name;
        sdrControlLeaseUntilTick_ = now + kSdrControlLeaseMs;
        sdrControlExtensionsUsed_ = 0;
        sdrControlQueue_.erase(
            std::remove_if(sdrControlQueue_.begin(), sdrControlQueue_.end(),
                           [&](const SdrControlQueueEntry& entry) {
                             return entry.clientId == clientId;
                           }),
            sdrControlQueue_.end());
        ++sdrControlRevision_;
      } else if (sdrControlActiveClient_ != clientId) {
        const auto found = std::find_if(
            sdrControlQueue_.begin(), sdrControlQueue_.end(),
            [&](const SdrControlQueueEntry& entry) { return entry.clientId == clientId; });
        if (found == sdrControlQueue_.end()) {
          sdrControlQueue_.push_back({clientId, name, now, now});
          ++sdrControlRevision_;
        }
      }
    } else if (action == "extend") {
      if (sdrControlActiveClient_ != clientId) {
        ok = false;
        error = "Only the active controller can extend this SDR Town session.";
      } else if (sdrControlExtensionsUsed_ >= kSdrControlMaxExtensions) {
        ok = false;
        error = "This control session has already used both extensions.";
      } else {
        const std::uint64_t base = std::max(now, sdrControlLeaseUntilTick_);
        sdrControlLeaseUntilTick_ = base + kSdrControlExtendMs;
        ++sdrControlExtensionsUsed_;
        ++sdrControlRevision_;
      }
    } else if (action == "release") {
      if (sdrControlActiveClient_ == clientId) {
        sdrControlActiveClient_.clear();
        sdrControlActiveName_.clear();
        sdrControlLeaseUntilTick_ = 0;
        sdrControlExtensionsUsed_ = 0;
        ++sdrControlRevision_;
        sdrTownControlPromoteLocked(now);
      } else {
        const auto oldSize = sdrControlQueue_.size();
        sdrControlQueue_.erase(
            std::remove_if(sdrControlQueue_.begin(), sdrControlQueue_.end(),
                           [&](const SdrControlQueueEntry& entry) {
                             return entry.clientId == clientId;
                           }),
            sdrControlQueue_.end());
        if (sdrControlQueue_.size() != oldSize) ++sdrControlRevision_;
      }
    } else if (action != "status") {
      ok = false;
      error = "Unknown SDR Town control-session action.";
    }
  }
  state = sdrTownControlSessionStateJsonLocked(clientId, now);
  LeaveCriticalSection(&lock_);

  std::ostringstream json;
  json << "{\"ok\":" << (ok ? "true" : "false");
  if (!error.empty()) json << ",\"error\":\"" << jsonEscape(error) << "\"";
  json << ",\"session\":" << state << "}";
  return json.str();
}

bool CaptureWebServer::sdrTownControlCommandAllowed(const std::string& body,
                                                    std::string* response) {
  const std::uint64_t now = tickNow64();
  const std::string clientId = sanitizeSdrControlId(FubarNetDirectory::jsonGetString(body, "clientId"));
  std::string state;
  std::string error;
  bool allowed = false;
  EnterCriticalSection(&lock_);
  sdrTownControlPromoteLocked(now);
  if (!sdrTownControl_.enabled) {
    error = "SDR Town control is disabled in FUBAR.";
  } else if (clientId.empty()) {
    error = "Take control before sending SDR Town commands.";
  } else if (sdrControlActiveClient_ == clientId && now < sdrControlLeaseUntilTick_) {
    allowed = true;
  } else {
    error = "SDR Town control is locked by another operator. Click Take control to join the queue.";
  }
  if (!allowed) state = sdrTownControlSessionStateJsonLocked(clientId, now);
  LeaveCriticalSection(&lock_);

  if (!allowed && response) {
    *response = std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) +
                "\",\"session\":" + state + "}";
  }
  return allowed;
}

bool CaptureWebServer::safeCaptureId(const std::string& id) {
  if (id.empty() || id.size() > 180) return false;
  if (id.find("..") != std::string::npos || id.find('/') != std::string::npos ||
      id.find('\\') != std::string::npos) {
    return false;
  }
  if (id.size() < 4) return false;
  std::string lower = id;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (lower.rfind(".wav") != lower.size() - 4) return false;
  return std::all_of(id.begin(), id.end(), [](unsigned char ch) {
    return std::isalnum(ch) || ch == '.' || ch == '_' || ch == '-';
  });
}

double CaptureWebServer::wavDurationSeconds(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return 0.0;
  char header[44]{};
  file.read(header, 44);
  if (file.gcount() < 44 || std::memcmp(header, "RIFF", 4) != 0) return 0.0;
  const auto sampleRate = *reinterpret_cast<const std::uint32_t*>(header + 24);
  const auto byteRate = *reinterpret_cast<const std::uint32_t*>(header + 28);
  std::uint32_t dataBytes = *reinterpret_cast<const std::uint32_t*>(header + 40);
  if (std::memcmp(header + 36, "data", 4) != 0) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    dataBytes = size > 44 ? static_cast<std::uint32_t>(size - 44) : 0;
  }
  if (byteRate == 0 && sampleRate == 0) return 0.0;
  const double denom = byteRate ? static_cast<double>(byteRate) : static_cast<double>(sampleRate * 2);
  return denom > 0 ? static_cast<double>(dataBytes) / denom : 0.0;
}

std::vector<CaptureItem> CaptureWebServer::listCaptures(const std::filesystem::path& directory) {
  std::vector<CaptureItem> items;
  std::error_code error;
  if (!std::filesystem::exists(directory, error)) return items;
  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (error || !entry.is_regular_file()) continue;
    const auto name = entry.path().filename().string();
    if (!safeCaptureId(name)) continue;
    CaptureItem item;
    item.id = name;
    item.name = name;
    item.mode = modeFromName(name);
    item.bytes = entry.file_size(error);
    item.started = rfc3339Local(entry.last_write_time());
    item.durationSeconds = wavDurationSeconds(entry.path());
    items.push_back(std::move(item));
  }
  std::sort(items.begin(), items.end(),
            [](const CaptureItem& a, const CaptureItem& b) { return a.started > b.started; });
  return items;
}

bool CaptureWebServer::handlePathForTest(const std::string& method, const std::string& path,
                                         const std::filesystem::path& root, int* status,
                                         std::string* contentType) {
  if (path == "/fubar-net" || path == "/fubar-net/" || path == "/fubar-net/servers") {
    if (method == "GET" || method == "OPTIONS") {
      *status = 200;
      *contentType = "application/json";
      return true;
    }
    *status = 405;
    *contentType = "text/plain";
    return false;
  }
  if (path == "/fubar-net/announce" || path == "/fubar-net/leave") {
    if (method == "POST" || method == "OPTIONS") {
      *status = 200;
      *contentType = "application/json";
      return true;
    }
    *status = 405;
    *contentType = "text/plain";
    return false;
  }
  if (path == "/api/sdr-town/config" || path == "/api/sdr-town/status" ||
      path == "/api/sdr-town/satcom-status" ||
      path == "/api/sdr-town/aircraft-status" ||
      path == "/api/sdr-town/inmarsat-status" ||
      path == "/api/sdr-town/inmarsat-bandplans" ||
      path == "/api/sdr-town/inmarsat-messages" ||
      path == "/api/sdr-town/sstv-file") {
    if (method == "GET" || method == "OPTIONS") {
      *status = 200;
      *contentType = path == "/api/sdr-town/sstv-file" ? "image/png" : "application/json";
      return true;
    }
    *status = 405;
    *contentType = "application/json";
    return false;
  }
  if (path == "/api/sdr-town/control-session") {
    if (method == "POST" || method == "OPTIONS") {
      *status = 200;
      *contentType = "application/json";
      return true;
    }
    *status = 405;
    *contentType = "application/json";
    return false;
  }
  if (path == "/api/sdr-town/tune" || path == "/api/sdr-town/mode" ||
      path == "/api/sdr-town/rf-gain" ||
      path == "/api/sdr-town/volume" ||
      path == "/api/sdr-town/direct-sampling" ||
      path == "/api/sdr-town/sdrplay" ||
      path == "/api/sdr-town/satcom-control" ||
      path == "/api/sdr-town/satcom-observer" ||
      path == "/api/sdr-town/satcom-catalogue" ||
      path == "/api/sdr-town/satcom-arm" ||
      path == "/api/sdr-town/satcom-tle-refresh" ||
      path == "/api/sdr-town/aircraft-refresh" ||
      path == "/api/sdr-town/inmarsat-control" ||
      path == "/api/sdr-town/p25-control") {
    if (method == "POST" || method == "OPTIONS") {
      *status = 200;
      *contentType = "application/json";
      return true;
    }
    *status = 405;
    *contentType = "application/json";
    return false;
  }
  if (method != "GET") {
    *status = 405;
    *contentType = "text/plain";
    return false;
  }
  if (path == "/" || path == "/index.html") {
    *status = 200;
    *contentType = "text/html";
    return true;
  }
  if (path == "/live" || path == "/live.wav") {
    *status = 200;
    *contentType = "audio/wav";
    return true;
  }
  if (path == "/live.pcm") {
    *status = 200;
    *contentType = "application/octet-stream";
    return true;
  }
  if (path == "/live.mp3") {
    *status = 200;
    *contentType = "audio/mpeg";
    return true;
  }
  if (path == "/api/status" || path == "/api/captures") {
    *status = 200;
    *contentType = "application/json";
    return true;
  }
  if (path.rfind("/audio/", 0) == 0) {
    const auto id = urlDecode(path.substr(7));
    if (!safeCaptureId(id)) {
      *status = 400;
      *contentType = "text/plain";
      return false;
    }
    const auto full = (root / utf8ToWide(id)).lexically_normal();
    const auto allowed = root.lexically_normal();
    if (full.wstring().rfind(allowed.wstring(), 0) != 0) {
      *status = 403;
      *contentType = "text/plain";
      return false;
    }
    *status = 200;
    *contentType = "audio/wav";
    return true;
  }
  *status = 404;
  *contentType = "text/plain";
  return false;
}

std::string CaptureWebServer::statusJson() const {
  EnterCriticalSection(&lock_);
  const bool recording = recording_;
  const std::wstring live = liveStatus_;
  const std::string playing = nowPlaying_;
  const std::string p25 = p25Status_;
  const std::uint16_t port = port_;
  const SdrTownBridgeConfig sdrTown = sdrTownControl_;
  LeaveCriticalSection(&lock_);
  std::ostringstream json;
  LiveAudioHub* hub = liveHub_;
  json << "{\"ok\":true,\"recording\":" << (recording ? "true" : "false")
       << ",\"live\":" << (hub && hub->live() ? "true" : "false")
       << ",\"sampleRate\":" << (hub ? hub->sampleRate() : 0)
       << ",\"status\":\"" << jsonEscape(wideToUtf8(live)) << "\",\"port\":" << port
       << ",\"listeners\":" << liveSlots_.active()
       << ",\"listenerLimit\":" << liveSlots_.limit()
       << ",\"queued\":" << liveSlots_.queued()
       << ",\"nowPlaying\":\"" << jsonEscape(playing) << "\""
       << ",\"p25Status\":\"" << jsonEscape(p25) << "\""
       << ",\"sdrTownControl\":" << (sdrTown.enabled ? "true" : "false")
       << "}";
  return json.str();
}

std::string CaptureWebServer::sdrTownControlConfigJson() const {
  const auto cfg = sdrTownControlConfigLocked();
  std::ostringstream json;
  json << "{\"ok\":true,\"enabled\":" << (cfg.enabled ? "true" : "false")
       << ",\"allowTune\":" << (cfg.allowTune ? "true" : "false")
       << ",\"allowMode\":" << (cfg.allowMode ? "true" : "false")
       << ",\"allowRfGain\":" << (cfg.allowRfGain ? "true" : "false")
       << ",\"allowP25Control\":" << (cfg.allowP25Control ? "true" : "false")
       << ",\"leaseMs\":" << kSdrControlLeaseMs
       << ",\"extendMs\":" << kSdrControlExtendMs
       << ",\"warningMs\":" << kSdrControlWarningMs
       << ",\"maxExtensions\":" << kSdrControlMaxExtensions
       << ",\"port\":" << cfg.port << "}";
  return json.str();
}

std::string CaptureWebServer::capturesJson() const {
  const auto items = listCaptures(rootLocked());
  std::ostringstream json;
  json << "{\"ok\":true,\"captures\":[";
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i) json << ",";
    json << "{\"id\":\"" << jsonEscape(items[i].id) << "\",\"name\":\"" << jsonEscape(items[i].name)
         << "\",\"started\":\"" << jsonEscape(items[i].started) << "\",\"mode\":\""
         << jsonEscape(items[i].mode) << "\",\"durationSeconds\":" << std::fixed
         << std::setprecision(1) << items[i].durationSeconds << ",\"bytes\":" << items[i].bytes
         << "}";
  }
  json << "]}";
  return json.str();
}

bool CaptureWebServer::start(std::uint16_t port) {
  stop();
  port_ = port ? port : 80;
  const SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET) {
    lastError_ = L"Could not create socket";
    return false;
  }
  BOOL reuse = TRUE;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    lastError_ = L"Port " + std::to_wstring(port_) + L" is in use or blocked";
    closesocket(listener);
    return false;
  }
  if (::listen(listener, SOMAXCONN) != 0) {
    lastError_ = L"Could not listen on port " + std::to_wstring(port_);
    closesocket(listener);
    return false;
  }
  listen_ = static_cast<std::uintptr_t>(listener);
  stop_ = false;
  running_ = true;
  lastError_.clear();
  thread_ = CreateThread(nullptr, 0, acceptThreadEntry, this, 0, nullptr);
  if (!thread_) {
    lastError_ = L"Could not start website thread";
    stop();
    return false;
  }
  return true;
}

DWORD WINAPI CaptureWebServer::acceptThreadEntry(LPVOID context) {
  static_cast<CaptureWebServer*>(context)->acceptLoop();
  return 0;
}

struct ClientJob {
  CaptureWebServer* server;
  SOCKET client;
};

DWORD WINAPI CaptureWebServer::clientThreadEntry(LPVOID context) {
  ClientJob* job = static_cast<ClientJob*>(context);
  CaptureWebServer* server = job->server;
  SOCKET client = job->client;
  delete job;
  server->handleClient(static_cast<std::uintptr_t>(client));
  closesocket(client);
  return 0;
}

void CaptureWebServer::stop() {
  stop_ = true;
  running_ = false;
  liveSlots_.shutdown();
  if (listen_ != static_cast<std::uintptr_t>(-1)) {
    closesocket(static_cast<SOCKET>(listen_));
    listen_ = static_cast<std::uintptr_t>(-1);
  }
  if (thread_) {
    WaitForSingleObject(thread_, 4000);
    CloseHandle(thread_);
    thread_ = nullptr;
  }
}

void CaptureWebServer::acceptLoop() {
  while (!stop_) {
    sockaddr_in clientAddr{};
    int len = sizeof(clientAddr);
    SOCKET client = accept(static_cast<SOCKET>(listen_), reinterpret_cast<sockaddr*>(&clientAddr), &len);
    if (client == INVALID_SOCKET) {
      if (stop_) break;
      continue;
    }
    auto* work = new ClientJob{this, client};
    HANDLE worker = CreateThread(nullptr, 0, clientThreadEntry, work, 0, nullptr);
    if (worker) {
      CloseHandle(worker);
    } else {
      delete work;
      handleClient(static_cast<std::uintptr_t>(client));
      closesocket(client);
    }
  }
}

void CaptureWebServer::streamLive(std::uintptr_t clientHandle, bool wavContainer) {
  const SOCKET client = static_cast<SOCKET>(clientHandle);
  const BOOL nodelay = TRUE;
  setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
  DWORD sendTimeout = 120000;
  setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&sendTimeout),
             sizeof(sendTimeout));
  DWORD recvTimeout = 0;
  setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&recvTimeout),
             sizeof(recvTimeout));
  BOOL keepAlive = TRUE;
  setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepAlive),
             sizeof(keepAlive));
  tcp_keepalive ka{};
  ka.onoff = 1;
  ka.keepalivetime = 15000;
  ka.keepaliveinterval = 2000;
  DWORD bytesReturned = 0;
  WSAIoctl(client, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), nullptr, 0, &bytesReturned, nullptr,
           nullptr);

  struct SlotGuard {
    LiveSlotGate* gate = nullptr;
    bool held = false;
    ~SlotGuard() {
      if (held && gate) gate->release();
    }
  } slot;
  slot.gate = &liveSlots_;
  if (!liveSlots_.tryAcquire(INFINITE, &stop_, clientHandle)) {
    if (!stop_) {
      sendResponse(client, 503, "Service Unavailable", "text/plain", "live queue full");
    }
    return;
  }
  slot.held = true;

  EnterCriticalSection(&lock_);
  LiveAudioHub* hub = liveHub_;
  LeaveCriticalSection(&lock_);

  while (hub && !hub->live() && !stop_) Sleep(25);
  if (stop_) return;
  const std::uint32_t rate = (hub && hub->sampleRate()) ? hub->sampleRate() : 48000;
  const std::uint16_t streamChannels = (hub && hub->channels() == 2) ? 2 : 1;
  const char* prelude = wavContainer
      ? "HTTP/1.0 200 OK\r\nContent-Type: audio/wav\r\nCache-Control: no-store, no-cache, no-transform, must-revalidate\r\nPragma: no-cache\r\nX-Accel-Buffering: no\r\nAccept-Ranges: none\r\nConnection: close\r\nicy-name: FUBAR Live\r\n\r\n"
      : "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\nCache-Control: no-store, no-transform\r\nX-Accel-Buffering: no\r\nConnection: close\r\n\r\n";
  if (!sendAll(client, prelude, static_cast<int>(std::strlen(prelude)))) return;
  if (wavContainer) {
    std::uint8_t wav[44];
    LiveAudioHub::writeWavHeader(wav, rate, streamChannels);
    if (!sendAll(client, reinterpret_cast<const char*>(wav), 44)) return;
  } else {
    std::uint8_t pcmHeader[16];
    LiveAudioHub::writePcmHeader(pcmHeader, rate, streamChannels);
    if (!sendAll(client, reinterpret_cast<const char*>(pcmHeader), 16)) return;
  }

  LiveAudioHub::Cursor cursor;
  std::int16_t pcm[2048];
  std::uint32_t generation = hub ? hub->generation() : 0;
  while (!stop_) {
    if (!hub) {
      Sleep(25);
      continue;
    }
    const std::uint32_t nowRate = hub->sampleRate();
    if (nowRate && nowRate != rate) break;
    if (hub->channels() && hub->channels() != streamChannels) break;
    if (!hub->live()) {
      Sleep(25);
      continue;
    }
    if (hub->generation() != generation) {
      generation = hub->generation();
      cursor = {};
    }
    const std::size_t got = hub->pull(cursor, pcm, 2048, 20);
    if (got == 0) continue;
    if (!sendAll(client, reinterpret_cast<const char*>(pcm),
                 static_cast<int>(got * sizeof(std::int16_t)))) {
      break;
    }
  }
}

void CaptureWebServer::streamLiveMp3(std::uintptr_t clientHandle) {
  const SOCKET client = static_cast<SOCKET>(clientHandle);
  const BOOL nodelay = TRUE;
  setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
  DWORD sendTimeout = 120000;
  setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&sendTimeout),
             sizeof(sendTimeout));
  BOOL keepAlive = TRUE;
  setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepAlive),
             sizeof(keepAlive));
  tcp_keepalive ka{};
  ka.onoff = 1;
  ka.keepalivetime = 10000;
  ka.keepaliveinterval = 2000;
  DWORD bytesReturned = 0;
  WSAIoctl(client, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), nullptr, 0, &bytesReturned, nullptr,
           nullptr);

  struct SlotGuard {
    LiveSlotGate* gate = nullptr;
    bool held = false;
    ~SlotGuard() {
      if (held && gate) gate->release();
    }
  } slot;
  slot.gate = &liveSlots_;
  if (!liveSlots_.tryAcquire(INFINITE, &stop_, clientHandle)) {
    if (!stop_) {
      sendResponse(client, 503, "Service Unavailable", "text/plain", "live queue full");
    }
    return;
  }
  slot.held = true;

  EnterCriticalSection(&lock_);
  LiveAudioHub* hub = liveHub_;
  LeaveCriticalSection(&lock_);
  while (hub && !hub->live() && !stop_) Sleep(25);
  if (stop_) return;

  const std::uint32_t rate = (hub && hub->sampleRate()) ? hub->sampleRate() : 48000;
  const std::uint16_t streamChannels = (hub && hub->channels() == 2) ? 2 : 1;
  LiveMp3Encoder encoder;
  if (!encoder.open(rate, streamChannels)) return;
  const char prelude[] =
      "HTTP/1.0 200 OK\r\nContent-Type: audio/mpeg\r\n"
      "Cache-Control: no-store, no-cache, no-transform, must-revalidate\r\n"
      "Pragma: no-cache\r\nX-Accel-Buffering: no\r\nAccept-Ranges: none\r\n"
      "Connection: close\r\n"
      "icy-name: FUBAR Live\r\nicy-genre: Radio\r\nicy-pub: 1\r\nicy-br: 128\r\n\r\n";
  if (!sendAll(client, prelude, static_cast<int>(std::strlen(prelude)))) return;

  LiveAudioHub::Cursor cursor;
  std::vector<std::int16_t> pcm(4096);
  std::vector<std::uint8_t> mp3;
  std::uint32_t generation = hub ? hub->generation() : 0;
  int emptyPulls = 0;
  const std::size_t silenceFrames = static_cast<std::size_t>(encoder.samplesPerPass());
  std::vector<std::int16_t> silence(silenceFrames * streamChannels, 0);
  {
    std::vector<std::uint8_t> prime;
    encoder.encodeInterleaved(silence.data(), silence.size(), &prime);
    encoder.encodeInterleaved(silence.data(), silence.size(), &prime);
    if (!prime.empty() &&
        !sendAll(client, reinterpret_cast<const char*>(prime.data()),
                 static_cast<int>(prime.size()))) {
      return;
    }
  }
  while (!stop_) {
    if (!hub) {
      Sleep(20);
      continue;
    }
    if (hub->sampleRate() && hub->sampleRate() != rate) break;
    if (hub->channels() && hub->channels() != streamChannels) break;
    if (!hub->live()) {
      Sleep(20);
      continue;
    }
    if (hub->generation() != generation) {
      generation = hub->generation();
      cursor = {};
    }
    mp3.clear();
    const std::size_t got = hub->pull(cursor, pcm.data(), pcm.size(), 20);
    if (got > 0) {
      emptyPulls = 0;
      encoder.encodeInterleaved(pcm.data(), got, &mp3);
    } else if (++emptyPulls >= 20) {
      emptyPulls = 0;
      encoder.encodeInterleaved(silence.data(), silence.size(), &mp3);
    }
    if (!mp3.empty() &&
        !sendAll(client, reinterpret_cast<const char*>(mp3.data()), static_cast<int>(mp3.size()))) {
      break;
    }
  }
}

void CaptureWebServer::handleClient(std::uintptr_t clientHandle) {
  const SOCKET client = static_cast<SOCKET>(clientHandle);
  DWORD timeout = 8000;
  setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
  std::string request;
  char buffer[2048];
  while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16384) {
    const int got = recv(client, buffer, sizeof(buffer), 0);
    if (got <= 0) return;
    request.append(buffer, static_cast<std::size_t>(got));
  }
  const auto headerEnd = request.find("\r\n\r\n");
  if (headerEnd == std::string::npos) return;
  std::string body = request.substr(headerEnd + 4);
  const std::string lengthHeader = headerValue(request, "Content-Length");
  const std::size_t contentLength = lengthHeader.empty() ? 0 : static_cast<std::size_t>(std::strtoul(lengthHeader.c_str(), nullptr, 10));
  while (body.size() < contentLength && body.size() < 8192) {
    const int got = recv(client, buffer, sizeof(buffer), 0);
    if (got <= 0) break;
    body.append(buffer, static_cast<std::size_t>(got));
  }
  if (body.size() > contentLength) body.resize(contentLength);

  const auto lineEnd = request.find("\r\n");
  if (lineEnd == std::string::npos) return;
  std::istringstream line(request.substr(0, lineEnd));
  std::string method, path, version;
  line >> method >> path >> version;
  std::string queryString;
  auto query = path.find('?');
  if (query != std::string::npos) {
    queryString = path.substr(query + 1);
    path.resize(query);
  }
  path = urlDecode(path);

  std::string range;
  auto rangePos = request.find("Range:");
  if (rangePos != std::string::npos) {
    auto end = request.find("\r\n", rangePos);
    range = request.substr(rangePos + 6, end - rangePos - 6);
    while (!range.empty() && (range.front() == ' ' || range.front() == '\t')) range.erase(range.begin());
  }

  if (method == "OPTIONS" && path.rfind("/fubar-net", 0) == 0) {
    sendResponse(client, 204, "No Content", "text/plain", "", kCors);
    return;
  }
  if (method == "OPTIONS" && path.rfind("/api/sdr-town", 0) == 0) {
    sendResponse(client, 204, "No Content", "text/plain", "", kCors);
    return;
  }

  int status = 0;
  std::string type;
  if (!handlePathForTest(method, path, rootLocked(), &status, &type)) {
    const bool wantsJson = type.find("application/json") != std::string::npos;
    const std::string body = wantsJson
        ? std::string("{\"ok\":false,\"error\":\"HTTP ") + std::to_string(status ? status : 404) + "\"}"
        : std::string("error");
    sendResponse(client, status ? status : 404, "Error", type.empty() ? "text/plain" : type, body,
                 kCors);
    return;
  }
  if (path == "/" || path == "/index.html") {
    sendResponse(client, 200, "OK", "text/html; charset=utf-8", kPage);
    return;
  }
  if (path == "/api/status") {
    sendResponse(client, 200, "OK", "application/json", statusJson());
    return;
  }
  if (path == "/api/captures") {
    sendResponse(client, 200, "OK", "application/json", capturesJson());
    return;
  }
  if (path == "/api/sdr-town/config") {
    sendResponse(client, 200, "OK", "application/json", sdrTownControlConfigJson(), kCors);
    return;
  }
  if (path == "/api/sdr-town/sstv-file") {
    std::string fileParam;
    auto pos = queryString.find("path=");
    if (pos != std::string::npos) {
      fileParam = queryString.substr(pos + 5);
      const auto amp = fileParam.find('&');
      if (amp != std::string::npos) fileParam.resize(amp);
      fileParam = urlDecode(fileParam);
    }
    auto isSafeImagePath = [](const std::filesystem::path& candidate) -> bool {
      if (candidate.empty()) return false;
      std::error_code ec;
      auto canonical = std::filesystem::weakly_canonical(candidate, ec);
      if (ec || !std::filesystem::is_regular_file(canonical, ec)) return false;
      const auto ext = canonical.extension().wstring();
      std::wstring lower = ext;
      for (auto& ch : lower) ch = static_cast<wchar_t>(towlower(ch));
      if (lower != L".png" && lower != L".jpg" && lower != L".jpeg" && lower != L".webp") {
        return false;
      }
      auto underRoot = [&](const wchar_t* envName, const wchar_t* child) -> bool {
        wchar_t rootBuf[MAX_PATH]{};
        if (GetEnvironmentVariableW(envName, rootBuf, MAX_PATH) == 0) return false;
        std::filesystem::path rootPath = rootBuf;
        if (child && *child) rootPath /= child;
        const auto allowedRoot = std::filesystem::weakly_canonical(rootPath, ec);
        if (ec) return false;
        const auto fileText = canonical.wstring();
        const auto rootText = allowedRoot.wstring();
        if (fileText.size() < rootText.size()) return false;
        if (_wcsnicmp(fileText.c_str(), rootText.c_str(), rootText.size()) != 0) return false;
        if (fileText.size() > rootText.size() && fileText[rootText.size()] != L'\\' &&
            fileText[rootText.size()] != L'/') {
          return false;
        }
        return true;
      };
      return underRoot(L"APPDATA", L"SDR_Town") || underRoot(L"LOCALAPPDATA", L"SDR_Town") ||
             underRoot(L"USERPROFILE", nullptr);
    };
    const std::filesystem::path imagePath = utf8ToWide(fileParam);
    if (!isSafeImagePath(imagePath)) {
      sendResponse(client, 404, "Not Found", "text/plain", "missing", kCors);
      return;
    }
    std::ifstream file(imagePath, std::ios::binary);
    if (!file) {
      sendResponse(client, 404, "Not Found", "text/plain", "missing", kCors);
      return;
    }
    std::ostringstream bytes;
    bytes << file.rdbuf();
    const auto ext = imagePath.extension().string();
    const char* type = "image/png";
    if (ext == ".jpg" || ext == ".jpeg" || ext == ".JPG" || ext == ".JPEG") type = "image/jpeg";
    else if (ext == ".webp" || ext == ".WEBP") type = "image/webp";
    sendResponse(client, 200, "OK", type, bytes.str(), kCors);
    return;
  }
  if (path == "/api/sdr-town/control-session") {
    sendResponse(client, 200, "OK", "application/json", sdrTownControlSessionActionJson(body),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/status") {
    SdrTownBridge bridge;
    const std::string response = bridge.status(sdrTownControlConfigLocked());
    sendResponse(client, 200, "OK", "application/json", response, kCors);
    return;
  }
  if (path == "/api/sdr-town/satcom-status") {
    SdrTownBridge bridge;
    std::string error;
    const std::string response =
        bridge.request(sdrTownControlConfigLocked(), "GET", "/v1/satcom/status", "{}", &error);
    sendResponse(client, error.empty() ? 200 : 502, error.empty() ? "OK" : "Bad Gateway",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/aircraft-status") {
    SdrTownBridge bridge;
    std::string error;
    const std::string response =
        bridge.request(sdrTownControlConfigLocked(), "GET", "/v1/aircraft/status", "{}", &error);
    sendResponse(client, error.empty() ? 200 : 502, error.empty() ? "OK" : "Bad Gateway",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/aircraft-refresh") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    SdrTownBridge bridge;
    std::string error;
    const std::string response = bridge.request(sdrTownControlConfigLocked(), "POST",
                                                "/v1/aircraft/refresh", body.empty() ? "{}" : body, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/inmarsat-status") {
    SdrTownBridge bridge;
    std::string error;
    const std::string response =
        bridge.request(sdrTownControlConfigLocked(), "GET", "/v1/inmarsat/status", "{}", &error);
    sendResponse(client, error.empty() ? 200 : 502, error.empty() ? "OK" : "Bad Gateway",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/inmarsat-bandplans") {
    SdrTownBridge bridge;
    std::string error;
    const std::string response =
        bridge.request(sdrTownControlConfigLocked(), "GET", "/v1/inmarsat/bandplans", "{}", &error);
    sendResponse(client, error.empty() ? 200 : 502, error.empty() ? "OK" : "Bad Gateway",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/inmarsat-messages") {
    SdrTownBridge bridge;
    std::string error;
    const std::string response =
        bridge.request(sdrTownControlConfigLocked(), "GET", "/v1/inmarsat/messages", "{}", &error);
    sendResponse(client, error.empty() ? 200 : 502, error.empty() ? "OK" : "Bad Gateway",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/inmarsat-control") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    SdrTownBridge bridge;
    std::string error;
    const std::string response = bridge.request(sdrTownControlConfigLocked(), "POST",
                                                "/v1/inmarsat/control", body.empty() ? "{}" : body, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/satcom-control") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    SdrTownBridge bridge;
    std::string error;
    const std::string response = bridge.request(sdrTownControlConfigLocked(), "POST",
                                                "/v1/satcom/control", body.empty() ? "{}" : body, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/satcom-observer") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    SdrTownBridge bridge;
    std::string error;
    const std::string response = bridge.request(sdrTownControlConfigLocked(), "POST",
                                                "/v1/satcom/observer", body.empty() ? "{}" : body, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/satcom-catalogue") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    SdrTownBridge bridge;
    std::string error;
    const std::string response = bridge.request(sdrTownControlConfigLocked(), "POST",
                                                "/v1/satcom/catalogue", body.empty() ? "{}" : body, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/satcom-arm") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    SdrTownBridge bridge;
    std::string error;
    const std::string response = bridge.request(sdrTownControlConfigLocked(), "POST",
                                                "/v1/satcom/arm", body.empty() ? "{}" : body, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/satcom-tle-refresh") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    SdrTownBridge bridge;
    std::string error;
    const std::string response = bridge.request(sdrTownControlConfigLocked(), "POST",
                                                "/v1/satcom/tle/refresh", body.empty() ? "{}" : body, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json",
                 error.empty() ? response
                               : (std::string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}"),
                 kCors);
    return;
  }
  if (path == "/api/sdr-town/tune") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    const auto cfg = sdrTownControlConfigLocked();
    const double mhz = FubarNetDirectory::jsonGetNumber(body, "frequencyMHz", 0.0);
    double hz = FubarNetDirectory::jsonGetNumber(body, "frequencyHz", 0.0);
    if (hz <= 0.0 && mhz > 0.0) hz = mhz * 1000000.0;
    const std::string mode = FubarNetDirectory::jsonGetString(body, "mode");
    const double bwK = FubarNetDirectory::jsonGetNumber(body, "bandwidthKHz", 0.0);
    const double lpfK = FubarNetDirectory::jsonGetNumber(body, "lpfKHz", 0.0);
    const bool hasAudioLpf = body.find("\"audioLpfEnabled\"") != std::string::npos;
    const int audioLpfEnabled = hasAudioLpf
        ? (FubarNetDirectory::jsonGetBool(body, "audioLpfEnabled", true) ? 1 : 0)
        : -1;
    const double rfGain = FubarNetDirectory::jsonGetNumber(body, "rfGainDb", NAN);
    const double volume = FubarNetDirectory::jsonGetNumber(body, "volume", NAN);
    std::string error;
    SdrTownBridge bridge;
    const std::string response = bridge.tune(cfg, hz, mode, bwK > 0.0 ? bwK * 1000.0 : 0.0,
                                             lpfK > 0.0 ? lpfK * 1000.0 : 0.0,
                                             audioLpfEnabled, rfGain, volume, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json", response, kCors);
    return;
  }
  if (path == "/api/sdr-town/mode") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    const auto cfg = sdrTownControlConfigLocked();
    const std::string mode = FubarNetDirectory::jsonGetString(body, "mode");
    std::string error;
    SdrTownBridge bridge;
    const std::string response = bridge.setMode(cfg, mode, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json", response, kCors);
    return;
  }
  if (path == "/api/sdr-town/rf-gain") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    const auto cfg = sdrTownControlConfigLocked();
    const double rfGain = FubarNetDirectory::jsonGetNumber(body, "rfGainDb", NAN);
    std::string error;
    SdrTownBridge bridge;
    const std::string response = bridge.setRfGain(cfg, rfGain, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json", response, kCors);
    return;
  }
  if (path == "/api/sdr-town/volume") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    const auto cfg = sdrTownControlConfigLocked();
    const double volume = FubarNetDirectory::jsonGetNumber(body, "volume", NAN);
    std::string error;
    SdrTownBridge bridge;
    const std::string response = bridge.setVolume(cfg, volume, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json", response, kCors);
    return;
  }
  if (path == "/api/sdr-town/direct-sampling") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    const auto cfg = sdrTownControlConfigLocked();
    const int mode = static_cast<int>(FubarNetDirectory::jsonGetNumber(body, "directSampling", -1.0));
    std::string error;
    SdrTownBridge bridge;
    const std::string response = bridge.setDirectSampling(cfg, mode, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json", response, kCors);
    return;
  }
  if (path == "/api/sdr-town/sdrplay") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    const auto cfg = sdrTownControlConfigLocked();
    std::string error;
    SdrTownBridge bridge;
    const std::string response = bridge.setSdrplay(cfg, body, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json", response, kCors);
    return;
  }
  if (path == "/api/sdr-town/p25-control") {
    std::string controlError;
    if (!sdrTownControlCommandAllowed(body, &controlError)) {
      sendResponse(client, 423, "Locked", "application/json", controlError, kCors);
      return;
    }
    const auto cfg = sdrTownControlConfigLocked();
    const double mhz = FubarNetDirectory::jsonGetNumber(body, "frequencyMHz", 0.0);
    double hz = FubarNetDirectory::jsonGetNumber(body, "frequencyHz", 0.0);
    if (hz <= 0.0 && mhz > 0.0) hz = mhz * 1000000.0;
    const bool autoFollow = FubarNetDirectory::jsonGetBool(body, "autoFollow", true);
    std::string error;
    SdrTownBridge bridge;
    const std::string response = bridge.startP25Control(cfg, hz, autoFollow, &error);
    sendResponse(client, error.empty() ? 200 : 400, error.empty() ? "OK" : "Bad Request",
                 "application/json", response, kCors);
    return;
  }
  if (path == "/fubar-net" || path == "/fubar-net/" || path == "/fubar-net/servers") {
    sendResponse(client, 200, "OK", "application/json", directory_.listJson(), kCors);
    return;
  }
  if (path == "/fubar-net/announce") {
    auto station = FubarNetDirectory::fromAnnounceJson(body);
    std::string error;
    if (!directory_.upsert(station, observedPublicIp(client, request), &error)) {
      sendResponse(client, 400, "Bad Request", "application/json",
                   std::string("{\"ok\":false,\"error\":\"") + error + "\"}", kCors);
      return;
    }
    sendResponse(client, 200, "OK", "application/json",
                 std::string("{\"ok\":true,\"id\":\"") + station.id + "\",\"ttl\":90}", kCors);
    return;
  }
  if (path == "/fubar-net/leave") {
    const auto id = FubarNetDirectory::jsonGetString(body, "id");
    directory_.leave(id);
    sendResponse(client, 200, "OK", "application/json", "{\"ok\":true}", kCors);
    return;
  }
  if (path == "/live" || path == "/live.wav") {
    streamLive(static_cast<std::uintptr_t>(client), true);
    return;
  }
  if (path == "/live.pcm") {
    streamLive(static_cast<std::uintptr_t>(client), false);
    return;
  }
  if (path == "/live.mp3") {
    streamLiveMp3(static_cast<std::uintptr_t>(client));
    return;
  }
  if (path.rfind("/audio/", 0) == 0) {
    const auto id = urlDecode(path.substr(7));
    sendFile(client, rootLocked() / utf8ToWide(id), range);
  }
}
