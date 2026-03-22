#include <algorithm>
#include <coroutine>
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <vector>

template <typename T> struct generator {
  struct promise_type {
    T current_value;
    std::exception_ptr exception;

    generator get_return_object() {
      return generator(
          std::coroutine_handle<promise_type>::from_promise(*this));
    }

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(T value) {
      current_value = value;
      return {};
    }

    void unhandled_exception() { exception = std::current_exception(); }
  };

  std::coroutine_handle<promise_type> handle;

  generator(std::coroutine_handle<promise_type> h) : handle(h) {}

  ~generator() {
    if (handle)
      handle.destroy();
  }

  generator(generator &&other) noexcept : handle(other.handle) {
    other.handle = nullptr;
  }

  generator &operator=(generator &&other) noexcept {
    if (this != &other) {
      if (handle)
        handle.destroy();
      handle = other.handle;
      other.handle = nullptr;
    }
    return *this;
  }

  bool next() {
    if (!handle.done()) {
      handle.resume();
      if (handle.promise().exception) {
        std::rethrow_exception(handle.promise().exception);
      }
    }
    return !handle.done();
  }

  T value() const {
    if (handle.done()) {
      throw std::logic_error("Произошел вызов завершенного генератора");
    }
    return handle.promise().current_value;
  }

  bool done() const { return handle.done(); }
};

class Graph {
  std::vector<std::vector<int>> adj;

public:
  explicit Graph(int n) : adj(n) {}

  void add_edge(int u, int v) {
    adj[u].push_back(v);
    adj[v].push_back(u);
  }

  int size() const { return static_cast<int>(adj.size()); }

  const std::vector<int> &neighbors(int v) const { return adj[v]; }
};

generator<int> dfs_cooperative(std::shared_ptr<Graph> g, 
                               std::shared_ptr<std::vector<bool>> shared_visited, 
                               int start,
                               std::string task_name) {
  if (!g || start < 0 || start >= g->size()) {
    throw std::out_of_range("Некорректные границы");
  }
  std::stack<int> s;
  s.push(start);
  std::cout << "\t" << task_name << " со стартом: " << start << std::endl;
  while (!s.empty()) {
    int u = s.top();
    s.pop();      
    if ((*shared_visited)[u]) {
      std::cout << "\t" << task_name << " Пропускаем узел: " << u
                << " (уже посещён)" << std::endl;
      continue;
    }
    (*shared_visited)[u] = true;
    std::cout << "\t" << task_name << " посетила узел: " << u << std::endl;
    co_yield u;
    for (int v : g->neighbors(u)) {
      if (!(*shared_visited)[v]) {
        s.push(v);
        std::cout << "\t" << task_name << " добавляет соседа узла " << u << ": " << v << " " << std::endl;
      }
    }

    std::cout << "\t" << task_name << " с размером стека: " << s.size() << std::endl;
  }
  std::cout << "\t" << task_name << " завершается " << std::endl;
}

struct Task {
  std::string name;
  generator<int> gen;
  bool is_finished = false;
  int priority = 0;

  Task(std::string n, generator<int> g, int p = 0)
      : name(std::move(n)), gen(std::move(g)), priority(p) {}
};

class Scheduler {
  std::list<Task> tasks;

  struct PriorityTask {
    int priority;
    typename std::list<Task>::iterator iterator;
  };

  std::vector<PriorityTask> priority_order;

  void update_priority_order() {
    priority_order.clear();
    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
      priority_order.push_back({it->priority, it});
    }
    std::sort(priority_order.begin(), priority_order.end(),
              [](const PriorityTask &a, const PriorityTask &b) {
                return a.priority > b.priority;
              });
  }

public:
  void add_task(std::string name, generator<int> gen, int priority = 0) {
    tasks.emplace_back(std::move(name), std::move(gen), priority);
    update_priority_order();
  }

  void run() {
    bool active = true;
    while (active) {
      active = false;
      bool tasks_completed = false;
      for (auto &pt : priority_order) {
        if (pt.iterator->is_finished)
          continue;
        try {
          if (pt.iterator->gen.next()) {
            active = true;
          } else {
            pt.iterator->is_finished = true;
            tasks_completed = true;
            std::cout << "Задача " << pt.iterator->name << " завершена"
                      << std::endl;
          }
        } catch (const std::exception &e) {
          std::cerr << "Ошибка в задаче \"" << pt.iterator->name
                    << "\": " << e.what() << std::endl;
          pt.iterator->is_finished = true;
          tasks_completed = true;
        }
      }
      if (tasks_completed) {
        update_priority_order();
      }
    }
  }

  size_t active_tasks_count() const {
    size_t count = 0;
    for (const auto &task : tasks) {
      if (!task.is_finished)
        ++count;
    }
    return count;
  }
};