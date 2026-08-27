#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

struct FubarNetStation {
  std::string id;
  std::string name;
  std::string host;
  std::uint16_t port = 80;
  std::string path = "/";
  std::string url;
  double frequencyMhz = 0.0;
  bool recording = false;
  bool live = false;
  int listeners = 0;
  int listenerLimit = 5;
  std::string version = "1.1.8";
  ULONGLONG lastSeen = 0;
};

class FubarNetDirectory {
 public:
  static constexpr DWORD kTtlMs = 90000;
  static constexpr int kMaxStations = 200;

  FubarNetDirectory();
  ~FubarNetDirectory();

  FubarNetDirectory(const FubarNetDirectory&) = delete;
  FubarNetDirectory& operator=(const FubarNetDirectory&) = delete;

  bool upsert(FubarNetStation station, const std::string& observedIp, std::string* error);
  void leave(const std::string& id);
  std::string listJson() const;
  int size() const;

  static bool validStationId(const std::string& id);
  static std::string sanitizeName(const std::string& name);
  static std::string sanitizeHost(const std::string& host);
  static std::string sanitizePath(const std::string& path);
  static bool isPrivateIp(const std::string& host);
  static bool looksLikeHostname(const std::string& host);
  static std::string makeStationId();
  static std::string jsonGetString(const std::string& json, const char* key);
  static double jsonGetNumber(const std::string& json, const char* key, double fallback = 0.0);
  static bool jsonGetBool(const std::string& json, const char* key, bool fallback = false);
  static FubarNetStation fromAnnounceJson(const std::string& json);
  static std::string publicUrl(const std::string& host, std::uint16_t port, const std::string& path);

 private:
  void sweepLocked() const;

  mutable CRITICAL_SECTION lock_{};
  mutable std::vector<FubarNetStation> stations_;
};

class FubarNetClient {
 public:
  static constexpr DWORD kHeartbeatMs = 25000;

  FubarNetClient();
  ~FubarNetClient();

  FubarNetClient(const FubarNetClient&) = delete;
  FubarNetClient& operator=(const FubarNetClient&) = delete;

  void setPayload(const FubarNetStation& station);
  void start();
  void stop();
  bool running() const;
  std::wstring lastError() const;
  std::wstring lastOk() const;

  static bool postHttps(const wchar_t* path, const std::string& body, std::string* response,
                        std::wstring* error);

 private:
  void loop();
  bool sendAnnounce();
  void sendLeave();
  static DWORD WINAPI threadEntry(LPVOID context);

  mutable CRITICAL_SECTION lock_{};
  FubarNetStation payload_{};
  HANDLE thread_ = nullptr;
  HANDLE wake_ = nullptr;
  std::atomic<bool> stop_{true};
  std::atomic<bool> running_{false};
  std::wstring lastError_;
  std::wstring lastOk_;
};
