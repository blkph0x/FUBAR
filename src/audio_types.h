#pragma once

#include <chrono>
#include <filesystem>
#include <string>

enum class ChannelMode {
  Stereo,
  Left,
  Right,
  Mono
};

inline const wchar_t* channelModeName(ChannelMode mode) {
  switch (mode) {
    case ChannelMode::Stereo: return L"Stereo";
    case ChannelMode::Left: return L"Left only";
    case ChannelMode::Right: return L"Right only";
    case ChannelMode::Mono: return L"Mono mix";
  }
  return L"Stereo";
}

struct AudioOptions {
  std::wstring deviceId;
  std::wstring deviceName;
  ChannelMode mode = ChannelMode::Stereo;
  float thresholdDb = -35.0f;
  float preRollSeconds = 1.0f;
  float holdSeconds = 1.5f;
  bool saveAudio = true;
  bool monitor = true;
  bool forceRecord = false;
  bool appendSession = false;
  bool splitStereoFiles = false;
  double frequencyMhz = 268.0;
  std::filesystem::path outputDirectory = L"recordings";
};

struct LevelSnapshot {
  float inputLeftDb = -90.0f;
  float inputRightDb = -90.0f;
  float outputLeftDb = -90.0f;
  float outputRightDb = -90.0f;
};

struct ReplayEntry {
  std::filesystem::path path;
  std::chrono::system_clock::time_point started;
  double durationSeconds = 0.0;
  float peakDb = -90.0f;
  ChannelMode mode = ChannelMode::Stereo;
  double frequencyMhz = 0.0;
};
