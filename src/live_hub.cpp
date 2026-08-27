#include "live_hub.h"

#include <algorithm>
#include <cmath>
#include <cstring>

LiveAudioHub::LiveAudioHub() {
  InitializeCriticalSection(&lock_);
  event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

LiveAudioHub::~LiveAudioHub() {
  if (event_) CloseHandle(event_);
  DeleteCriticalSection(&lock_);
}

void LiveAudioHub::resizeLocked(std::uint32_t sampleRate) {
  const std::size_t capacity = static_cast<std::size_t>(std::max<std::uint32_t>(sampleRate, 8000)) * 4;
  buffer_.assign(capacity, 0);
  writePos_ = 0;
  sampleRate_ = sampleRate;
}

void LiveAudioHub::beginSession(std::uint32_t sampleRate) {
  EnterCriticalSection(&lock_);
  resizeLocked(sampleRate ? sampleRate : 48000);
  live_ = true;
  ++generation_;
  SetEvent(event_);
  LeaveCriticalSection(&lock_);
}

void LiveAudioHub::endSession() {
  EnterCriticalSection(&lock_);
  live_ = false;
  ++generation_;
  SetEvent(event_);
  LeaveCriticalSection(&lock_);
}

void LiveAudioHub::pushInterleaved(const std::int16_t* samples, std::size_t count,
                                   std::uint16_t channels) {
  if (!samples || count == 0) return;
  EnterCriticalSection(&lock_);
  if (!live_ || buffer_.empty()) {
    LeaveCriticalSection(&lock_);
    return;
  }
  const std::size_t frames = channels ? count / channels : count;
  for (std::size_t frame = 0; frame < frames; ++frame) {
    std::int32_t mix = 0;
    if (channels <= 1) {
      mix = samples[frame];
    } else {
      for (std::uint16_t ch = 0; ch < channels; ++ch) mix += samples[frame * channels + ch];
      mix /= channels;
    }
    mix = static_cast<std::int32_t>(std::lround(static_cast<double>(mix) * gainLinear_));
    if (mix > 32767) mix = 32767;
    if (mix < -32768) mix = -32768;
    buffer_[static_cast<std::size_t>(writePos_ % buffer_.size())] = static_cast<std::int16_t>(mix);
    ++writePos_;
  }
  SetEvent(event_);
  LeaveCriticalSection(&lock_);
}

std::size_t LiveAudioHub::pull(Cursor& cursor, std::int16_t* out, std::size_t maxSamples,
                               DWORD waitMs) {
  if (!out || maxSamples == 0) return 0;

  auto copyLocked = [&]() -> std::size_t {
    if (buffer_.empty() || writePos_ == 0) return 0;
    if (cursor.generation != generation_) {
      cursor.generation = generation_;
      const std::uint64_t preroll = std::max<std::uint32_t>(sampleRate_ / 12, 160);
      cursor.pos = writePos_ > preroll ? writePos_ - preroll : 0;
    }
    if (cursor.pos > writePos_) cursor.pos = writePos_;
    const std::uint64_t behind = writePos_ - cursor.pos;
    if (behind > buffer_.size()) cursor.pos = writePos_ - (buffer_.size() / 4);
    std::size_t copied = 0;
    while (copied < maxSamples && cursor.pos < writePos_) {
      out[copied++] = buffer_[static_cast<std::size_t>(cursor.pos % buffer_.size())];
      ++cursor.pos;
    }
    return copied;
  };

  EnterCriticalSection(&lock_);
  std::size_t copied = copyLocked();
  if (copied > 0 || waitMs == 0) {
    LeaveCriticalSection(&lock_);
    return copied;
  }
  LeaveCriticalSection(&lock_);
  // Sleep instead of resetting a shared event. ResetEvent starved other listeners
  // and dropped browser streams when two people listened at once.
  Sleep(waitMs);
  EnterCriticalSection(&lock_);
  copied = copyLocked();
  LeaveCriticalSection(&lock_);
  return copied;
}

void LiveAudioHub::setGainDb(float db) {
  if (!std::isfinite(db)) db = 0.0f;
  db = std::clamp(db, 0.0f, 18.0f);
  EnterCriticalSection(&lock_);
  gainDb_ = db;
  gainLinear_ = std::pow(10.0f, db / 20.0f);
  LeaveCriticalSection(&lock_);
}

float LiveAudioHub::gainDb() const {
  EnterCriticalSection(&lock_);
  const float value = gainDb_;
  LeaveCriticalSection(&lock_);
  return value;
}

std::uint32_t LiveAudioHub::sampleRate() const {
  EnterCriticalSection(&lock_);
  const std::uint32_t rate = sampleRate_;
  LeaveCriticalSection(&lock_);
  return rate;
}

std::uint32_t LiveAudioHub::generation() const {
  EnterCriticalSection(&lock_);
  const std::uint32_t value = generation_;
  LeaveCriticalSection(&lock_);
  return value;
}

bool LiveAudioHub::live() const {
  EnterCriticalSection(&lock_);
  const bool value = live_;
  LeaveCriticalSection(&lock_);
  return value;
}

void LiveAudioHub::writeWavHeader(std::uint8_t header[44], std::uint32_t sampleRate) {
  if (!sampleRate) sampleRate = 48000;
  const std::uint32_t byteRate = sampleRate * 2;
  std::memset(header, 0, 44);
  std::memcpy(header, "RIFF", 4);
  header[4] = header[5] = header[6] = header[7] = 0xFF;
  std::memcpy(header + 8, "WAVEfmt ", 8);
  header[16] = 16;
  header[20] = 1;
  header[22] = 1;
  header[24] = static_cast<std::uint8_t>(sampleRate);
  header[25] = static_cast<std::uint8_t>(sampleRate >> 8);
  header[26] = static_cast<std::uint8_t>(sampleRate >> 16);
  header[27] = static_cast<std::uint8_t>(sampleRate >> 24);
  header[28] = static_cast<std::uint8_t>(byteRate);
  header[29] = static_cast<std::uint8_t>(byteRate >> 8);
  header[30] = static_cast<std::uint8_t>(byteRate >> 16);
  header[31] = static_cast<std::uint8_t>(byteRate >> 24);
  header[32] = 2;
  header[34] = 16;
  std::memcpy(header + 36, "data", 4);
  header[40] = header[41] = header[42] = header[43] = 0xFF;
}

void LiveAudioHub::writePcmHeader(std::uint8_t header[16], std::uint32_t sampleRate) {
  if (!sampleRate) sampleRate = 48000;
  std::memcpy(header, "FUBARPCM", 8);
  header[8] = static_cast<std::uint8_t>(sampleRate);
  header[9] = static_cast<std::uint8_t>(sampleRate >> 8);
  header[10] = static_cast<std::uint8_t>(sampleRate >> 16);
  header[11] = static_cast<std::uint8_t>(sampleRate >> 24);
  header[12] = 1;
  header[13] = 0;
  header[14] = 16;
  header[15] = 0;
}
