#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include <windows.h>

// Caps concurrent live listeners and parks overflow in FIFO order.
// Lowering the cap never kicks anyone already listening.
class LiveSlotGate {
 public:
  static constexpr int kDefaultLimit = 5;
  static constexpr int kMinLimit = 1;
  static constexpr int kMaxLimit = 64;
  static constexpr int kMaxQueue = 64;

  LiveSlotGate();
  ~LiveSlotGate();

  LiveSlotGate(const LiveSlotGate&) = delete;
  LiveSlotGate& operator=(const LiveSlotGate&) = delete;

  static int clampLimit(int limit);

  void setLimit(int limit);
  int limit() const;
  int active() const;
  int queued() const;

  bool tryAcquire(DWORD waitMs, const std::atomic<bool>* stop = nullptr,
                  std::uintptr_t clientSocket = static_cast<std::uintptr_t>(-1));
  void release();
  void shutdown();

 private:
  struct Waiter {
    HANDLE event = nullptr;
    bool granted = false;
  };

  void promoteWaitersLocked();
  static bool clientDisconnected(std::uintptr_t clientSocket);

  mutable CRITICAL_SECTION lock_{};
  int limit_ = kDefaultLimit;
  int active_ = 0;
  std::atomic<int> inFlight_{0};
  std::vector<Waiter*> waiters_;
};
