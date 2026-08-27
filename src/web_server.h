#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <windows.h>

struct CaptureItem {
  std::string id;
  std::string name;
  std::string started;
  std::string mode;
  double durationSeconds = 0.0;
  std::uint64_t bytes = 0;
};

class CaptureWebServer {
 public:
  CaptureWebServer();
  ~CaptureWebServer();

  CaptureWebServer(const CaptureWebServer&) = delete;
  CaptureWebServer& operator=(const CaptureWebServer&) = delete;

  bool start(std::uint16_t port = 80);
  void stop();
  bool running() const;
  std::uint16_t port() const;
  std::wstring lastError() const;
  std::wstring url() const;
  std::wstring lanUrl() const;

  void setRoot(const std::filesystem::path& directory);
  void setLiveStatus(const std::wstring& status, bool recording);

  static std::vector<CaptureItem> listCaptures(const std::filesystem::path& directory);
  static bool safeCaptureId(const std::string& id);
  static double wavDurationSeconds(const std::filesystem::path& path);
  static bool handlePathForTest(const std::string& method, const std::string& path,
                                const std::filesystem::path& root, int* status,
                                std::string* contentType);

 private:
  void acceptLoop();
  void handleClient(std::uintptr_t client);
  std::filesystem::path rootLocked() const;
  std::string statusJson() const;
  std::string capturesJson() const;

  static DWORD WINAPI acceptThreadEntry(LPVOID context);
  static DWORD WINAPI clientThreadEntry(LPVOID context);

  std::uintptr_t listen_ = static_cast<std::uintptr_t>(-1);
  HANDLE thread_ = nullptr;
  std::atomic<bool> stop_{true};
  std::atomic<bool> running_{false};
  std::atomic<bool> recording_{false};
  std::uint16_t port_ = 80;
  mutable CRITICAL_SECTION lock_{};
  std::filesystem::path root_;
  std::wstring liveStatus_ = L"Idle";
  std::wstring lastError_;
};
