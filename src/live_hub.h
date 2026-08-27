#pragma once

#include <cstdint>
#include <vector>

#include <windows.h>

class LiveAudioHub {
 public:
  struct Cursor {
    std::uint64_t pos = 0;
    std::uint32_t generation = 0;
  };

  LiveAudioHub();
  ~LiveAudioHub();

  LiveAudioHub(const LiveAudioHub&) = delete;
  LiveAudioHub& operator=(const LiveAudioHub&) = delete;

  void beginSession(std::uint32_t sampleRate, std::uint16_t channels = 1);
  void endSession();
  void pushInterleaved(const std::int16_t* samples, std::size_t count, std::uint16_t channels);
  std::size_t pull(Cursor& cursor, std::int16_t* out, std::size_t maxSamples, DWORD waitMs);
  void setGainDb(float db);
  float gainDb() const;
  std::uint32_t sampleRate() const;
  std::uint16_t channels() const;
  std::uint32_t generation() const;
  bool live() const;

  static void writeWavHeader(std::uint8_t header[44], std::uint32_t sampleRate,
                             std::uint16_t channels = 1);
  static void writePcmHeader(std::uint8_t header[16], std::uint32_t sampleRate,
                             std::uint16_t channels = 1);

 private:
  void resizeLocked(std::uint32_t sampleRate, std::uint16_t channels);

  mutable CRITICAL_SECTION lock_{};
  HANDLE event_ = nullptr;
  std::vector<std::int16_t> buffer_;
  std::uint64_t writePos_ = 0;
  std::uint32_t sampleRate_ = 0;
  std::uint16_t channels_ = 1;
  std::uint32_t generation_ = 1;
  float gainDb_ = 0.0f;
  float gainLinear_ = 1.0f;
  bool live_ = false;
};
