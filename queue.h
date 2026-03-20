#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

constexpr int PROTO_VER = 1;
constexpr int MAX_MSG_SIZE = 256;
constexpr uint32_t MAGIC_NUM = 0xCAFEBABE;

struct Message {
  std::atomic<uint32_t> ready{0};
  std::atomic<int> type{-1};
  std::atomic<int> len{0};
  char data[MAX_MSG_SIZE];

  Message() : ready(0), type(-1), len(0) { std::memset(data, 0, sizeof(data)); }
};

struct ControlBlock {
  std::atomic<uint32_t> magic{0};
  std::atomic<int> version{0};
  std::atomic<int> slots{0};
  std::atomic<uint32_t> prod_pos{0};
  std::atomic<uint32_t> cons_pos{0};
};

class ProducerNode {
public:
  ProducerNode(const std::string &path, int capacity)
      : ctrl_(nullptr), msgs_(nullptr), slot_cnt_(capacity), path_(path),
        map_size_(0), is_owner_(false) {
    size_t total =
        sizeof(ControlBlock) + static_cast<size_t>(capacity) * sizeof(Message);
    int fd = shm_open(path.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
      throw std::runtime_error("shm_open не сработал: " + path);
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
      close(fd);
      throw std::runtime_error("fstat не сработал");
    }
    bool needs_init = (st.st_size < (long)total);
    if (needs_init) {
      if (ftruncate(fd, total) == -1) {
        close(fd);
        throw std::runtime_error("ftruncate не сработал");
      }
      is_owner_ = true;
    }
    void *ptr = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
      close(fd);
      throw std::runtime_error("mmap не сработал");
    }
    close(fd);
    ctrl_ = static_cast<ControlBlock *>(ptr);
    msgs_ = reinterpret_cast<Message *>(reinterpret_cast<char *>(ctrl_) +
                                        sizeof(ControlBlock));
    map_size_ = total;
    uint32_t expected = 0;
    bool first = ctrl_->magic.compare_exchange_strong(
        expected, MAGIC_NUM, std::memory_order_acq_rel,
        std::memory_order_relaxed);

    if (first) {
      ctrl_->version.store(PROTO_VER, std::memory_order_relaxed);
      ctrl_->slots.store(capacity, std::memory_order_relaxed);
      ctrl_->prod_pos.store(0, std::memory_order_relaxed);
      ctrl_->cons_pos.store(0, std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_release);
      for (int i = 0; i < capacity; ++i) {
        msgs_[i].ready.store(0, std::memory_order_relaxed);
        msgs_[i].type = -1;
        msgs_[i].len = 0;
      }
    } else {
      is_owner_ = false;
      if (ctrl_->magic.load(std::memory_order_acquire) != MAGIC_NUM ||
          ctrl_->version.load(std::memory_order_acquire) != PROTO_VER) {
        throw std::runtime_error("Ошибка с протоколом или с числом");
      }
      slot_cnt_ = ctrl_->slots.load(std::memory_order_acquire);
    }
  }

  ProducerNode(const ProducerNode &) = delete;
  ProducerNode &operator=(const ProducerNode &) = delete;
  ProducerNode(ProducerNode &&) = delete;
  ProducerNode &operator=(ProducerNode &&) = delete;

  ~ProducerNode() {
    if (ctrl_ && map_size_ > 0) {
      munmap(ctrl_, map_size_);
    }
  }

  bool push(int msg_type, std::span<const std::byte> payload) {
    if (payload.size() > MAX_MSG_SIZE) {
      return false;
    }
    uint32_t pos = ctrl_->prod_pos.load(std::memory_order_relaxed);
    uint32_t next;
    do {
      uint32_t cons = ctrl_->cons_pos.load(std::memory_order_acquire);
      int capacity = slot_cnt_;
      uint32_t used = (pos - cons + capacity) % capacity;
      if (used >= static_cast<uint32_t>(capacity) - 1) {
        return false;
      }
      next = (pos + 1) % capacity;
    } while (!ctrl_->prod_pos.compare_exchange_strong(
        pos, next, std::memory_order_acq_rel, std::memory_order_relaxed));
    Message *slot = &msgs_[pos];
    slot->type = msg_type;
    slot->len = static_cast<int>(payload.size());
    std::memcpy(slot->data, payload.data(), payload.size());
    slot->ready.store(1, std::memory_order_release);
    return true;
  }

  void cleanup() {
    if (is_owner_ && !path_.empty()) {
      if (shm_unlink(path_.c_str()) == -1 && errno != ENOENT) {
      }
    }
  }

