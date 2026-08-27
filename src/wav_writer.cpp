#include "wav_writer.h"

#include <array>
#include <cstring>

namespace {
template <typename T>
void writeValue(std::ostream& stream, T value) {
  stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
}

WavWriter::~WavWriter() {
  close();
}

bool WavWriter::open(const std::filesystem::path& path, std::uint32_t sampleRate,
                     std::uint16_t channels) {
  close();
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  path_ = path;
  sampleRate_ = sampleRate;
  channels_ = channels;
  dataBytes_ = 0;
  file_.open(path, std::ios::binary | std::ios::trunc);
  if (!file_) return false;
  writeHeader(0);
  return static_cast<bool>(file_);
}

bool WavWriter::write(std::span<const std::int16_t> samples) {
  if (!file_ || samples.empty()) return static_cast<bool>(file_);
  const auto bytes = static_cast<std::streamsize>(samples.size_bytes());
  file_.write(reinterpret_cast<const char*>(samples.data()), bytes);
  if (!file_) return false;
  dataBytes_ += static_cast<std::uint64_t>(bytes);
  return true;
}

void WavWriter::close() {
  if (!file_) return;
  const auto clampedBytes = static_cast<std::uint32_t>(
      dataBytes_ > 0xffffffffULL ? 0xffffffffULL : dataBytes_);
  file_.seekp(0, std::ios::beg);
  writeHeader(clampedBytes);
  file_.flush();
  file_.close();
}

bool WavWriter::isOpen() const {
  return file_.is_open();
}

std::uint64_t WavWriter::framesWritten() const {
  const auto frameBytes = static_cast<std::uint64_t>(channels_) * sizeof(std::int16_t);
  return frameBytes == 0 ? 0 : dataBytes_ / frameBytes;
}

void WavWriter::writeHeader(std::uint32_t dataBytes) {
  const std::uint16_t bitsPerSample = 16;
  const std::uint16_t blockAlign = static_cast<std::uint16_t>(channels_ * bitsPerSample / 8);
  const std::uint32_t byteRate = sampleRate_ * blockAlign;
  const std::uint32_t riffSize = 36 + dataBytes;
  const std::uint32_t fmtSize = 16;
  const std::uint16_t pcmFormat = 1;

  file_.write("RIFF", 4);
  writeValue(file_, riffSize);
  file_.write("WAVE", 4);
  file_.write("fmt ", 4);
  writeValue(file_, fmtSize);
  writeValue(file_, pcmFormat);
  writeValue(file_, channels_);
  writeValue(file_, sampleRate_);
  writeValue(file_, byteRate);
  writeValue(file_, blockAlign);
  writeValue(file_, bitsPerSample);
  file_.write("data", 4);
  writeValue(file_, dataBytes);
}
