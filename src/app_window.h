#pragma once

#include "audio_engine.h"

#include <windows.h>

#include <chrono>
#include <mutex>
#include <vector>

class AppWindow {
 public:
  explicit AppWindow(AudioOptions initialOptions);
  int run(HINSTANCE instance, int showCommand);

 private:
  static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK replayProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

  LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
  LRESULT handleReplayMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
  void createControls();
  void populateDevices();
  void refreshDevices();
  void startEngine();
  void stopEngine();
  void updateMeters();
  void updateStatus(const std::wstring& status);
  void applyOptionsToControls();
  AudioOptions optionsFromControls() const;
  void browseOutputDirectory();
  void openOutputDirectory() const;
  void showReplayWindow();
  void refreshReplayWindow();
  void playSelectedReplay();
  void saveSettings() const;
  void loadSettings();

  AudioOptions options_;
  AudioEngine engine_;
  HINSTANCE instance_ = nullptr;
  HWND window_ = nullptr;
  HWND replayWindow_ = nullptr;
  HWND replayList_ = nullptr;
  HWND statusLabel_ = nullptr;
  HWND deviceCombo_ = nullptr;
  HWND modeCombo_ = nullptr;
  HWND thresholdSlider_ = nullptr;
  HWND thresholdValue_ = nullptr;
  HWND frequencyEdit_ = nullptr;
  HWND preRollEdit_ = nullptr;
  HWND holdEdit_ = nullptr;
  HWND saveCheck_ = nullptr;
  HWND monitorCheck_ = nullptr;
  HWND forceCheck_ = nullptr;
  HWND appendCheck_ = nullptr;
  HWND splitCheck_ = nullptr;
  HWND outputEdit_ = nullptr;
  HWND startButton_ = nullptr;
  HWND stopButton_ = nullptr;
  HWND inputLeftMeter_ = nullptr;
  HWND inputRightMeter_ = nullptr;
  HWND outputLeftMeter_ = nullptr;
  HWND outputRightMeter_ = nullptr;
  HWND inputLeftValue_ = nullptr;
  HWND inputRightValue_ = nullptr;
  HWND outputLeftValue_ = nullptr;
  HWND outputRightValue_ = nullptr;
  HBRUSH idleStatusBrush_ = nullptr;
  HBRUSH recordingStatusBrush_ = nullptr;
  std::vector<AudioDeviceInfo> devices_;
  std::vector<ReplayEntry> replays_;
  std::vector<std::wstring> probedDeviceIds_;
  std::chrono::steady_clock::time_point inputProbeStarted_{};
  bool autoSelectInput_ = false;
  bool statusRecording_ = false;
};
