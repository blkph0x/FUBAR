#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

class WavWriter {
 public:
  WavWriter() = default;
  ~WavWriter();

  WavWriter(const WavWriter&) = delete;
  WavWriter& operator=(const WavWriter&) = delete;

  bool open(const std::filesystem::path& path, std::uint32_t sampleRate, std::uint16_t channels);
  bool write(std::span<const std::int16_t> samples);
  void close();
  bool isOpen() const;
  std::uint64_t framesWritten() const;

 private:
  void writeHeader(std::uint32_t dataBytes);

  std::ofstream file_;
  std::filesystem::path path_;
  std::uint32_t sampleRate_ = 0;
  std::uint16_t channels_ = 0;
  std::uint64_t dataBytes_ = 0;
};
