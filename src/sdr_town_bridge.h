#pragma once

#include <cstdint>
#include <string>

struct SdrTownBridgeConfig {
  bool enabled = false;
  bool allowTune = false;
  bool allowMode = false;
  bool allowRfGain = false;
  bool allowP25Control = false;
  std::uint16_t port = 8765;
  std::string token;
};

class SdrTownBridge {
 public:
  SdrTownBridge();
  ~SdrTownBridge();

  SdrTownBridge(const SdrTownBridge&) = delete;
  SdrTownBridge& operator=(const SdrTownBridge&) = delete;

  bool available(std::string* error = nullptr);
  std::string status(const SdrTownBridgeConfig& config);
  std::string tune(const SdrTownBridgeConfig& config,
                   double frequencyHz,
                   const std::string& mode,
                   double bandwidthHz,
                   double lpfHz,
                   int audioLpfEnabled,
                   double rfGainDb,
                   double volume,
                   std::string* error);
  std::string setRfGain(const SdrTownBridgeConfig& config, double rfGainDb, std::string* error);
  std::string setVolume(const SdrTownBridgeConfig& config, double volume, std::string* error);
  std::string startP25Control(const SdrTownBridgeConfig& config,
                              double frequencyHz,
                              bool autoFollow,
                              std::string* error);

 private:
  bool load(std::string* error);

  void* dll_ = nullptr;
  void* fnHealth_ = nullptr;
  void* fnStatus_ = nullptr;
  void* fnTune_ = nullptr;
  void* fnSetRfGain_ = nullptr;
  void* fnSetVolume_ = nullptr;
  void* fnStartP25Control_ = nullptr;
};
