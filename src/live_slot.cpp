#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "live_slot.h"

#include <algorithm>

LiveSlotGate::LiveSlotGate() { InitializeCriticalSection(&lock_); }

LiveSlotGate::~LiveSlotGate() {
  shutdown();
  const DWORD started = GetTickCount();
  while (inFlight_.load() > 0 && GetTickCount() - started < 5000) Sleep(15);
  DeleteCriticalSection(&lock_);
}

int LiveSlotGate::clampLimit(int limit) {
  if (limit < kMinLimit) return kMinLimit;
  if (limit > kMaxLimit) return kMaxLimit;
  return limit;
}

void LiveSlotGate::promoteWaitersLocked() {
  while (!waiters_.empty() && active_ < limit_) {
    Waiter* waiter = waiters_.front();
    waiters_.erase(waiters_.begin());
    ++active_;
    waiter->granted = true;
    SetEvent(waiter->event);
  }
}

bool LiveSlotGate::clientDisconnected(std::uintptr_t clientSocket) {
  if (clientSocket == static_cast<std::uintptr_t>(-1)) return false;
  const SOCKET socket = static_cast<SOCKET>(clientSocket);
  fd_set readable{};
  FD_ZERO(&readable);
  FD_SET(socket, &readable);
  timeval timeout{};
  const int ready = select(0, &readable, nullptr, nullptr, &timeout);
  if (ready <= 0) return false;
  char peek = 0;
  const int got = recv(socket, &peek, 1, MSG_PEEK);
  if (got == 0) return true;
  if (got < 0) {
    const int error = WSAGetLastError();
    return error != WSAEWOULDBLOCK && error != WSAETIMEDOUT && error != WSAEINTR;
  }
  return false;
}

void LiveSlotGate::setLimit(int limit) {
  EnterCriticalSection(&lock_);
  limit_ = clampLimit(limit);
  promoteWaitersLocked();
  LeaveCriticalSection(&lock_);
}

int LiveSlotGate::limit() const {
  EnterCriticalSection(&lock_);
  const int value = limit_;
  LeaveCriticalSection(&lock_);
  return value;
}

int LiveSlotGate::active() const {
  EnterCriticalSection(&lock_);
  const int value = active_;
  LeaveCriticalSection(&lock_);
  return value;
}

int LiveSlotGate::queued() const {
  EnterCriticalSection(&lock_);
  const int value = static_cast<int>(waiters_.size());
  LeaveCriticalSection(&lock_);
  return value;
}

bool LiveSlotGate::tryAcquire(DWORD waitMs, const std::atomic<bool>* stop,
                              std::uintptr_t clientSocket) {
  inFlight_.fetch_add(1);
  struct Dec {
    std::atomic<int>* value;
    ~Dec() { value->fetch_sub(1); }
  } dec{&inFlight_};

  Waiter* waiter = nullptr;
  EnterCriticalSection(&lock_);
  if (active_ < limit_) {
    ++active_;
    LeaveCriticalSection(&lock_);
    return true;
  }
  if (waitMs == 0 || waiters_.size() >= static_cast<std::size_t>(kMaxQueue)) {
    LeaveCriticalSection(&lock_);
    return false;
  }
  waiter = new Waiter();
  waiter->event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!waiter->event) {
    delete waiter;
    LeaveCriticalSection(&lock_);
    return false;
  }
  waiters_.push_back(waiter);
  LeaveCriticalSection(&lock_);

  const DWORD started = GetTickCount();
  for (;;) {
    if ((stop && stop->load()) || clientDisconnected(clientSocket)) break;
    const DWORD waited = GetTickCount() - started;
    if (waitMs != INFINITE && waited >= waitMs) break;
    DWORD slice = 200;
    if (waitMs != INFINITE) {
      const DWORD remaining = waitMs - waited;
      if (remaining < slice) slice = remaining ? remaining : 1;
    }
    WaitForSingleObject(waiter->event, slice);

    EnterCriticalSection(&lock_);
    const bool granted = waiter->granted;
    LeaveCriticalSection(&lock_);
    if (granted) {
      CloseHandle(waiter->event);
      delete waiter;
      return true;
    }
  }

  EnterCriticalSection(&lock_);
  if (waiter->granted) {
    LeaveCriticalSection(&lock_);
    CloseHandle(waiter->event);
    delete waiter;
    return true;
  }
  waiters_.erase(std::remove(waiters_.begin(), waiters_.end(), waiter), waiters_.end());
  LeaveCriticalSection(&lock_);
  CloseHandle(waiter->event);
  delete waiter;
  return false;
}

void LiveSlotGate::release() {
  EnterCriticalSection(&lock_);
  if (active_ > 0) --active_;
  promoteWaitersLocked();
  LeaveCriticalSection(&lock_);
}

void LiveSlotGate::shutdown() {
  std::vector<Waiter*> waiting;
  EnterCriticalSection(&lock_);
  waiting.swap(waiters_);
  LeaveCriticalSection(&lock_);
  for (Waiter* waiter : waiting) {
    if (waiter && waiter->event) SetEvent(waiter->event);
  }
}
