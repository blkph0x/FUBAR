#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "sdr_town_bridge.h"

#include "fubar_net.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct SdrTownControlConfig {
  const char* host;
  std::uint16_t port;
  const char* token;
  std::uint32_t timeoutMs;
};

struct SdrTownTuneRequest {
  double frequencyHz;
  const char* mode;
  double bandwidthHz;
  double lpfHz;
  int audioLpfEnabled;
  double rfGainDb;
  double squelchDb;
  int startDevice;
  int p25AutoFollow;
  int force;  // leave live P25 follow on intentional website tune
};

using HealthFn = int (*)(const SdrTownControlConfig*, char*, size_t);
using StatusFn = int (*)(const SdrTownControlConfig*, char*, size_t);
using TuneFn = int (*)(const SdrTownControlConfig*, const SdrTownTuneRequest*, char*, size_t);
using SetModeFn = int (*)(const SdrTownControlConfig*, const char*, char*, size_t);
using SetRfGainFn = int (*)(const SdrTownControlConfig*, double, char*, size_t);
using SetVolumeFn = int (*)(const SdrTownControlConfig*, double, char*, size_t);
using SetDirectSamplingFn = int (*)(const SdrTownControlConfig*, int, char*, size_t);
using StartP25Fn = int (*)(const SdrTownControlConfig*, double, int, char*, size_t);

std::string wideToUtf8(const std::wstring& value) {
  if (value.empty()) return {};
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                        nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<std::size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), bytes,
                      nullptr, nullptr);
  return out;
}

std::wstring exeDir() {
  wchar_t path[MAX_PATH]{};
  const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::filesystem::path p(std::wstring(path, path + n));
  return p.parent_path().wstring();
}

std::wstring envWide(const wchar_t* name) {
  wchar_t buffer[2048]{};
  const DWORD n = GetEnvironmentVariableW(name, buffer, 2048);
  return n > 0 && n < 2048 ? std::wstring(buffer, buffer + n) : std::wstring();
}

std::string lastWinError() {
  const DWORD code = GetLastError();
  wchar_t* message = nullptr;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, code, 0, reinterpret_cast<LPWSTR>(&message), 0, nullptr);
  std::wstring text = message ? message : L"unknown error";
  if (message) LocalFree(message);
  return wideToUtf8(text);
}

SdrTownControlConfig controlConfig(const SdrTownBridgeConfig& cfg) {
  const auto port = static_cast<std::uint16_t>(cfg.port ? cfg.port : 8765);
  return {"127.0.0.1", port, cfg.token.empty() ? nullptr : cfg.token.c_str(), 2500};
}

