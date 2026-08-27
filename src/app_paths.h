#pragma once

#include "audio_types.h"
#include "web_server.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cwctype>
#include <filesystem>
#include <string>
#include <vector>

#include <knownfolders.h>
#include <shlobj.h>
#include <windows.h>

inline std::filesystem::path executableDirectory() {
  std::wstring buffer(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  buffer.resize(length);
  return std::filesystem::path(buffer).parent_path();
}

inline std::filesystem::path fubarRoamingDirectory() {
  PWSTR appdata = nullptr;
  std::filesystem::path root;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &appdata)) &&
      appdata) {
    root = std::filesystem::path(appdata) / L"FUBAR";
    CoTaskMemFree(appdata);
  } else {
    root = executableDirectory() / L"FUBAR";
  }
  std::error_code error;
  std::filesystem::create_directories(root, error);
  return root;
}

inline std::filesystem::path defaultCaptureDirectory() {
  const auto dir = fubarRoamingDirectory() / L"Vox_captures";
  std::error_code error;
  std::filesystem::create_directories(dir, error);
  return dir;
}

inline std::filesystem::path fubarSettingsPath() {
  return fubarRoamingDirectory() / L"FUBAR.ini";
}

inline bool isPlaceholderCapturePath(const std::filesystem::path& path) {
  if (path.empty() || !path.is_absolute()) return true;
  if (path.filename() != L"recordings") return false;
  std::error_code error;
  return std::filesystem::equivalent(path.parent_path(), executableDirectory(), error);
}

inline void migrateLegacyCaptures(const std::filesystem::path& destination) {
  const auto legacy = executableDirectory() / L"recordings";
  std::error_code error;
  if (!std::filesystem::exists(legacy, error) || !std::filesystem::is_directory(legacy, error)) {
    return;
  }
  if (std::filesystem::equivalent(legacy, destination, error)) return;
  std::filesystem::create_directories(destination, error);
  for (const auto& entry : std::filesystem::directory_iterator(legacy, error)) {
    if (error || !entry.is_regular_file()) continue;
    if (entry.path().extension() != L".wav") continue;
    const auto target = destination / entry.path().filename();
    if (!std::filesystem::exists(target, error)) {
      std::filesystem::copy_file(entry.path(), target, error);
    }
  }
}

inline ReplayEntry replayEntryFromFile(const std::filesystem::path& path, double frequencyMhz) {
  ReplayEntry entry;
  entry.path = path;
  entry.frequencyMhz = frequencyMhz;
  entry.durationSeconds = CaptureWebServer::wavDurationSeconds(path);
  const auto stem = path.stem().wstring();
  if (stem.find(L"_left") != std::wstring::npos) entry.mode = ChannelMode::Left;
  else if (stem.find(L"_right") != std::wstring::npos) entry.mode = ChannelMode::Right;
  else if (stem.find(L"_mono") != std::wstring::npos) entry.mode = ChannelMode::Mono;
  else entry.mode = ChannelMode::Stereo;

  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (swscanf(stem.c_str(), L"FUBAR_%4d%2d%2d_%2d%2d%2d", &year, &month, &day, &hour, &minute,
              &second) == 6) {
    std::tm local{};
    local.tm_year = year - 1900;
    local.tm_mon = month - 1;
    local.tm_mday = day;
    local.tm_hour = hour;
    local.tm_min = minute;
    local.tm_sec = second;
    local.tm_isdst = -1;
    const std::time_t time = std::mktime(&local);
    if (time != -1) entry.started = std::chrono::system_clock::from_time_t(time);
  }
  if (entry.started.time_since_epoch().count() == 0) {
    std::error_code error;
    const auto fileTime = std::filesystem::last_write_time(path, error);
    if (!error) {
      entry.started = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
          fileTime - std::filesystem::file_time_type::clock::now() +
          std::chrono::system_clock::now());
    } else {
      entry.started = std::chrono::system_clock::now();
    }
  }
  return entry;
}

inline std::vector<ReplayEntry> loadCapturesFromDirectory(const std::filesystem::path& directory,
                                                          double frequencyMhz) {
  std::vector<ReplayEntry> captures;
  std::error_code error;
  if (!std::filesystem::exists(directory, error)) return captures;
  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (error || !entry.is_regular_file()) continue;
    auto ext = entry.path().extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    if (ext != L".wav") continue;
    captures.push_back(replayEntryFromFile(entry.path(), frequencyMhz));
  }
  std::sort(captures.begin(), captures.end(),
            [](const ReplayEntry& a, const ReplayEntry& b) { return a.started < b.started; });
  return captures;
}
