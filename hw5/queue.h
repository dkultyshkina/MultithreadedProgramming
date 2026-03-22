#include <atomic>
#include <cstddef>
#include <cstdint>
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
constexpr int MAX_MSG_SIZE = 4096;
constexpr uint32_t MAGIC_NUM = 0xCAFEBABE;

struct Message {
  std::atomic<uint32_t> ready{0};
  std::atomic<int> type{-1};
  std::atomic<int> len{0};
  char data[MAX_MSG_SIZE];
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
      : ctrl_(nullptr), msgs_(nullptr), slot_cnt_(capacity), map_size_(0) {
    if (capacity < 2) {
      throw std::runtime_error("capacity должен быть >= 2");
    }
    const std::size_t total =
        sizeof(ControlBlock) +
        static_cast<std::size_t>(capacity) * sizeof(Message);
    int fd = shm_open(path.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
      throw std::runtime_error("shm_open не сработал: " + path);
    }
    struct stat st {};
    if (fstat(fd, &st) == -1) {
      close(fd);
      throw std::runtime_error("fstat не сработал");
    }
    const bool need_init = static_cast<std::size_t>(st.st_size) < total;
    if (need_init) {
      if (ftruncate(fd, static_cast<off_t>(total)) == -1) {
        close(fd);
        throw std::runtime_error("ftruncate не сработал");
      }
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
    uint32_t expected_magic = 0;
    const bool first = ctrl_->magic.compare_exchange_strong(
        expected_magic, MAGIC_NUM, std::memory_order_acq_rel,
        std::memory_order_relaxed);
    if (first) {
      ctrl_->version.store(PROTO_VER, std::memory_order_relaxed);
      ctrl_->slots.store(capacity, std::memory_order_relaxed);
      ctrl_->prod_pos.store(0, std::memory_order_relaxed);
      ctrl_->cons_pos.store(0, std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_release);
      for (int i = 0; i < capacity; ++i) {
        msgs_[i].ready.store(0, std::memory_order_relaxed);
        msgs_[i].type.store(-1, std::memory_order_relaxed);
        msgs_[i].len.store(0, std::memory_order_relaxed);
      }
    } else {
      if (ctrl_->magic.load(std::memory_order_acquire) != MAGIC_NUM ||
          ctrl_->version.load(std::memory_order_acquire) != PROTO_VER) {
        munmap(ptr, total);
        throw std::runtime_error("protocol не совпадает");
      }
      const int existing = ctrl_->slots.load(std::memory_order_acquire);
      if (existing != capacity) {
        munmap(ptr, total);
        throw std::runtime_error("capacity не совпадает");
      }
      slot_cnt_ = existing;
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

  bool push(int32_t msg_type, std::span<const std::byte> payload) {
    if (payload.size() > static_cast<std::size_t>(MAX_MSG_SIZE)) {
      return false;
    }
    uint32_t pos = ctrl_->prod_pos.load(std::memory_order_relaxed);
    uint32_t next_pos = 0;
    const int cap = slot_cnt_;
    do {
      const uint32_t cons = ctrl_->cons_pos.load(std::memory_order_acquire);
      const uint32_t used = (pos - cons + static_cast<uint32_t>(cap)) %
                            static_cast<uint32_t>(cap);
      if (used >= static_cast<uint32_t>(cap) - 1U) {
        return false;
      }
      next_pos = (pos + 1U) % static_cast<uint32_t>(cap);
    } while (!ctrl_->prod_pos.compare_exchange_strong(
        pos, next_pos, std::memory_order_acq_rel, std::memory_order_relaxed));
    Message &slot = msgs_[pos];
    slot.type.store(msg_type, std::memory_order_relaxed);
    slot.len.store(static_cast<int>(payload.size()), std::memory_order_relaxed);
    if (!payload.empty()) {
      std::memcpy(slot.data, payload.data(), payload.size());
    }
    slot.ready.store(1, std::memory_order_release);
    return true;
  }

private:
  ControlBlock *ctrl_;
  Message *msgs_;
  int slot_cnt_;
  std::size_t map_size_;
};

class ConsumerNode {
public:
  explicit ConsumerNode(const std::string &path)
      : ctrl_(nullptr), msgs_(nullptr), slot_cnt_(0), map_size_(0) {
    int fd = shm_open(path.c_str(), O_RDWR, 0666);
    if (fd == -1) {
      throw std::runtime_error("shm_open не сработал: " + path);
    }
    struct stat st {};
    if (fstat(fd, &st) == -1) {
      close(fd);
      throw std::runtime_error("fstat не сработал");
    }
    void *ptr = mmap(nullptr, static_cast<std::size_t>(st.st_size),
                     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
      close(fd);
      throw std::runtime_error("mmap не сработал");
    }
    close(fd);
    ctrl_ = static_cast<ControlBlock *>(ptr);
    map_size_ = static_cast<std::size_t>(st.st_size);
    if (ctrl_->magic.load(std::memory_order_acquire) != MAGIC_NUM ||
        ctrl_->version.load(std::memory_order_acquire) != PROTO_VER) {
      munmap(ptr, map_size_);
      throw std::runtime_error("protocol не совпадает");
    }
    slot_cnt_ = ctrl_->slots.load(std::memory_order_acquire);
    const std::size_t expected_size =
        sizeof(ControlBlock) +
        static_cast<std::size_t>(slot_cnt_) * sizeof(Message);
    if (map_size_ < expected_size || slot_cnt_ < 2) {
      munmap(ptr, map_size_);
      throw std::runtime_error("Невалидный размео segment");
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

  std::optional<std::vector<std::byte>> pop(int32_t expected_type) {
    while (true) {
      const uint32_t pos = ctrl_->cons_pos.load(std::memory_order_relaxed);
      const uint32_t prod = ctrl_->prod_pos.load(std::memory_order_acquire);
      if (pos == prod) {
        return std::nullopt;
      }
      Message &slot = msgs_[pos];
      if (slot.ready.load(std::memory_order_acquire) == 0) {
        return std::nullopt;
      }
      const int t = slot.type.load(std::memory_order_relaxed);
      const int len = slot.len.load(std::memory_order_relaxed);
      if (len < 0 || len > MAX_MSG_SIZE) {
        release_slot(slot, pos);
        continue;
      }
      const bool match = (t == expected_type);
      std::vector<std::byte> result;
      if (match) {
        result.resize(static_cast<std::size_t>(len));
        if (len > 0) {
          std::memcpy(result.data(), slot.data, static_cast<std::size_t>(len));
        }
      }
      release_slot(slot, pos);
      if (match) {
        return result;
      }
    }
  }

private:
  void release_slot(Message &slot, uint32_t current_pos) {
    slot.ready.store(0, std::memory_order_relaxed);
    slot.type.store(-1, std::memory_order_relaxed);
    slot.len.store(0, std::memory_order_relaxed);
    const uint32_t next = (current_pos + 1U) % static_cast<uint32_t>(slot_cnt_);
    ctrl_->cons_pos.store(next, std::memory_order_release);
  }

  ControlBlock *ctrl_;
  Message *msgs_;
  int slot_cnt_;
  std::size_t map_size_;
};
