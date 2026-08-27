#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <winhttp.h>

#include "fubar_net.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace {

constexpr const char* kHubListenUrl = "https://gearsqueens.online/fubar/";
constexpr const wchar_t* kHubHost = L"gearsqueens.online";
constexpr const char* kHubPublicIp = "43.240.40.251";

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
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
          out += buf;
        } else {
          out += static_cast<char>(ch);
        }
    }
  }
  return out;
}

void skipWs(const std::string& json, std::size_t& pos) {
  while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
}

bool isHubSideHost(const std::string& host) {
  if (host.empty()) return true;
  if (FubarNetDirectory::isPrivateIp(host)) return true;
  if (host == kHubPublicIp) return true;
  if (host == "gearsqueens.online" || host == "www.gearsqueens.online") return true;
  if (host == "127.0.0.1" || host == "localhost") return true;
  return false;
}

}  // namespace

FubarNetDirectory::FubarNetDirectory() { InitializeCriticalSection(&lock_); }
FubarNetDirectory::~FubarNetDirectory() { DeleteCriticalSection(&lock_); }

bool FubarNetDirectory::validStationId(const std::string& id) {
  if (id.size() < 8 || id.size() > 64) return false;
  return std::all_of(id.begin(), id.end(), [](unsigned char ch) {
    return std::isalnum(ch) || ch == '-' || ch == '_';
  });
}

std::string FubarNetDirectory::sanitizeName(const std::string& name) {
  std::string out;
  for (unsigned char ch : name) {
    if (std::isalnum(ch) || ch == ' ' || ch == '-' || ch == '_' || ch == '.') {
      out += static_cast<char>(ch);
    }
    if (out.size() >= 32) break;
  }
  while (!out.empty() && out.front() == ' ') out.erase(out.begin());
  while (!out.empty() && out.back() == ' ') out.pop_back();
  if (out.empty()) out = "FUBAR";
  return out;
}

std::string FubarNetDirectory::sanitizeHost(const std::string& host) {
  std::string out;
  for (unsigned char ch : host) {
    const char lower = static_cast<char>(std::tolower(ch));
    if (std::isalnum(static_cast<unsigned char>(lower)) || lower == '.' || lower == '-') {
      out += lower;
    }
    if (out.size() >= 80) break;
  }
  return out;
}

std::string FubarNetDirectory::sanitizePath(const std::string& path) {
  if (path.empty() || path[0] != '/') return "/";
  std::string out = "/";
  for (std::size_t i = 1; i < path.size() && out.size() < 40; ++i) {
    const unsigned char ch = static_cast<unsigned char>(path[i]);
    if (std::isalnum(ch) || ch == '/' || ch == '-' || ch == '_') out += static_cast<char>(ch);
  }
  if (out.size() > 1 && out.back() != '/') out += '/';
  if (out.find("..") != std::string::npos) return "/";
  return out;
}

