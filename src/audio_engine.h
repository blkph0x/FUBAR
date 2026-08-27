#pragma once

#include "audio_types.h"
#include "live_hub.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include <windows.h>

struct AudioDeviceInfo {
  std::wstring id;
  std::wstring name;
  bool isDefault = false;
};

bool testAudioSampleDecoder();

class AudioEngine {
 public:
  using StatusCallback = std::function<void(const std::wstring&)>;
  using ReplayCallback = std::function<void(const ReplayEntry&)>;

  AudioEngine();
  ~AudioEngine();

  AudioEngine(const AudioEngine&) = delete;
  AudioEngine& operator=(const AudioEngine&) = delete;

  static std::vector<AudioDeviceInfo> enumerateInputDevices(std::wstring* error = nullptr);

  bool start(const AudioOptions& options, StatusCallback statusCallback,
             ReplayCallback replayCallback);
  void stop();
  bool running() const;
  bool recording() const;
  LevelSnapshot levels() const;
  std::wstring status() const;
  LiveAudioHub& liveHub() { return liveHub_; }

 private:
  void captureThread();
  static DWORD WINAPI captureThreadEntry(LPVOID context);
  void setStatus(const std::wstring& status);

  AudioOptions options_;
  StatusCallback statusCallback_;
  ReplayCallback replayCallback_;
  HANDLE thread_ = nullptr;
  std::atomic<bool> stopRequested_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> recording_{false};
  std::atomic<float> inputLeftDb_{-90.0f};
  std::atomic<float> inputRightDb_{-90.0f};
  std::atomic<float> outputLeftDb_{-90.0f};
  std::atomic<float> outputRightDb_{-90.0f};
  mutable CRITICAL_SECTION statusLock_{};
  std::wstring status_ = L"Idle";
  LiveAudioHub liveHub_;
};
