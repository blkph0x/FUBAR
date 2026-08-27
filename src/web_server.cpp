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
.livebox button{ border:0; border-radius:999px; padding:10px 18px; font-weight:800; cursor:pointer; background:var(--live); color:#fff; }
.livebox button.on{ background:var(--green); color:#111; }
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
</style>
</head>
<body>
<header>
  <p class="kicker">Communication is key</p>
  <h1>FUBAR</h1>
  <p class="sub">Listen live to what FUBAR hears, or play back saved captures.</p>
  <div class="pill"><span id="dot" class="dot"></span><span id="live">Connecting…</span></div>
  <div class="livebox">
    <button id="liveBtn" type="button">Listen live</button>
    <div>
      <div class="name">Live monitor</div>
      <div class="when" id="liveHint">Same audio the app is capturing right now</div>
    </div>
    <div class="level" title="Live level"><span id="liveLevel"></span></div>
  </div>
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
  <audio id="liveMedia" muted playsinline webkit-playsinline autoplay style="position:absolute;width:1px;height:1px;opacity:0;pointer-events:none"></audio>
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
let livePlaying = false;
let liveWanted = false;
let liveAbort = null;
let liveAc = null;
let liveNode = null;
let liveWake = null;
let livePeak = 0;
let meterRaf = 0;
const SILENT_WAV = 'data:audio/wav;base64,UklGRigAAABXQVZFZm10IBIAAAABAAEARKwAAIhYAQACABAAAABkYXRhAgAAAAEA';
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
  try { if (liveAc && liveAc.state !== 'closed' && liveAc.close) liveAc.close(); } catch {}
  liveAc = null;
}
function stopLive(){
  liveWanted = false;
  livePlaying = false;
  if (liveAbort) { liveAbort.abort(); liveAbort = null; }
  closeLiveAudio();
  try { liveMedia.pause(); liveMedia.removeAttribute('src'); liveMedia.load(); } catch {}
  try { if (liveWake) liveWake.release(); } catch {}
  liveWake = null;
  try { if (navigator.mediaSession) navigator.mediaSession.playbackState = 'none'; } catch {}
  setLiveUi(false, 'Same audio the app is capturing right now');
}
async function keepLiveAlive(){
  if (!liveWanted) return;
  try { if (liveAc && liveAc.state !== 'running') await liveAc.resume(); } catch {}
  try {
    liveMedia.muted = true;
    liveMedia.volume = 0;
    if (liveMedia.paused) {
      liveMedia.src = SILENT_WAV;
      liveMedia.loop = true;
      await liveMedia.play();
    }
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
  return (pcm) => {
    const merged = new Float32Array(hold.length + pcm.length);
    merged.set(hold);
    for (let i = 0; i < pcm.length; i++) merged[hold.length + i] = pcm[i] * scale;
    const frames = Math.floor(merged.length / ch);
    const out = [];
    while (phase + 1 < frames) {
      const i0 = phase | 0;
      const frac = phase - i0;
      const i1 = i0 + 1;
      for (let c = 0; c < ch; c++) {
        const a = merged[i0 * ch + c];
        const b = merged[i1 * ch + c];
        out.push(a + (b - a) * frac);
      }
      phase += step;
    }
    const consumed = phase | 0;
    hold = merged.subarray(consumed * ch).slice();
    phase -= consumed;
    return Float32Array.from(out);
  };
}
function foldFrame(outs, i, samples, srcCh){
  const outCh = outs.length;
  if (srcCh === 1) {
    for (let c = 0; c < outCh; c++) outs[c][i] = samples[0];
    return;
  }
  if (outCh === 1) {
    let sum = 0;
    for (let c = 0; c < srcCh; c++) sum += samples[c];
    outs[0][i] = sum / srcCh;
    return;
  }
  for (let c = 0; c < outCh; c++) outs[c][i] = samples[c < srcCh ? c : 0];
}
function attachScriptRing(ac, channels){
  const sr = ac.sampleRate;
  const srcCh = Math.max(1, channels);
  const outCh = Math.max(1, Math.min(2, srcCh));
  const n = Math.max(16384, Math.floor(sr * 8) * srcCh);
  const ring = new Float32Array(n);
  let w = 0, r = 0, primed = false;
  const avail = () => { let a = w - r; if (a < 0) a += n; return a; };
  const primeNeed = Math.floor(sr * 1.0) * srcCh;
  const lowNeed = Math.floor(sr * 0.2) * srcCh;
  let node;
  try { node = ac.createScriptProcessor(4096, 0, outCh); }
  catch { node = ac.createScriptProcessor(4096, 1, outCh); }
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
    const frame = new Float32Array(srcCh);
    for (let i = 0; i < frames; i++) {
      for (let c = 0; c < srcCh; c++) {
        const v = ring[r];
        r++; if (r >= n) r = 0;
        frame[c] = v;
        const abs = v < 0 ? -v : v;
        if (abs > peak) peak = abs;
      }
      foldFrame(outs, i, frame, srcCh);
    }
    livePeak = peak;
  };
  node.connect(ac.destination);
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
        this.n = Math.max(16384, sampleRate * 8 * 2);
        this.ring = new Float32Array(this.n);
        this.w = 0; this.r = 0; this.primed = false; this.tick = 0;
        this.port.onmessage = (e) => {
          const d = e.data || {};
          if (d.ch) this.ch = d.ch;
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
          if (out.length === 1) out[0][i] = srcCh > 1 ? sum / 2 : left;
          else {
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
    outputChannelCount: [srcCh > 1 ? 2 : 1],
    channelCount: srcCh > 1 ? 2 : 1,
    channelCountMode: 'explicit'
  });
  let fillSec = 0;
  node.port.onmessage = (e) => {
    const d = e.data || {};
    livePeak = Number(d.p) || 0;
    if (d.f != null) fillSec = Number(d.f) || 0;
  };
  node.port.postMessage({ch: srcCh});
  node.connect(ac.destination);
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
async function playLiveSession(){
  audio.pause();
  closeLiveAudio();
  liveAbort = new AbortController();
  if (navigator.audioSession) { try { navigator.audioSession.type = 'playback'; } catch {} }
  liveMedia.muted = true;
  liveMedia.volume = 0;
  liveMedia.loop = true;
  liveMedia.src = SILENT_WAV;
  try { await liveMedia.play(); } catch {}
  await keepLiveAlive();
  const res = await fetch('live.pcm?v=115&t=' + Date.now(), {signal: liveAbort.signal, cache:'no-store'});
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
  let ac;
  try { ac = new (window.AudioContext || window.webkitAudioContext)({sampleRate: rate, latencyHint:'playback'}); }
  catch { ac = new (window.AudioContext || window.webkitAudioContext)({latencyHint:'playback'}); }
  liveAc = ac;
  await ac.resume();
  let tap;
  try {
    if (ac.audioWorklet) tap = await attachWorkletRing(ac, channels);
    else tap = attachScriptRing(ac, channels);
  } catch {
    tap = attachScriptRing(ac, channels);
  }
  liveNode = tap.node;
  const resample = makeResampler(rate, ac.sampleRate, channels);
  startMeter();
  if (navigator.mediaSession) {
    try {
      navigator.mediaSession.metadata = new MediaMetadata({title:'FUBAR Live', artist:'FUBAR'});
      navigator.mediaSession.playbackState = 'playing';
      navigator.mediaSession.setActionHandler('pause', () => ac.suspend());
      navigator.mediaSession.setActionHandler('play', () => ac.resume());
      navigator.mediaSession.setActionHandler('stop', () => stopLive());
    } catch {}
  }
  setLiveUi(true, 'Live · ' + rate + ' Hz 16-bit PCM' + (channels>1?' stereo':'') + ' · 1:1 · ~1s buffer');
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
    try { if (liveAc === ac && ac.state !== 'closed' && ac.close) ac.close(); } catch {}
    if (liveAc === ac) liveAc = null;
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
      if (!liveWanted || (err && err.name === 'AbortError')) return;
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
  startLive();
});
document.addEventListener('visibilitychange', keepLiveAlive);
window.addEventListener('pageshow', keepLiveAlive);
window.addEventListener('focus', keepLiveAlive);
document.addEventListener('resume', keepLiveAlive);
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
      return `<a class="station" href="${esc(url)}" target="_blank" rel="noopener">
        <div><div class="name">${esc(s.name||'FUBAR')}</div>
        <div class="when">${esc(freq)} · ${esc(state)} · ${esc(people)} listening</div></div>
        <span class="visit">Open</span></a>`;
    }).join('');
  } catch {
    netCount.textContent = '';
    box.className = 'empty';
    box.textContent = 'Could not reach the public station list.';
  }
}
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
    render();
  } catch {
    live.textContent = 'Website unreachable';
    dot.className = 'dot';
  }
}
refresh();
loadStations();
setInterval(refresh, 4000);
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

