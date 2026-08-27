#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "app_window.h"
#include "audio_engine.h"
#include "audio_safety.h"
#include "wav_writer.h"
#include "vox_gate.h"
#include "web_server.h"
#include "live_hub.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::atomic<bool> consoleStop{false};

bool hasCliFlag(int argc, wchar_t** argv) {
  for (int index = 1; index < argc; ++index) {
    if (std::wstring_view(argv[index]) == L"--cli") return true;
  }
  return false;
}

void configureConsole(int argc, wchar_t** argv) {
  if (hasCliFlag(argc, argv)) return;

  const HWND consoleWindow = GetConsoleWindow();
  if (consoleWindow == nullptr) return;

  DWORD consoleProcesses[2]{};
  if (GetConsoleProcessList(consoleProcesses, 2) == 1) {
    ShowWindow(consoleWindow, SW_HIDE);
  }
  FreeConsole();
}

BOOL WINAPI consoleHandler(DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
    consoleStop = true;
    return TRUE;
  }
  return FALSE;
}

void printHelp() {
  std::wcout
      << L"FUBAR 1.1.3 - VOX audio monitor and recorder\n\n"
      << L"Usage:\n"
      << L"  FUBAR.exe                                  Open GUI without a console\n"
      << L"  FUBAR.exe --cli --list-devices             List capture devices\n"
      << L"  FUBAR.exe --cli --headless [options]       Run without GUI\n"
      << L"  FUBAR.exe --cli --self-test                Test WAV output, web UI, and CLI\n\n"
      << L"Options:\n"
      << L"  --cli                 Keep the terminal attached for logs and input\n"
      << L"  --web                 Enable the public capture website on port 80\n"
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
      << L"  --force-record        Record continuously instead of VOX\n"
      << L"  --append-session      Pause on silence and resume into the same WAV\n"
      << L"  --split-stereo        Write stereo as separate mono left/right WAV files\n"
      << L"  --web                 Serve the public capture website on TCP port 80\n";
}

ChannelMode parseMode(const std::wstring& text) {
  if (text == L"left") return ChannelMode::Left;
  if (text == L"right") return ChannelMode::Right;
  if (text == L"mono") return ChannelMode::Mono;
  return ChannelMode::Stereo;
}

