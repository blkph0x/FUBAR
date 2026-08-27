#include "live_mp3.h"

extern "C" {
#include "../third_party/shine/src/lib/layer3.h"
}

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

}  // namespace

LiveMp3Encoder::LiveMp3Encoder() = default;

LiveMp3Encoder::~LiveMp3Encoder() { close(); }

int LiveMp3Encoder::nearestShineRate(std::uint32_t rate) {
  const int rates[] = {48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};
  int best = 48000;
  int bestDiff = std::abs(static_cast<int>(rate) - 48000);
  for (int candidate : rates) {
    const int diff = std::abs(static_cast<int>(rate) - candidate);
    if (diff < bestDiff) {
      best = candidate;
      bestDiff = diff;
    }
  }
  return best;
}

bool LiveMp3Encoder::open(std::uint32_t sampleRate, std::uint16_t channels) {
  close();
  if (!sampleRate) sampleRate = 48000;
  channels_ = channels == 2 ? 2 : 1;
  inputRate_ = sampleRate;
  encodeRate_ = static_cast<std::uint32_t>(nearestShineRate(sampleRate));

  shine_config_t config{};
  config.wave.channels = channels_ == 2 ? PCM_STEREO : PCM_MONO;
  config.wave.samplerate = static_cast<int>(encodeRate_);
  shine_set_config_mpeg_defaults(&config.mpeg);
  config.mpeg.mode = channels_ == 2 ? JOINT_STEREO : MONO;
  const int prefer[] = {192, 160, 128, 112, 96, 80, 64, 56, 48, 40, 32, 24, 16, 8};
  config.mpeg.bitr = 0;
  for (int bitrate : prefer) {
    if (channels_ == 1 && bitrate > 128) continue;
    if (shine_check_config(config.wave.samplerate, bitrate) >= 0) {
      config.mpeg.bitr = bitrate;
      break;
    }
  }
  if (config.mpeg.bitr <= 0) return false;

  shine_t shine = shine_initialise(&config);
  if (!shine) return false;
  encoder_ = shine;
  samplesPerPass_ = shine_samples_per_pass(shine);
  pending_.clear();
  pending_.reserve(static_cast<std::size_t>(samplesPerPass_) * channels_ * 2);
  inputHold_.clear();
  resamplePhase_ = 0.0;
  return true;
}

void LiveMp3Encoder::close() {
  if (encoder_) {
    shine_close(static_cast<shine_t>(encoder_));
    encoder_ = nullptr;
  }
  pending_.clear();
}

bool LiveMp3Encoder::encodePending(std::vector<std::uint8_t>* out) {
  if (!encoder_ || !out) return false;
  const std::size_t need = static_cast<std::size_t>(samplesPerPass_) * channels_;
  while (pending_.size() >= need) {
    int written = 0;
    unsigned char* frame = shine_encode_buffer_interleaved(static_cast<shine_t>(encoder_),
                                                           pending_.data(), &written);
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(need));
    if (frame && written > 0) out->insert(out->end(), frame, frame + written);
  }
  return true;
}

bool LiveMp3Encoder::encodeInterleaved(const std::int16_t* samples, std::size_t count,
                                       std::vector<std::uint8_t>* out) {
  if (!encoder_ || !out) return false;
  if (!samples || count == 0) return encodePending(out);

  if (inputRate_ == encodeRate_) {
    pending_.insert(pending_.end(), samples, samples + count);
    return encodePending(out);
  }

  inputHold_.insert(inputHold_.end(), samples, samples + count);
  const std::size_t holdFrames = inputHold_.size() / channels_;
  const double step = static_cast<double>(inputRate_) / static_cast<double>(encodeRate_);
  while (resamplePhase_ + 1.0 < static_cast<double>(holdFrames)) {
    const std::size_t i0 = static_cast<std::size_t>(resamplePhase_);
    const float frac = static_cast<float>(resamplePhase_ - static_cast<double>(i0));
    for (std::uint16_t ch = 0; ch < channels_; ++ch) {
      const float a = inputHold_[i0 * channels_ + ch];
      const float b = inputHold_[(i0 + 1) * channels_ + ch];
      pending_.push_back(static_cast<std::int16_t>(a + (b - a) * frac));
    }
    resamplePhase_ += step;
  }
  const auto consumed = static_cast<std::size_t>(resamplePhase_);
  if (consumed > 0 && consumed < holdFrames) {
    inputHold_.erase(inputHold_.begin(),
                     inputHold_.begin() + static_cast<std::ptrdiff_t>(consumed * channels_));
    resamplePhase_ -= static_cast<double>(consumed);
  }
  return encodePending(out);
}
