#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "sdr_town_bridge.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

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
};

using HealthFn = int (*)(const SdrTownControlConfig*, char*, size_t);
using StatusFn = int (*)(const SdrTownControlConfig*, char*, size_t);
using TuneFn = int (*)(const SdrTownControlConfig*, const SdrTownTuneRequest*, char*, size_t);
using SetRfGainFn = int (*)(const SdrTownControlConfig*, double, char*, size_t);
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
    fnSetRfGain_ = reinterpret_cast<void*>(GetProcAddress(lib, "SdrTownControl_SetRfGain"));
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
                                double rfGainDb,
                                std::string* error) {
  if (!config.enabled || !config.allowTune) {
    if (error) *error = "Frequency control is disabled";
    return disabledJson("frequency control is disabled");
  }
  if (!load(error)) return disabledJson(error && !error->empty() ? error->c_str() : "DLL missing");
  char response[32768]{};
  auto cfg = controlConfig(config);
  SdrTownTuneRequest req{};
  req.frequencyHz = frequencyHz;
  req.mode = (config.allowMode && !mode.empty()) ? mode.c_str() : nullptr;
  req.bandwidthHz = bandwidthHz;
  req.lpfHz = 0.0;
  req.audioLpfEnabled = -1;
  req.rfGainDb = config.allowRfGain ? rfGainDb : NAN;
  req.squelchDb = NAN;
  req.startDevice = 1;
  req.p25AutoFollow = 0;
  const int result = reinterpret_cast<TuneFn>(fnTune_)(&cfg, &req, response, sizeof(response));
  if (!okResult(result) && error) *error = response[0] ? response : "SDR Town tune failed";
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
