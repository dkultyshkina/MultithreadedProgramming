#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

template <class T> class BufferedChannel {
public:
  explicit BufferedChannel(int size)
      : capacity_(size >= 0 ? size : 0), closed_(false) {}

  void Send(const T &value) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_full_.wait(
        lock, [this]() { return queue_.size() < capacity_ || closed_.load(); });
    if (closed_.load()) {
      throw std::runtime_error("Закрытый канал");
    }
    queue_.push(value);
    not_empty_.notify_one();
  }

  std::optional<T> Recv() {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock,
                    [this]() { return !queue_.empty() || closed_.load(); });
    if (queue_.empty()) {
      return std::nullopt;
    }
    T value = queue_.front();
    queue_.pop();
    not_full_.notify_one();
    return value;
  }

  void Close() {
    std::unique_lock<std::mutex> lock(mutex_);
    closed_.store(true);
    not_empty_.notify_all();
    not_full_.notify_all();
  }

private:
  size_t capacity_;
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  std::atomic<bool> closed_;
};