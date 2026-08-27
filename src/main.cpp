#include "app_window.h"
#include "audio_engine.h"
#include "wav_writer.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::atomic<bool> consoleStop{false};

BOOL WINAPI consoleHandler(DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
    consoleStop = true;
    return TRUE;
  }
  return FALSE;
}

void printHelp() {
  std::wcout
      << L"AudioVox 1.0 - VOX audio monitor and recorder\n\n"
      << L"Usage:\n"
      << L"  AudioVox.exe                         Open GUI and start listening\n"
      << L"  AudioVox.exe --list-devices          List capture devices\n"
      << L"  AudioVox.exe --headless [options]    Run without GUI\n"
      << L"  AudioVox.exe --self-test             Test WAV output and CLI\n\n"
      << L"Options:\n"
      << L"  --device N            Capture device index from --list-devices\n"
      << L"  --mode MODE           stereo, left, right, or mono\n"
      << L"  --threshold-db DB     VOX threshold, e.g. -35\n"
      << L"  --pre-roll SEC        Audio retained before trigger (default 1.0)\n"
      << L"  --hold SEC            Silence before clip closes (default 1.5)\n"
      << L"  --output PATH         Recording directory\n"
      << L"  --frequency MHZ       Frequency metadata shown in replay log\n"
      << L"  --duration SEC        Headless run duration; 0 waits for Ctrl+C\n"
      << L"  --no-monitor          Disable live speaker monitoring\n"
      << L"  --no-save             Meter/monitor only; do not write clips\n"
      << L"  --force-record        Record continuously instead of VOX\n";
}

ChannelMode parseMode(const std::wstring& text) {
  if (text == L"left") return ChannelMode::Left;
  if (text == L"right") return ChannelMode::Right;
  if (text == L"mono") return ChannelMode::Mono;
  return ChannelMode::Stereo;
}

int runSelfTest() {
  const auto path = std::filesystem::temp_directory_path() / L"audiovox_self_test.wav";
  WavWriter writer;
  if (!writer.open(path, 8000, 1)) {
    std::wcerr << L"Self-test failed: could not create " << path << L"\n";
    return 1;
  }
  std::vector<std::int16_t> silence(800, 0);
  if (!writer.write(silence)) {
    std::wcerr << L"Self-test failed: WAV write error\n";
    return 1;
  }
  writer.close();
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  std::filesystem::remove(path, error);
  if (size != 1644) {
    std::wcerr << L"Self-test failed: expected 1644-byte WAV, got " << size << L"\n";
    return 1;
  }
  std::wcout << L"Self-test passed: CLI and WAV writer are operational.\n";
  return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  AudioOptions options;
  bool headless = false;
  bool listDevices = false;
  bool selfTest = false;
  double durationSeconds = 0.0;
  int deviceIndex = -1;

  for (int index = 1; index < argc; ++index) {
    const std::wstring argument = argv[index];
    auto next = [&]() -> std::wstring {
      if (index + 1 >= argc) throw std::runtime_error("Missing option value");
      return argv[++index];
    };
    try {
      if (argument == L"--help" || argument == L"-h") {
        printHelp();
        return 0;
      } else if (argument == L"--list-devices") {
        listDevices = true;
      } else if (argument == L"--self-test") {
        selfTest = true;
      } else if (argument == L"--headless") {
        headless = true;
      } else if (argument == L"--device") {
        deviceIndex = std::stoi(next());
      } else if (argument == L"--mode") {
        options.mode = parseMode(next());
      } else if (argument == L"--threshold-db") {
        options.thresholdDb = std::stof(next());
      } else if (argument == L"--pre-roll") {
        options.preRollSeconds = std::stof(next());
      } else if (argument == L"--hold") {
        options.holdSeconds = std::stof(next());
      } else if (argument == L"--output") {
        options.outputDirectory = next();
      } else if (argument == L"--frequency") {
        options.frequencyMhz = std::stod(next());
      } else if (argument == L"--duration") {
        durationSeconds = std::stod(next());
      } else if (argument == L"--no-monitor") {
        options.monitor = false;
      } else if (argument == L"--no-save") {
        options.saveAudio = false;
      } else if (argument == L"--force-record") {
        options.forceRecord = true;
      } else {
        std::wcerr << L"Unknown option: " << argument << L"\n";
        printHelp();
        return 2;
      }
    } catch (const std::exception& error) {
      std::cerr << "Invalid argument: " << error.what() << "\n";
      return 2;
    }
  }

  if (selfTest) return runSelfTest();

  std::wstring deviceError;
  const auto devices = AudioEngine::enumerateInputDevices(&deviceError);
  if (listDevices) {
    if (!deviceError.empty()) std::wcerr << deviceError << L"\n";
    for (std::size_t index = 0; index < devices.size(); ++index) {
      std::wcout << index << L": " << devices[index].name << L"\n";
    }
    return devices.empty() ? 1 : 0;
  }

  if (deviceIndex >= 0) {
    if (deviceIndex >= static_cast<int>(devices.size())) {
      std::wcerr << L"Device index is out of range. Use --list-devices.\n";
      return 2;
    }
    options.deviceId = devices[deviceIndex].id;
    options.deviceName = devices[deviceIndex].name;
  }

  if (!headless) {
    AppWindow app(options);
    return app.run(GetModuleHandleW(nullptr), SW_SHOWDEFAULT);
  }

  SetConsoleCtrlHandler(consoleHandler, TRUE);
  AudioEngine engine;
  engine.start(
      options,
      [](const std::wstring& status) { std::wcout << L"[status] " << status << L"\n"; },
      [](const ReplayEntry& entry) {
        std::wcout << L"[recording] " << entry.path.wstring() << L" ("
                   << entry.durationSeconds << L" sec, peak " << entry.peakDb << L" dB)\n";
      });

  const auto started = std::chrono::steady_clock::now();
  while (!consoleStop) {
    if (durationSeconds > 0.0 &&
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() >=
            durationSeconds) {
      break;
    }
    Sleep(100);
  }
  engine.stop();
  return 0;
}