int runSelfTest() {
  if (!testAudioSampleDecoder()) {
    std::wcerr << L"Self-test failed: audio sample decoder error\n";
    return 1;
  }

  if (!isVirtualCableMonitorLoop(L"CABLE Output (VB-Audio Virtual Cable)",
                                 L"CABLE Input (VB-Audio Virtual Cable)") ||
      !isVirtualCableMonitorLoop(L"CABLE-A Output (VB-Audio Cable A)",
                                 L"CABLE-A Input (VB-Audio Cable A)") ||
      isVirtualCableMonitorLoop(L"CABLE-A Output (VB-Audio Cable A)",
                                L"CABLE-B Input (VB-Audio Cable B)") ||
      isVirtualCableMonitorLoop(L"Microphone (USB Audio)", L"Speakers (USB Audio)") ||
      !isVirtualAudioEndpoint(L"CABLE Output (VB-Audio Virtual Cable)") ||
      isVirtualAudioEndpoint(L"Microphone (USB Audio)") ||
      sanitizeAudioSample(INFINITY) != 0.0f || sanitizeAudioSample(NAN) != 0.0f ||
      sanitizeAudioSample(2.0) != 1.0f || sanitizeAudioSample(-2.0) != -1.0f) {
    std::wcerr << L"Self-test failed: virtual-cable safety error\n";
    return 1;
  }

  VoxGate gate(100);
  if (gate.update(false, 50, false, true) != VoxAction::None ||
      gate.update(true, 10, false, true) != VoxAction::Start ||
      gate.update(false, 40, true, true) != VoxAction::None ||
      gate.update(false, 60, true, true) != VoxAction::Pause || gate.active() ||
      gate.update(true, 10, true, true) != VoxAction::Resume ||
      gate.update(false, 100, true, false) != VoxAction::Finish || gate.active()) {
    std::wcerr << L"Self-test failed: VOX append-session transition error\n";
    return 1;
  }

  const auto path = std::filesystem::temp_directory_path() / L"fubar_self_test.wav";
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
  std::vector<std::int16_t> resumedAudio(400, 1000);
  if (!writer.write(resumedAudio)) {
    std::wcerr << L"Self-test failed: resumed WAV write error\n";
    return 1;
  }
  writer.close();
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (size != 2444) {
    std::filesystem::remove(path, error);
    std::wcerr << L"Self-test failed: expected 2444-byte WAV, got " << size << L"\n";
    return 1;
  }
  const auto webDir = std::filesystem::temp_directory_path() / L"fubar_web_test";
  std::filesystem::create_directories(webDir);
  const auto clip = webDir / L"FUBAR_20260101_120000_stereo.wav";
  std::filesystem::copy_file(path, clip, std::filesystem::copy_options::overwrite_existing, error);
  if (!CaptureWebServer::safeCaptureId("FUBAR_20260101_120000_stereo.wav") ||
      CaptureWebServer::safeCaptureId("../secret.wav") ||
      CaptureWebServer::safeCaptureId("nope.txt")) {
    std::wcerr << L"Self-test failed: capture id filter error\n";
    return 1;
  }
  int status = 0;
  std::string type;
  if (!CaptureWebServer::handlePathForTest("GET", "/", webDir, &status, &type) || status != 200 ||
      !CaptureWebServer::handlePathForTest("GET", "/api/captures", webDir, &status, &type) ||
      CaptureWebServer::handlePathForTest("GET", "/audio/../secret.wav", webDir, &status, &type) ||
      status != 400) {
    std::wcerr << L"Self-test failed: web path filter error\n";
    return 1;
  }
  const auto listed = CaptureWebServer::listCaptures(webDir);
  if (listed.empty() || listed[0].id != "FUBAR_20260101_120000_stereo.wav") {
    std::wcerr << L"Self-test failed: capture listing error\n";
    return 1;
  }
  CaptureWebServer server;
  server.setRoot(webDir);
  if (!server.start(18080)) {
    std::wcerr << L"Self-test failed: could not bind test web port 18080\n";
    return 1;
  }
  SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(18080);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  const bool connected = sock != INVALID_SOCKET &&
                         ::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
  std::string response;
  if (connected) {
    const char request[] = "GET /api/captures HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    send(sock, request, sizeof(request) - 1, 0);
    char buf[2048];
    int got = 0;
    while ((got = recv(sock, buf, sizeof(buf), 0)) > 0) response.append(buf, static_cast<std::size_t>(got));
  }
  if (sock != INVALID_SOCKET) closesocket(sock);
  server.stop();
  std::error_code cleanup;
  std::filesystem::remove(clip, cleanup);
  std::filesystem::remove(path, cleanup);
  std::filesystem::remove(webDir, cleanup);
  if (!connected || response.find("FUBAR_20260101_120000_stereo.wav") == std::string::npos) {
    std::wcerr << L"Self-test failed: website did not list the capture\n";
    return 1;
  }

  LiveAudioHub hub;
  hub.beginSession(8000);
  std::vector<std::int16_t> tone(800);
  for (int i = 0; i < 800; ++i) tone[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(i);
  hub.pushInterleaved(tone.data(), tone.size(), 1);
  LiveAudioHub::Cursor cursor;
  std::vector<std::int16_t> pulled(800);
  const std::size_t got = hub.pull(cursor, pulled.data(), pulled.size(), 0);
  if (got < 400 || pulled[10] != 10) {
    std::wcerr << L"Self-test failed: live audio hub\n";
    return 1;
  }
  std::uint8_t wavHeader[44];
  LiveAudioHub::writeWavHeader(wavHeader, 8000);
  if (std::memcmp(wavHeader, "RIFF", 4) != 0 || std::memcmp(wavHeader + 8, "WAVE", 4) != 0) {
    std::wcerr << L"Self-test failed: live WAV header\n";
    return 1;
  }
  if (!CaptureWebServer::handlePathForTest("GET", "/live.wav", webDir, &status, &type) ||
      status != 200) {
    std::wcerr << L"Self-test failed: live stream path\n";
    return 1;
  }
  server.setLiveHub(&hub);
  if (!server.start(18080)) {
    std::wcerr << L"Self-test failed: could not restart website for live stream\n";
    return 1;
  }
  Sleep(50);
  SOCKET liveSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  const bool liveConnected =
      liveSock != INVALID_SOCKET &&
      ::connect(liveSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
  std::string liveHead;
  if (liveConnected) {
    DWORD timeout = 1500;
    setsockopt(liveSock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
               sizeof(timeout));
    const char liveReq[] = "GET /live.wav HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    send(liveSock, liveReq, sizeof(liveReq) - 1, 0);
    hub.pushInterleaved(tone.data(), tone.size(), 1);
    char buf[1024];
    int n = 0;
    while (liveHead.size() < 512 && (n = recv(liveSock, buf, sizeof(buf), 0)) > 0) {
      liveHead.append(buf, static_cast<std::size_t>(n));
    }
  }
  if (liveSock != INVALID_SOCKET) closesocket(liveSock);
  server.stop();
  if (!liveConnected || liveHead.find("audio/wav") == std::string::npos ||
      liveHead.find("RIFF") == std::string::npos) {
    std::wcerr << L"Self-test failed: live stream did not send WAV audio\n";
    return 1;
  }
  std::wcout << L"Self-test passed: CLI, WAV writer, website, and live stream are operational.\n";
  return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  configureConsole(argc, argv);

  AudioOptions options;
  bool headless = false;
  bool listDevices = false;
  bool selfTest = false;
  bool webEnabled = false;
  double durationSeconds = 0.0;
  int deviceIndex = -1;

  for (int index = 1; index < argc; ++index) {
    const std::wstring argument = argv[index];
    auto next = [&]() -> std::wstring {
      if (index + 1 >= argc) throw std::runtime_error("Missing option value");
      return argv[++index];
    };
    try {
      if (argument == L"--cli") {
        continue;
      } else if (argument == L"--help" || argument == L"-h") {
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
      } else if (argument == L"--append-session") {
        options.appendSession = true;
      } else if (argument == L"--split-stereo") {
        options.splitStereoFiles = true;
      } else if (argument == L"--web") {
        webEnabled = true;
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
      std::wcout << index << L": " << devices[index].name
                 << (devices[index].isDefault ? L" (default)" : L"") << L"\n";
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

  AudioEngine engine;
  CaptureWebServer website;
  if (webEnabled) {
    website.setRoot(options.outputDirectory);
    website.setLiveHub(&engine.liveHub());
    if (!website.start(80)) {
      std::wcerr << L"Website failed: " << website.lastError() << L"\n";
    } else {
      std::wcout << L"[web] " << website.lanUrl() << L"\n";
    }
  }

  SetConsoleCtrlHandler(consoleHandler, TRUE);
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
  website.stop();
  return 0;
}
