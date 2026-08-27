#pragma once

#include <cstdint>

enum class VoxAction {
  None,
  Start,
  Resume,
  Pause,
  Finish
};

class VoxGate {
 public:
  explicit VoxGate(std::uint64_t holdFrames) : holdFrames_(holdFrames) {}

  VoxAction update(bool signalAboveThreshold, std::uint64_t frameCount,
                   bool sessionOpen, bool appendSession) {
    if (!active_) {
      if (!signalAboveThreshold) return VoxAction::None;
      active_ = true;
      quietFrames_ = 0;
      return sessionOpen ? VoxAction::Resume : VoxAction::Start;
    }

    if (signalAboveThreshold) {
      quietFrames_ = 0;
      return VoxAction::None;
    }

    quietFrames_ += frameCount;
    if (quietFrames_ < holdFrames_) return VoxAction::None;
    active_ = false;
    quietFrames_ = 0;
    return appendSession ? VoxAction::Pause : VoxAction::Finish;
  }

  void activate() {
    active_ = true;
    quietFrames_ = 0;
  }

  void reset() {
    active_ = false;
    quietFrames_ = 0;
  }

  bool active() const { return active_; }

 private:
  std::uint64_t holdFrames_;
  std::uint64_t quietFrames_ = 0;
  bool active_ = false;
};
