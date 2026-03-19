#include <atomic>
#include <cerrno>
#include <cstdint>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <system_error>
#include <unistd.h>

class FutexMutex {
public:
  FutexMutex() : state_(0) {}
  FutexMutex(const FutexMutex &) = delete;
  FutexMutex &operator=(const FutexMutex &) = delete;
  FutexMutex(FutexMutex &&) = delete;
  FutexMutex &operator=(FutexMutex &&) = delete;
  ~FutexMutex() = default;

  void lock() {
    uint32_t expected = 0;
    if (state_.compare_exchange_strong(expected, 1, std::memory_order_acquire,
                                       std::memory_order_relaxed)) {
      return;
    }
    lock_slow();
  }

  bool try_lock() {
    uint32_t expected = 0;
    return state_.compare_exchange_strong(
        expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
  }

  void unlock() {
    uint32_t old_state = state_.exchange(0, std::memory_order_release);
    if (old_state == 2) {
      wake_one();
    }
  }

private:
  alignas(4) std::atomic<uint32_t> state_;

  uint32_t *native_handle() { return reinterpret_cast<uint32_t *>(&state_); }

  static int futex_wait(uint32_t *addr, uint32_t expected_val) {
    return syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, expected_val, nullptr,
                   nullptr, 0);
  }

  static int futex_wake(uint32_t *addr, int count = 1) {
    return syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr,
                   0);
  }

  void wake_one() { futex_wake(native_handle(), 1); }

  void lock_slow() {
    while (true) {
      uint32_t expected = 0;
      if (state_.compare_exchange_strong(expected, 2, std::memory_order_acquire,
                                         std::memory_order_relaxed)) {
        return;
      }
      expected = 1;
      if (state_.compare_exchange_strong(expected, 2, std::memory_order_relaxed,
                                         std::memory_order_relaxed)) {
        while (true) {
          int ret = futex_wait(native_handle(), 2);
          if (ret == 0) {
            break;
          }
          if (ret == -1) {
            if (errno == EAGAIN) {
              break;
            }
            if (errno != EINTR) {
              throw std::system_error(errno, std::generic_category(),
                                      "futex_wait failed");
            }
          }
        }
      }
    }
  }
};