void CaptureWebServer::setMaxLiveListeners(int limit) { liveSlots_.setLimit(limit); }
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
  const std::uint16_t port = port_;
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
       << "}";
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
      ? "HTTP/1.0 200 OK\r\nContent-Type: audio/wav\r\nCache-Control: no-store, no-cache, must-revalidate\r\nConnection: close\r\nicy-name: FUBAR Live\r\n\r\n"
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
      "HTTP/1.0 200 OK\r\nContent-Type: audio/mpeg\r\nCache-Control: no-store\r\n"
      "Connection: close\r\nicy-name: FUBAR Live\r\nicy-br: 192\r\n\r\n";
  if (!sendAll(client, prelude, static_cast<int>(std::strlen(prelude)))) return;

  LiveAudioHub::Cursor cursor;
  std::vector<std::int16_t> pcm(4096);
  std::vector<std::uint8_t> mp3;
  std::uint32_t generation = hub ? hub->generation() : 0;
  int emptyPulls = 0;
  const std::size_t silenceFrames = static_cast<std::size_t>(encoder.samplesPerPass());
  std::vector<std::int16_t> silence(silenceFrames * streamChannels, 0);
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
  auto query = path.find('?');
  if (query != std::string::npos) path.resize(query);
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

  int status = 0;
  std::string type;
  if (!handlePathForTest(method, path, rootLocked(), &status, &type)) {
    sendResponse(client, status ? status : 404, "Error", type.empty() ? "text/plain" : type, "error",
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