std::string jsonEscape(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (unsigned char ch : value) {
    switch (ch) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (ch < 0x20) {
          static constexpr char hex[] = "0123456789abcdef";
          out += "\\u00";
          out += hex[(ch >> 4) & 0x0f];
          out += hex[ch & 0x0f];
        } else {
          out.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
  return out;
}

std::string disabledJson(const char* reason) {
  std::ostringstream out;
  out << "{\"ok\":false,\"error\":\"" << jsonEscape(reason ? reason : "") << "\"}";
  return out.str();
}

bool okResult(int result) { return result == 0; }

}  // namespace

SdrTownBridge::SdrTownBridge() = default;

SdrTownBridge::~SdrTownBridge() {
  if (dll_) {
    FreeLibrary(static_cast<HMODULE>(dll_));
    dll_ = nullptr;
  }
}

bool SdrTownBridge::available(std::string* error) { return load(error); }

bool SdrTownBridge::load(std::string* error) {
  if (dll_) return true;
  std::wstring candidates[3];
  candidates[0] = envWide(L"SDRTOWN_CONTROL_DLL");
  candidates[1] = exeDir() + L"\\SdrTownControl.dll";
  candidates[2] = L"SdrTownControl.dll";
  for (const auto& candidate : candidates) {
    if (candidate.empty()) continue;
    HMODULE lib = LoadLibraryW(candidate.c_str());
    if (!lib) continue;
    fnHealth_ = reinterpret_cast<void*>(GetProcAddress(lib, "SdrTownControl_Health"));
    fnStatus_ = reinterpret_cast<void*>(GetProcAddress(lib, "SdrTownControl_Status"));
    fnTune_ = reinterpret_cast<void*>(GetProcAddress(lib, "SdrTownControl_Tune"));
    fnSetMode_ = reinterpret_cast<void*>(GetProcAddress(lib, "SdrTownControl_SetMode"));
    fnSetRfGain_ = reinterpret_cast<void*>(GetProcAddress(lib, "SdrTownControl_SetRfGain"));
    fnSetVolume_ = reinterpret_cast<void*>(GetProcAddress(lib, "SdrTownControl_SetVolume"));
    fnSetDirectSampling_ =
        reinterpret_cast<void*>(GetProcAddress(lib, "SdrTownControl_SetDirectSampling"));
    fnStartP25Control_ =
        reinterpret_cast<void*>(GetProcAddress(lib, "SdrTownControl_StartP25Control"));
    if (fnHealth_ && fnStatus_ && fnTune_ && fnSetRfGain_ && fnStartP25Control_) {
      dll_ = lib;
      return true;
    }
    FreeLibrary(lib);
  }
  if (error) *error = "SdrTownControl.dll could not be loaded: " + lastWinError();
  return false;
}

std::string SdrTownBridge::status(const SdrTownBridgeConfig& config) {
  if (!config.enabled) return disabledJson("SDR Town control is disabled by the FUBAR admin");
  std::string error;
  if (!load(&error)) return disabledJson(error.c_str());
  char response[32768]{};
  auto cfg = controlConfig(config);
  const int result = reinterpret_cast<StatusFn>(fnStatus_)(&cfg, response, sizeof(response));
  if (!okResult(result) && response[0] == '\0') return disabledJson("SDR Town status request failed");
  return response;
}

std::string SdrTownBridge::tune(const SdrTownBridgeConfig& config,
                                double frequencyHz,
                                const std::string& mode,
                                double bandwidthHz,
                                double lpfHz,
                                int audioLpfEnabled,
                                double rfGainDb,
                                double volume,
                                std::string* error) {
  if (!config.enabled || !config.allowTune) {
    if (error) *error = "Frequency control is disabled";
    return disabledJson("frequency control is disabled");
  }
  if (!load(error)) return disabledJson(error && !error->empty() ? error->c_str() : "DLL missing");

  // Website Take-control operators intentionally leave P25 — force analog retune/mode.
  char response[32768]{};
  auto cfg = controlConfig(config);
  SdrTownTuneRequest req{};
  req.frequencyHz = frequencyHz;
  req.mode = (config.allowMode && !mode.empty()) ? mode.c_str() : nullptr;
  req.bandwidthHz = bandwidthHz;
  req.lpfHz = lpfHz;
  req.audioLpfEnabled = audioLpfEnabled;
  req.rfGainDb = config.allowRfGain ? rfGainDb : NAN;
  req.squelchDb = NAN;
  req.startDevice = 1;
  req.p25AutoFollow = 0;
  req.force = 1;
  const int result = reinterpret_cast<TuneFn>(fnTune_)(&cfg, &req, response, sizeof(response));
  if (!okResult(result) && error) *error = response[0] ? response : "SDR Town tune failed";
  if (okResult(result) && std::isfinite(volume)) {
    char volumeResponse[32768]{};
    const int volumeResult =
        fnSetVolume_
            ? reinterpret_cast<SetVolumeFn>(fnSetVolume_)(&cfg, volume, volumeResponse,
                                                          sizeof(volumeResponse))
            : -4;
    if (!okResult(volumeResult)) {
      if (error) {
        *error = volumeResponse[0] ? volumeResponse : "SDR Town volume control is unavailable";
      }
      return volumeResponse[0] ? volumeResponse : disabledJson("SDR Town volume control is unavailable");
    }
    if (volumeResponse[0]) return volumeResponse;
  }
  return response;
}

std::string SdrTownBridge::setMode(const SdrTownBridgeConfig& config,
                                   const std::string& mode,
                                   std::string* error) {
  if (!config.enabled || !config.allowMode) {
    if (error) *error = "Mode control is disabled";
    return disabledJson("mode control is disabled");
  }
  if (mode.empty()) {
    if (error) *error = "Mode is required";
    return disabledJson("mode is required");
  }
  if (!load(error)) return disabledJson(error && !error->empty() ? error->c_str() : "DLL missing");
  if (!fnSetMode_) {
    if (error) *error = "SdrTownControl.dll does not expose mode switching";
    return disabledJson("SdrTownControl.dll does not expose mode switching");
  }

  char response[32768]{};
  auto cfg = controlConfig(config);
  const int result =
      reinterpret_cast<SetModeFn>(fnSetMode_)(&cfg, mode.c_str(), response, sizeof(response));
  if (!okResult(result) && error) *error = response[0] ? response : "SDR Town mode change failed";
  return response;
}

std::string SdrTownBridge::setRfGain(const SdrTownBridgeConfig& config,
                                     double rfGainDb,
                                     std::string* error) {
  if (!config.enabled || !config.allowRfGain) {
    if (error) *error = "RF gain control is disabled";
    return disabledJson("RF gain control is disabled");
  }
  if (!load(error)) return disabledJson(error && !error->empty() ? error->c_str() : "DLL missing");
  char response[32768]{};
  auto cfg = controlConfig(config);
  const int result =
      reinterpret_cast<SetRfGainFn>(fnSetRfGain_)(&cfg, rfGainDb, response, sizeof(response));
  if (!okResult(result) && error) *error = response[0] ? response : "SDR Town RF gain failed";
  return response;
}

std::string SdrTownBridge::setVolume(const SdrTownBridgeConfig& config,
                                     double volume,
                                     std::string* error) {
  if (!config.enabled || !config.allowTune) {
    if (error) *error = "Volume control is disabled";
    return disabledJson("Volume control is disabled");
  }
  if (!std::isfinite(volume) || volume < 0.0 || volume > 1.0) {
    if (error) *error = "Volume is out of range";
    return disabledJson("Volume is out of range");
  }
  if (!load(error)) return disabledJson(error && !error->empty() ? error->c_str() : "DLL missing");
  if (!fnSetVolume_) {
    if (error) *error = "SdrTownControl.dll does not expose volume control";
    return disabledJson("SdrTownControl.dll does not expose volume control");
  }
  char response[32768]{};
  auto cfg = controlConfig(config);
  const int result =
      reinterpret_cast<SetVolumeFn>(fnSetVolume_)(&cfg, volume, response, sizeof(response));
  if (!okResult(result) && error) *error = response[0] ? response : "SDR Town volume failed";
  return response;
}

std::string SdrTownBridge::setDirectSampling(const SdrTownBridgeConfig& config,
                                             int mode,
                                             std::string* error) {
  if (!config.enabled || !config.allowTune) {
    if (error) *error = "Direct sampling control is disabled";
    return disabledJson("direct sampling control is disabled");
  }
  if (mode < 0 || mode > 2) {
    if (error) *error = "directSampling must be 0, 1, or 2";
    return disabledJson("directSampling must be 0, 1, or 2");
  }
  if (!load(error)) return disabledJson(error && !error->empty() ? error->c_str() : "DLL missing");
  if (!fnSetDirectSampling_) {
    if (error) *error = "SdrTownControl.dll does not expose direct sampling control";
    return disabledJson("SdrTownControl.dll does not expose direct sampling control");
  }
  char response[32768]{};
  auto cfg = controlConfig(config);
  const int result =
      reinterpret_cast<SetDirectSamplingFn>(fnSetDirectSampling_)(&cfg, mode, response,
                                                                  sizeof(response));
  if (!okResult(result) && error) {
    *error = response[0] ? response : "SDR Town direct sampling failed";
  }
  return response;
}

std::string SdrTownBridge::startP25Control(const SdrTownBridgeConfig& config,
                                           double frequencyHz,
                                           bool autoFollow,
                                           std::string* error) {
  if (!config.enabled || !config.allowP25Control) {
    if (error) *error = "P25 control is disabled";
    return disabledJson("P25 control is disabled");
  }
  if (!load(error)) return disabledJson(error && !error->empty() ? error->c_str() : "DLL missing");
  char response[32768]{};
  auto cfg = controlConfig(config);
  const int result = reinterpret_cast<StartP25Fn>(fnStartP25Control_)(
      &cfg, frequencyHz, autoFollow ? 1 : 0, response, sizeof(response));
  if (!okResult(result) && error) *error = response[0] ? response : "SDR Town P25 control failed";
  return response;
}

namespace {

std::wstring sdrTownAppDataFile(const wchar_t* leaf) {
  wchar_t appData[MAX_PATH]{};
  const DWORD len = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) return {};
  return std::wstring(appData) + L"\\SDR_Town\\SDR Town\\" + leaf;
}

std::string readFileUtf8Limited(const std::wstring& path, std::size_t maxBytes) {
  if (path.empty()) return {};
  std::ifstream in(std::filesystem::path(path), std::ios::binary);
  if (!in) return {};
  std::string out;
  out.resize(maxBytes);
  in.read(out.data(), static_cast<std::streamsize>(maxBytes));
  out.resize(static_cast<std::size_t>(std::max<std::streamsize>(0, in.gcount())));
  return out;
}

std::string jsonStringAfterKeyNear(const std::string& json, std::size_t from, const char* key,
                                   std::size_t window) {
  const std::string pat = std::string("\"") + key + "\"";
  const std::size_t end = std::min(json.size(), from + window);
  auto pos = json.find(pat, from);
  if (pos == std::string::npos || pos >= end) return {};
  pos = json.find(':', pos + pat.size());
  if (pos == std::string::npos || pos >= end) return {};
  ++pos;
  while (pos < end && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
  if (pos >= end || json[pos] != '"') return {};
  ++pos;
  std::string out;
  while (pos < json.size() && json[pos] != '"') {
    if (json[pos] == '\\' && pos + 1 < json.size()) {
      ++pos;
      out += json[pos++];
    } else {
      out += json[pos++];
    }
    if (out.size() > 80) break;
  }
  return out;
}

std::string lookupP25TalkgroupAlias(unsigned talkgroupId) {
  if (talkgroupId == 0) return {};
  const std::string talkgroups =
      readFileUtf8Limited(sdrTownAppDataFile(L"p25_talkgroups.json"), 4 * 1024 * 1024);
  char idPat[48];
  std::snprintf(idPat, sizeof(idPat), "\"talkgroupId\": %u", talkgroupId);
  auto tgPos = talkgroups.find(idPat);
  if (tgPos == std::string::npos) {
    std::snprintf(idPat, sizeof(idPat), "\"talkgroupId\":%u", talkgroupId);
    tgPos = talkgroups.find(idPat);
  }
  if (tgPos != std::string::npos) {
    // Prefer the object that contains this talkgroupId (scan backward for '{').
    std::size_t objectStart = talkgroups.rfind('{', tgPos);
    if (objectStart == std::string::npos) objectStart = tgPos > 400 ? tgPos - 400 : 0;
    const std::string manual = jsonStringAfterKeyNear(talkgroups, objectStart, "alphaTag", 500);
    if (!manual.empty()) return manual;
  }

  const std::string aliases =
      readFileUtf8Limited(sdrTownAppDataFile(L"p25_aliases.json"), 4 * 1024 * 1024);
  if (aliases.empty()) return {};
  char aliasIdPat[40];
  std::snprintf(aliasIdPat, sizeof(aliasIdPat), "\"id\": %u", talkgroupId);
  std::size_t search = 0;
  while (search < aliases.size()) {
    auto pos = aliases.find(aliasIdPat, search);
    if (pos == std::string::npos) {
      std::snprintf(aliasIdPat, sizeof(aliasIdPat), "\"id\":%u", talkgroupId);
      pos = aliases.find(aliasIdPat, search);
      if (pos == std::string::npos) break;
    }
    const std::string name = jsonStringAfterKeyNear(aliases, pos, "name", 180);
    if (!name.empty()) return name;
    search = pos + 4;
  }
  return {};
}

}  // namespace

std::string sdrTownP25LiveStatus(const std::string& statusJson) {
  if (statusJson.find("\"ok\":true") == std::string::npos &&
      statusJson.find("\"ok\": true") == std::string::npos) {
    return {};
  }

  std::string label = FubarNetDirectory::jsonGetString(statusJson, "talkgroupStatusLabel");
  const int talkgroupId =
      static_cast<int>(FubarNetDirectory::jsonGetNumber(statusJson, "followTalkgroupId", 0.0));
  const bool voiceActive = talkgroupId > 0 &&
                           (FubarNetDirectory::jsonGetBool(statusJson, "followEnabled", false) ||
                            FubarNetDirectory::jsonGetBool(statusJson, "trafficActive", false) ||
                            FubarNetDirectory::jsonGetBool(statusJson, "warmStandbyActive", false));

  if (voiceActive) {
    if (label.empty()) {
      const std::string alias = lookupP25TalkgroupAlias(static_cast<unsigned>(talkgroupId));
      label = alias.empty() ? ("TG " + std::to_string(talkgroupId))
                            : ("TG " + std::to_string(talkgroupId) + " " + alias);
    }
    return FubarNetDirectory::sanitizeNowPlaying(label);
  }

  const double controlHz =
      FubarNetDirectory::jsonGetNumber(statusJson, "controlFrequencyHz", 0.0);
  const bool onControl =
      controlHz > 0.0 &&
      (FubarNetDirectory::jsonGetBool(statusJson, "autoFollow", false) ||
       FubarNetDirectory::jsonGetNumber(statusJson, "monitorArmedMs", 0.0) > 0.0);
  if (onControl) {
    return FubarNetDirectory::sanitizeNowPlaying("Listening to NSWGRN Control");
  }
  return {};
}
