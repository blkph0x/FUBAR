#pragma once

#include <cstdint>
#include <vector>

class LiveMp3Encoder {
 public:
  LiveMp3Encoder();
  ~LiveMp3Encoder();

  LiveMp3Encoder(const LiveMp3Encoder&) = delete;
  LiveMp3Encoder& operator=(const LiveMp3Encoder&) = delete;

  bool open(std::uint32_t sampleRate, std::uint16_t channels);
  bool encodeInterleaved(const std::int16_t* samples, std::size_t count, std::vector<std::uint8_t>* out);
  void close();
  bool ready() const { return encoder_ != nullptr; }
  std::uint32_t encodeRate() const { return encodeRate_; }
  std::uint16_t channels() const { return channels_; }
  int samplesPerPass() const { return samplesPerPass_; }

 private:
  void* encoder_ = nullptr;
  std::uint32_t inputRate_ = 0;
  std::uint32_t encodeRate_ = 0;
  std::uint16_t channels_ = 1;
  int samplesPerPass_ = 1152;
  double resamplePhase_ = 0.0;
  std::vector<std::int16_t> inputHold_;
  std::vector<std::int16_t> pending_;

  static int nearestShineRate(std::uint32_t rate);
  bool encodePending(std::vector<std::uint8_t>* out);
};