bool FubarNetDirectory::isPrivateIp(const std::string& host) {
  if (host == "127.0.0.1" || host == "localhost" || host == "::1") return true;
  unsigned a = 0, b = 0, c = 0, d = 0;
  if (std::sscanf(host.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
  if (a == 10) return true;
  if (a == 192 && b == 168) return true;
  if (a == 172 && b >= 16 && b <= 31) return true;
  if (a == 169 && b == 254) return true;
  if (a == 127) return true;
  return false;
}

bool FubarNetDirectory::looksLikeHostname(const std::string& host) {
  if (host.size() < 4 || host.find('.') == std::string::npos) return false;
  bool letter = false;
  for (unsigned char ch : host) {
    if (std::isalpha(ch)) letter = true;
  }
  return letter && !isPrivateIp(host);
}

std::string FubarNetDirectory::makeStationId() {
  GUID guid{};
  if (FAILED(CoCreateGuid(&guid))) {
    char fallback[40];
    std::snprintf(fallback, sizeof(fallback), "fubar-%08lx-%04lx",
                  static_cast<unsigned long>(GetTickCount()),
                  static_cast<unsigned long>(GetCurrentProcessId() & 0xffff));
    return fallback;
  }
  char text[40];
  std::snprintf(text, sizeof(text), "%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
                static_cast<unsigned long>(guid.Data1), guid.Data2, guid.Data3, guid.Data4[0],
                guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5],
                guid.Data4[6], guid.Data4[7]);
  return text;
}

std::string FubarNetDirectory::jsonGetString(const std::string& json, const char* key) {
  const std::string pat = std::string("\"") + key + "\"";
  auto pos = json.find(pat);
  if (pos == std::string::npos) return {};
  pos = json.find(':', pos + pat.size());
  if (pos == std::string::npos) return {};
  ++pos;
  skipWs(json, pos);
  if (pos >= json.size() || json[pos] != '"') return {};
  ++pos;
  std::string out;
  while (pos < json.size() && json[pos] != '"') {
    if (json[pos] == '\\' && pos + 1 < json.size()) {
      ++pos;
      const char esc = json[pos++];
      if (esc == 'n') out += '\n';
      else if (esc == '"') out += '"';
      else if (esc == '\\') out += '\\';
      else out += esc;
    } else {
      out += json[pos++];
    }
    if (out.size() > 120) break;
  }
  return out;
}

double FubarNetDirectory::jsonGetNumber(const std::string& json, const char* key, double fallback) {
  const std::string pat = std::string("\"") + key + "\"";
  auto pos = json.find(pat);
  if (pos == std::string::npos) return fallback;
  pos = json.find(':', pos + pat.size());
  if (pos == std::string::npos) return fallback;
  ++pos;
  skipWs(json, pos);
  if (pos >= json.size()) return fallback;
  char* end = nullptr;
  const double value = std::strtod(json.c_str() + pos, &end);
  if (end == json.c_str() + pos) return fallback;
  return value;
}

bool FubarNetDirectory::jsonGetBool(const std::string& json, const char* key, bool fallback) {
  const std::string pat = std::string("\"") + key + "\"";
  auto pos = json.find(pat);
  if (pos == std::string::npos) return fallback;
  pos = json.find(':', pos + pat.size());
  if (pos == std::string::npos) return fallback;
  ++pos;
  skipWs(json, pos);
  if (json.compare(pos, 4, "true") == 0) return true;
  if (json.compare(pos, 5, "false") == 0) return false;
  return fallback;
}

FubarNetStation FubarNetDirectory::fromAnnounceJson(const std::string& json) {
  FubarNetStation station;
  station.id = jsonGetString(json, "id");
  station.name = sanitizeName(jsonGetString(json, "name"));
  station.host = sanitizeHost(jsonGetString(json, "host"));
  const double port = jsonGetNumber(json, "port", 80);
  station.port = static_cast<std::uint16_t>(std::clamp(port, 1.0, 65535.0));
  station.path = sanitizePath(jsonGetString(json, "path"));
  station.frequencyMhz = jsonGetNumber(json, "frequencyMhz", 0.0);
  station.recording = jsonGetBool(json, "recording", false);
  station.live = jsonGetBool(json, "live", true);
  station.listeners = static_cast<int>(std::max(0.0, jsonGetNumber(json, "listeners", 0)));
  station.listenerLimit =
      static_cast<int>(std::clamp(jsonGetNumber(json, "listenerLimit", 5), 1.0, 64.0));
  station.version = sanitizeName(jsonGetString(json, "version"));
  if (station.version.empty()) station.version = "1.1.8";
  return station;
}

std::string FubarNetDirectory::publicUrl(const std::string& host, std::uint16_t port,
                                         const std::string& path) {
  if (isHubSideHost(host)) return kHubListenUrl;
  std::string out = (port == 443) ? "https://" : "http://";
  out += host;
  if (port != 80 && port != 443) {
    out += ":";
    out += std::to_string(port);
  }
  out += path.empty() ? "/" : path;
  return out;
}

void FubarNetDirectory::sweepLocked() const {
  const ULONGLONG now = GetTickCount64();
  stations_.erase(std::remove_if(stations_.begin(), stations_.end(),
                                 [&](const FubarNetStation& item) {
                                   return now - item.lastSeen > kTtlMs;
                                 }),
                  stations_.end());
}

bool FubarNetDirectory::upsert(FubarNetStation station, const std::string& observedIp,
                               std::string* error) {
  if (!validStationId(station.id)) {
    if (error) *error = "invalid station id";
    return false;
  }
  station.name = sanitizeName(station.name);
  std::string host = sanitizeHost(station.host);
  const std::string observed = sanitizeHost(observedIp);
  if (!looksLikeHostname(host)) {
    host = observed;
  }
  if (host.empty()) {
    if (error) *error = "could not determine public address";
    return false;
  }
  if (station.listeners < 0) station.listeners = 0;
  if (station.listenerLimit < 1) station.listenerLimit = 5;
  station.host = host;
  station.path = sanitizePath(station.path);
  if (isHubSideHost(host)) station.path = "/fubar/";
  station.url = publicUrl(host, station.port, station.path);
  station.lastSeen = GetTickCount64();

  EnterCriticalSection(&lock_);
  sweepLocked();
  auto existing = std::find_if(stations_.begin(), stations_.end(),
                               [&](const FubarNetStation& item) { return item.id == station.id; });
  int fromIp = 0;
  for (const auto& item : stations_) {
    if (item.host == station.host) ++fromIp;
  }
  if (existing == stations_.end()) {
    if (static_cast<int>(stations_.size()) >= kMaxStations) {
      LeaveCriticalSection(&lock_);
      if (error) *error = "directory is full";
      return false;
    }
    if (fromIp >= 8) {
      LeaveCriticalSection(&lock_);
      if (error) *error = "too many stations from this address";
      return false;
    }
    stations_.push_back(std::move(station));
  } else {
    *existing = std::move(station);
  }
  LeaveCriticalSection(&lock_);
  return true;
}

void FubarNetDirectory::leave(const std::string& id) {
  EnterCriticalSection(&lock_);
  stations_.erase(std::remove_if(stations_.begin(), stations_.end(),
                                 [&](const FubarNetStation& item) { return item.id == id; }),
                  stations_.end());
  LeaveCriticalSection(&lock_);
}

std::string FubarNetDirectory::listJson() const {
  EnterCriticalSection(&lock_);
  sweepLocked();
  std::ostringstream json;
  json << "{\"ok\":true,\"hub\":\"https://gearsqueens.online/fubar-net\",\"ttl\":"
       << (kTtlMs / 1000) << ",\"count\":" << stations_.size() << ",\"servers\":[";
  const ULONGLONG now = GetTickCount64();
  for (std::size_t i = 0; i < stations_.size(); ++i) {
    const auto& s = stations_[i];
    if (i) json << ",";
    json << "{\"id\":\"" << jsonEscape(s.id) << "\",\"name\":\"" << jsonEscape(s.name)
         << "\",\"host\":\"" << jsonEscape(s.host) << "\",\"port\":" << s.port << ",\"url\":\""
         << jsonEscape(s.url) << "\",\"frequencyMhz\":" << s.frequencyMhz
         << ",\"recording\":" << (s.recording ? "true" : "false")
         << ",\"live\":" << (s.live ? "true" : "false") << ",\"listeners\":" << s.listeners
         << ",\"listenerLimit\":" << s.listenerLimit << ",\"version\":\"" << jsonEscape(s.version)
         << "\",\"ageSeconds\":" << ((now - s.lastSeen) / 1000) << "}";
  }
  json << "]}";
  const std::string out = json.str();
  LeaveCriticalSection(&lock_);
  return out;
}

int FubarNetDirectory::size() const {
  EnterCriticalSection(&lock_);
  sweepLocked();
  const int value = static_cast<int>(stations_.size());
  LeaveCriticalSection(&lock_);
  return value;
}

FubarNetClient::FubarNetClient() {
  InitializeCriticalSection(&lock_);
  wake_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

FubarNetClient::~FubarNetClient() {
  stop();
  if (wake_) CloseHandle(wake_);
  DeleteCriticalSection(&lock_);
}

void FubarNetClient::setPayload(const FubarNetStation& station) {
  EnterCriticalSection(&lock_);
  payload_ = station;
  LeaveCriticalSection(&lock_);
}

void FubarNetClient::start() {
  if (running_) return;
  stop_ = false;
  ResetEvent(wake_);
  running_ = true;
  thread_ = CreateThread(nullptr, 0, threadEntry, this, 0, nullptr);
  if (!thread_) {
    running_ = false;
    stop_ = true;
  }
}

void FubarNetClient::stop() {
  stop_ = true;
  if (wake_) SetEvent(wake_);
  if (thread_) {
    WaitForSingleObject(thread_, 8000);
    CloseHandle(thread_);
    thread_ = nullptr;
  }
  running_ = false;
}

bool FubarNetClient::running() const { return running_; }

std::wstring FubarNetClient::lastError() const {
  EnterCriticalSection(&lock_);
  std::wstring value = lastError_;
  LeaveCriticalSection(&lock_);
  return value;
}

std::wstring FubarNetClient::lastOk() const {
  EnterCriticalSection(&lock_);
  std::wstring value = lastOk_;
  LeaveCriticalSection(&lock_);
  return value;
}

DWORD WINAPI FubarNetClient::threadEntry(LPVOID context) {
  static_cast<FubarNetClient*>(context)->loop();
  return 0;
}

void FubarNetClient::loop() {
  while (!stop_) {
    sendAnnounce();
    WaitForSingleObject(wake_, kHeartbeatMs);
  }
  sendLeave();
}

bool FubarNetClient::postHttps(const wchar_t* path, const std::string& body, std::string* response,
                               std::wstring* error) {
  HINTERNET session = WinHttpOpen(L"FUBAR/1.1.8", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    if (error) *error = L"Could not open HTTPS session";
    return false;
  }
  WinHttpSetTimeouts(session, 4000, 4000, 4000, 6000);
  HINTERNET connect = WinHttpConnect(session, kHubHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!connect) {
    WinHttpCloseHandle(session);
    if (error) *error = L"Could not reach gearsqueens.online";
    return false;
  }
  HINTERNET request = WinHttpOpenRequest(connect, L"POST", path, nullptr, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!request) {
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    if (error) *error = L"Could not open HTTPS request";
    return false;
  }
  const wchar_t headers[] = L"Content-Type: application/json\r\n";
  const BOOL sent =
      WinHttpSendRequest(request, headers, static_cast<DWORD>(-1),
                         body.empty() ? nullptr : const_cast<char*>(body.data()),
                         static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) &&
      WinHttpReceiveResponse(request, nullptr);
  if (!sent) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    if (error) *error = L"HTTPS announce failed";
    return false;
  }
  DWORD status = 0;
  DWORD statusSize = sizeof(status);
  WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
  std::string collected;
  char buffer[1024];
  DWORD got = 0;
  while (WinHttpReadData(request, buffer, sizeof(buffer), &got) && got > 0) {
    collected.append(buffer, got);
    if (collected.size() > 8192) break;
  }
  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
  if (response) *response = collected;
  if (status < 200 || status >= 300) {
    if (error) *error = L"Directory returned HTTP " + std::to_wstring(status);
    return false;
  }
  return true;
}

bool FubarNetClient::sendAnnounce() {
  FubarNetStation station;
  EnterCriticalSection(&lock_);
  station = payload_;
  LeaveCriticalSection(&lock_);
  if (station.id.empty()) return false;
  std::ostringstream json;
  json << "{\"id\":\"" << jsonEscape(station.id) << "\",\"name\":\"" << jsonEscape(station.name)
       << "\",\"port\":" << station.port << ",\"path\":\"" << jsonEscape(station.path)
       << "\",\"frequencyMhz\":" << station.frequencyMhz
       << ",\"recording\":" << (station.recording ? "true" : "false")
       << ",\"live\":" << (station.live ? "true" : "false") << ",\"listeners\":" << station.listeners
       << ",\"listenerLimit\":" << station.listenerLimit << ",\"version\":\""
       << jsonEscape(station.version) << "\"";
  if (!station.host.empty() && FubarNetDirectory::looksLikeHostname(station.host)) {
    json << ",\"host\":\"" << jsonEscape(station.host) << "\"";
  }
  json << "}";
  std::string response;
  std::wstring error;
  const bool ok = postHttps(L"/fubar-net/announce", json.str(), &response, &error);
  EnterCriticalSection(&lock_);
  if (ok) {
    lastError_.clear();
    lastOk_ = L"Listed on gearsqueens.online";
  } else {
    lastError_ = error;
  }
  LeaveCriticalSection(&lock_);
  return ok;
}

void FubarNetClient::sendLeave() {
  FubarNetStation station;
  EnterCriticalSection(&lock_);
  station = payload_;
  LeaveCriticalSection(&lock_);
  if (station.id.empty()) return;
  const std::string body = std::string("{\"id\":\"") + jsonEscape(station.id) + "\"}";
  std::string response;
  std::wstring error;
  postHttps(L"/fubar-net/leave", body, &response, &error);
}
