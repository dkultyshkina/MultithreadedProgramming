#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

struct FutureStateBase {
  mutable std::mutex mtx;
  std::condition_variable cv;
  bool ready = false;
  bool retrieved = false;
  std::exception_ptr exception;

  void set_exception(std::exception_ptr e) {
    std::lock_guard<std::mutex> lock(mtx);
    if (ready) {
      throw std::runtime_error("Значение уже установлено");
    }
    exception = std::move(e);
    ready = true;
    cv.notify_all();
  }

protected:
  void wait_until_ready(std::unique_lock<std::mutex> &lock) {
    cv.wait(lock, [this] { return ready; });
  }
};

template <typename T> struct FutureState : FutureStateBase {
  T value;

  void set_value(T val) {
    std::lock_guard<std::mutex> lock(mtx);
    if (ready) {
      throw std::runtime_error("Значение уже установлено");
    }
    value = std::move(val);
    ready = true;
    cv.notify_all();
  }

  T get() {
    std::unique_lock<std::mutex> lock(mtx);
    wait_until_ready(lock);
    if (retrieved) {
      throw std::runtime_error("Future уже получено");
    }
    retrieved = true;
    if (exception) {
      std::rethrow_exception(exception);
    }
    return std::move(value);
  }
};

template <> struct FutureState<void> : FutureStateBase {
  void set_value() {
    std::lock_guard<std::mutex> lock(mtx);
    if (ready) {
      throw std::runtime_error("Значение уже установлено");
    }
    ready = true;
    cv.notify_all();
  }

  void get() {
    std::unique_lock<std::mutex> lock(mtx);
    wait_until_ready(lock);
    if (retrieved) {
      throw std::runtime_error("Future уже получено");
    }
    retrieved = true;
    if (exception) {
      std::rethrow_exception(exception);
    }
  }
};

template <typename T> class Future {
private:
  std::shared_ptr<FutureState<T>> state;

public:
  Future() = default;
  explicit Future(std::shared_ptr<FutureState<T>> s) : state(std::move(s)) {}

  Future(const Future &) = delete;
  Future &operator=(const Future &) = delete;
  Future(Future &&) noexcept = default;
  Future &operator=(Future &&) noexcept = default;

  bool valid() const { return state != nullptr; }

  bool is_ready() const {
    if (!state) {
      return false;
    }
    std::lock_guard<std::mutex> lock(state->mtx);
    return state->ready;
  }

  void wait() const {
    if (!state) {
      throw std::runtime_error("Нет shared state");
    }
    std::unique_lock<std::mutex> lock(state->mtx);
    state->cv.wait(lock, [s = state] { return s->ready; });
  }

  T get() {
    if (!state) {
      throw std::runtime_error("Нет shared state");
    }
    return state->get();
  }
};

class ThreadPool {
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  mutable std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop = false;
  size_t thread_count;

public:
  explicit ThreadPool(size_t threads_count) : thread_count(threads_count) {
    if (thread_count == 0) {
      thread_count = 1;
    }

    try {
      for (size_t i = 0; i < thread_count; ++i) {
        workers.emplace_back([this] {
          while (true) {
            std::function<void()> task;
            {
              std::unique_lock<std::mutex> lock(queue_mutex);
              condition.wait(lock, [this] { return stop || !tasks.empty(); });
              if (stop && tasks.empty()) {
                return;
              }
              task = std::move(tasks.front());
              tasks.pop();
            }
            if (task) {
              task();
            }
          }
        });
      }
    } catch (...) {
      {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
      }
      condition.notify_all();
      for (auto &worker : workers) {
        if (worker.joinable()) {
          worker.join();
        }
      }
      throw;
    }
  }

  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      stop = true;
    }
    condition.notify_all();
    for (auto &worker : workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  size_t threadsCount() const { return thread_count; }

  size_t tasksPending() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return tasks.size();
  }

  template <typename F, typename... Args>
  auto Submit(F &&f, Args &&...args)
      -> Future<std::invoke_result_t<F, Args...>> {
    using ReturnType = std::invoke_result_t<F, Args...>;
    auto state = std::make_shared<FutureState<ReturnType>>();

    auto task = [f = std::forward<F>(f), ... args = std::forward<Args>(args),
                 state]() mutable {
      try {
        if constexpr (std::is_void_v<ReturnType>) {
          std::invoke(f, args...);
          state->set_value();
        } else {
          state->set_value(std::invoke(f, args...));
        }
      } catch (...) {
        try {
          state->set_exception(std::current_exception());
        } catch (...) {
          std::terminate();
        }
      }
    };
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      if (stop) {
        throw std::runtime_error("Submit на остановленном ThreadPool");
      }
      tasks.push(std::move(task));
    }
    condition.notify_one();
    return Future<ReturnType>(state);
  }
};