#include "web_server.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
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
  <div class="row"><h2>Captures</h2><div class="meta" id="count"></div></div>
  <div id="list" class="empty">Loading captures…</div>
</main>
<div class="player">
  <div class="meta" id="now" style="margin-bottom:6px">Nothing playing</div>
  <audio id="audio" controls preload="none"></audio>
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
  audio.src = '/audio/' + encodeURIComponent(item.id);
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
let liveAbort = null;
let livePlaying = false;
function queueLabel(status){
  const limit = Math.max(1, Number(status && status.listenerLimit) || 5);
  const n = Number(status && status.listeners) || 0;
  const q = Number(status && status.queued) || 0;
  if (q > 0) return n + '/' + limit + ' listening · ' + q + ' waiting';
  return n + '/' + limit + ' listening';
}
function setLiveUi(on, text){
  livePlaying = on;
  liveBtn.textContent = on ? 'Stop live' : 'Listen live';
  liveBtn.classList.toggle('on', on);
  liveHint.textContent = text;
  if (!on) liveLevel.style.width = '0';
}
function stopLive(){
  livePlaying = false;
  if (liveAbort) { liveAbort.abort(); liveAbort = null; }
  setLiveUi(false, 'Same audio the app is capturing right now');
}
async function startLive(){
  audio.pause();
  stopLive();
  liveAbort = new AbortController();
  setLiveUi(true, 'Connecting to live capture…');
  const ac = new (window.AudioContext || window.webkitAudioContext)();
  await ac.resume();
  const poll = setInterval(async () => {
    if (!liveAbort) return;
    try {
      const status = await (await fetch('/api/status', {cache:'no-store'})).json();
      const limit = Math.max(1, Number(status.listenerLimit) || 5);
      const n = Number(status.listeners) || 0;
      const q = Number(status.queued) || 0;
      if (q > 0 || n >= limit) {
        liveHint.textContent = 'Live is full (' + n + '/' + limit + '). Waiting in queue…';
      }
    } catch {}
  }, 800);
  let res;
  try {
    res = await fetch('/live.pcm?t=' + Date.now(), {signal: liveAbort.signal, cache:'no-store'});
  } finally {
    clearInterval(poll);
  }
  if (!res || !res.ok || !res.body) {
    if (res && res.status === 503) throw new Error('queue');
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
  const rate = new DataView(hdr.buffer, hdr.byteOffset, 16).getUint32(8, true);
  let nextTime = ac.currentTime + 0.08;
  setLiveUi(true, 'Live · ' + rate + ' Hz');
  livePlaying = true;
  while (livePlaying) {
    if (buf.length < 512 && !await readMore()) break;
    const maxBytes = Math.max(512, Math.floor(rate / 10) * 2);
    let bytes = buf.length - (buf.length % 2);
    if (bytes > maxBytes) bytes = maxBytes;
    if (bytes < 2) continue;
    const chunk = buf.slice(0, bytes);
    buf = buf.slice(bytes);
    const samples = new Int16Array(chunk.buffer, chunk.byteOffset, chunk.byteLength / 2);
    const audioBuf = ac.createBuffer(1, samples.length, rate);
    const data = audioBuf.getChannelData(0);
    let peak = 0;
    for (let i = 0; i < samples.length; i++) {
      const v = samples[i] / 32768;
      data[i] = v;
      const a = v < 0 ? -v : v;
      if (a > peak) peak = a;
    }
    liveLevel.style.width = Math.min(100, Math.round(peak * 140)) + '%';
    if (nextTime < ac.currentTime + 0.04) nextTime = ac.currentTime + 0.04;
    if (nextTime > ac.currentTime + 0.35) {
      nextTime = ac.currentTime + 0.08;
    }
    const src = ac.createBufferSource();
    src.buffer = audioBuf;
    src.connect(ac.destination);
    src.start(nextTime);
    nextTime += audioBuf.duration;
  }
}
liveBtn.addEventListener('click', () => {
  if (livePlaying) { stopLive(); return; }
  startLive().catch((err) => {
    if (err && err.name === 'AbortError') return;
    setLiveUi(false, err && err.message === 'queue'
      ? 'Live queue is full — tap Listen live to wait again'
      : 'Live stream dropped — tap Listen live');
  });
});
async function refresh(){
  try {
    const [statusRes, listRes] = await Promise.all([
      fetch('/api/status', {cache:'no-store'}),
      fetch('/api/captures', {cache:'no-store'})
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
setInterval(refresh, 4000);
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

bool sendResponse(SOCKET socket, int status, const char* reason, const std::string& type,
                  const std::string& body, const std::string& extra = {}) {
  std::ostringstream header;
  header << "HTTP/1.1 " << status << " " << reason << "\r\n"
         << "Content-Type: " << type << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
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
  DWORD sendTimeout = 5000;
  setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&sendTimeout),
             sizeof(sendTimeout));
  BOOL keepAlive = TRUE;
  setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepAlive),
             sizeof(keepAlive));

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

  for (int wait = 0; hub && !hub->live() && wait < 40 && !stop_; ++wait) Sleep(25);
  const std::uint32_t rate = (hub && hub->sampleRate()) ? hub->sampleRate() : 48000;
  const char* prelude = wavContainer
      ? "HTTP/1.0 200 OK\r\nContent-Type: audio/wav\r\nCache-Control: no-store\r\nConnection: close\r\nicy-name: FUBAR Live\r\n\r\n"
      : "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
  if (!sendAll(client, prelude, static_cast<int>(std::strlen(prelude)))) return;
  if (wavContainer) {
    std::uint8_t wav[44];
    LiveAudioHub::writeWavHeader(wav, rate);
    if (!sendAll(client, reinterpret_cast<const char*>(wav), 44)) return;
  } else {
    std::uint8_t pcmHeader[16];
    LiveAudioHub::writePcmHeader(pcmHeader, rate);
    if (!sendAll(client, reinterpret_cast<const char*>(pcmHeader), 16)) return;
  }

  LiveAudioHub::Cursor cursor;
  std::int16_t pcm[1024];
  std::uint32_t generation = hub ? hub->generation() : 0;
  while (!stop_) {
    if (hub && hub->sampleRate() && hub->sampleRate() != rate) break;
    if (hub && hub->generation() != generation) {
      if (!hub->live()) break;
      generation = hub->generation();
      cursor = {};
    }
    const std::size_t got = hub ? hub->pull(cursor, pcm, 1024, 25) : 0;
    if (got == 0) {
      Sleep(5);
      continue;
    }
    if (!sendAll(client, reinterpret_cast<const char*>(pcm),
                 static_cast<int>(got * sizeof(std::int16_t)))) {
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

  int status = 0;
  std::string type;
  if (!handlePathForTest(method, path, rootLocked(), &status, &type)) {
    sendResponse(client, status ? status : 404, "Error", type.empty() ? "text/plain" : type, "error");
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
  if (path == "/live" || path == "/live.wav") {
    streamLive(static_cast<std::uintptr_t>(client), true);
    return;
  }
  if (path == "/live.pcm") {
    streamLive(static_cast<std::uintptr_t>(client), false);
    return;
  }
  if (path.rfind("/audio/", 0) == 0) {
    const auto id = urlDecode(path.substr(7));
    sendFile(client, rootLocked() / utf8ToWide(id), range);
  }
}