private:
  ControlBlock *ctrl_;
  Message *msgs_;
  int slot_cnt_;
  std::string path_;
  size_t map_size_;
  bool is_owner_;
};

class ConsumerNode {
public:
  ConsumerNode(const std::string &path)
      : ctrl_(nullptr), msgs_(nullptr), slot_cnt_(0), map_size_(0) {
    int fd = shm_open(path.c_str(), O_RDWR, 0666);
    if (fd == -1) {
      throw std::runtime_error("shm_open не сработал: " + path);
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
      close(fd);
      throw std::runtime_error("fstat не сработал");
    }
    void *ptr =
        mmap(nullptr, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
      close(fd);
      throw std::runtime_error("mmap не сработал");
    }
    close(fd);
    ctrl_ = static_cast<ControlBlock *>(ptr);
    map_size_ = st.st_size;
    if (ctrl_->magic.load(std::memory_order_acquire) != MAGIC_NUM ||
        ctrl_->version.load(std::memory_order_acquire) != PROTO_VER) {
      ctrl_ = nullptr;
      munmap(ptr, map_size_);
      throw std::runtime_error("Проблемы с числом или с протоколом");
    }
    slot_cnt_ = ctrl_->slots.load(std::memory_order_acquire);
    size_t expected_size =
        sizeof(ControlBlock) + static_cast<size_t>(slot_cnt_) * sizeof(Message);
    if (map_size_ < expected_size) {
      ctrl_ = nullptr;
      munmap(ptr, map_size_);
      throw std::runtime_error("Неправильный размер");
    }
    msgs_ = reinterpret_cast<Message *>(reinterpret_cast<char *>(ctrl_) +
                                        sizeof(ControlBlock));
  }
  ConsumerNode(const ConsumerNode &) = delete;
  ConsumerNode &operator=(const ConsumerNode &) = delete;
  ConsumerNode(ConsumerNode &&) = delete;
  ConsumerNode &operator=(ConsumerNode &&) = delete;
  ~ConsumerNode() {
    if (ctrl_ && map_size_ > 0) {
      munmap(ctrl_, map_size_);
    }
  }

  std::optional<std::vector<std::byte>> pop(int expected_type) {
    while (true) {
      uint32_t pos = ctrl_->cons_pos.load(std::memory_order_relaxed);
      uint32_t prod = ctrl_->prod_pos.load(std::memory_order_acquire);
      if (pos == prod) {
        return std::nullopt;
      }
      Message *slot = &msgs_[pos];
      if (slot->ready.load(std::memory_order_acquire) == 0) {
        return std::nullopt;
      }
      if (slot->len <= 0 || slot->len > MAX_MSG_SIZE) {
        advance_consumer(pos);
        continue;
      }
      bool match = (slot->type == expected_type);
      std::vector<std::byte> result;
      if (match) {
        result.resize(slot->len);
        std::memcpy(result.data(), slot->data, slot->len);
      }
      slot->ready.store(0, std::memory_order_relaxed);
      slot->type = -1;
      advance_consumer(pos);
      if (match) {
        return result;
      }
    }
  }

  std::optional<std::vector<std::byte>> pop_any() {
    while (true) {
      uint32_t pos = ctrl_->cons_pos.load(std::memory_order_relaxed);
      uint32_t prod = ctrl_->prod_pos.load(std::memory_order_acquire);
      if (pos == prod) {
        return std::nullopt;
      }
      Message *slot = &msgs_[pos];
      if (slot->ready.load(std::memory_order_acquire) == 0) {
        return std::nullopt;
      }
      if (slot->len <= 0 || slot->len > MAX_MSG_SIZE) {
        advance_consumer(pos);
        continue;
      }
      std::vector<std::byte> result(slot->len);
      std::memcpy(result.data(), slot->data, slot->len);
      slot->ready.store(0, std::memory_order_relaxed);
      slot->type = -1;
      advance_consumer(pos);
      return result;
    }
  }

private:
  void advance_consumer(uint32_t current_pos) {
    uint32_t next = (current_pos + 1) % slot_cnt_;
    ctrl_->cons_pos.store(next, std::memory_order_release);
  }

  ControlBlock *ctrl_;
  Message *msgs_;
  int slot_cnt_;
  size_t map_size_;
};