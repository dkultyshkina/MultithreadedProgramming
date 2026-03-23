#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

constexpr int PROTO_VER = 1;
constexpr uint32_t MAGIC_NUM = 0xCAFEBABE;

struct MsgHeader {
  int32_t type{0};
  int32_t len{0};
};

struct ControlBlock {
  std::atomic<uint32_t> magic{0};
  std::atomic<int> version{0};
  std::atomic<int> capacity{0};
  std::atomic<uint64_t> write_reserve{0};
  std::atomic<uint64_t> write_ready{0};
  std::atomic<uint64_t> read_pos{0};
};

void write(std::byte *buffer, std::size_t cap, std::size_t offset,
           const void *src, std::size_t size) {
  if (size == 0) {
    return;
  }
  const std::size_t begin = offset % cap;
  const std::size_t first = std::min(size, cap - begin);
  std::memcpy(buffer + begin, src, first);
  if (first < size) {
    std::memcpy(buffer, static_cast<const std::byte *>(src) + first,
                size - first);
  }
}

void read(const std::byte *buffer, std::size_t cap, std::size_t offset,
          void *dst, std::size_t size) {
  if (size == 0) {
    return;
  }
  const std::size_t begin = offset % cap;
  const std::size_t first = std::min(size, cap - begin);
  std::memcpy(dst, buffer + begin, first);
  if (first < size) {
    std::memcpy(static_cast<std::byte *>(dst) + first, buffer, size - first);
  }
}

class ProducerNode {
public:
  ProducerNode(const std::string &path, int capacity)
      : ctrl_(nullptr), buffer_(nullptr), capacity_(capacity), map_size_(0) {
    if (capacity <= static_cast<int>(sizeof(MsgHeader))) {
      throw std::runtime_error("capacity должен быть > sizeof(MsgHeader)");
    }
    const std::size_t total =
        sizeof(ControlBlock) + static_cast<std::size_t>(capacity);
    int fd = shm_open(path.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
      throw std::runtime_error("shm_open не сработал: " + path);
    }
    struct stat st {};
    if (fstat(fd, &st) == -1) {
      close(fd);
      throw std::runtime_error("fstat не сработал");
    }
    if (static_cast<std::size_t>(st.st_size) < total) {
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
    buffer_ = reinterpret_cast<std::byte *>(reinterpret_cast<char *>(ctrl_) +
                                            sizeof(ControlBlock));
    map_size_ = total;
    uint32_t expected_magic = 0;
    const bool first = ctrl_->magic.compare_exchange_strong(
        expected_magic, MAGIC_NUM, std::memory_order_acq_rel,
        std::memory_order_relaxed);
    if (first) {
      ctrl_->version.store(PROTO_VER, std::memory_order_relaxed);
      ctrl_->capacity.store(capacity, std::memory_order_relaxed);
      ctrl_->write_reserve.store(0, std::memory_order_relaxed);
      ctrl_->write_ready.store(0, std::memory_order_relaxed);
      ctrl_->read_pos.store(0, std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_release);
    } else {
      if (ctrl_->magic.load(std::memory_order_acquire) != MAGIC_NUM ||
          ctrl_->version.load(std::memory_order_acquire) != PROTO_VER) {
        munmap(ptr, total);
        throw std::runtime_error("protocol не совпадает");
      }
      const int existing = ctrl_->capacity.load(std::memory_order_acquire);
      if (existing != capacity) {
        munmap(ptr, total);
        throw std::runtime_error("capacity не совпадает");
      }
      capacity_ = existing;
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

  bool push(int32_t msg_type, std::span<const std::byte> message_data) {
    if (message_data.size() >
        static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
      return false;
    }
    const std::size_t message_size = message_data.size();
    const std::size_t message_total_size = sizeof(MsgHeader) + message_size;
    if (message_total_size > static_cast<std::size_t>(capacity_)) {
      return false;
    }
    uint64_t reserved_start = 0;
    while (true) {
      reserved_start = ctrl_->write_reserve.load(std::memory_order_relaxed);
      const uint64_t consumed = ctrl_->read_pos.load(std::memory_order_acquire);
      if (reserved_start - consumed + message_total_size >
          static_cast<uint64_t>(capacity_)) {
        return false;
      }
      if (ctrl_->write_reserve.compare_exchange_weak(
              reserved_start, reserved_start + message_total_size,
              std::memory_order_acq_rel, std::memory_order_relaxed)) {
        break;
      }
    }
    const MsgHeader header{msg_type, static_cast<int32_t>(message_size)};
    write(buffer_, static_cast<std::size_t>(capacity_),
          static_cast<std::size_t>(reserved_start), &header, sizeof(header));
    write(buffer_, static_cast<std::size_t>(capacity_),
          static_cast<std::size_t>(reserved_start + sizeof(header)),
          message_data.data(), message_size);
    uint64_t expected = reserved_start;
    while (!ctrl_->write_ready.compare_exchange_weak(
        expected, reserved_start + message_total_size,
        std::memory_order_release, std::memory_order_acquire)) {
      expected = reserved_start;
    }
    return true;
  }

private:
  ControlBlock *ctrl_;
  std::byte *buffer_;
  int capacity_;
  std::size_t map_size_;
};

class ConsumerNode {
public:
  explicit ConsumerNode(const std::string &path)
      : ctrl_(nullptr), buffer_(nullptr), capacity_(0), map_size_(0) {
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
    capacity_ = ctrl_->capacity.load(std::memory_order_acquire);
    const std::size_t expected_size =
        sizeof(ControlBlock) + static_cast<std::size_t>(capacity_);
    if (map_size_ < expected_size ||
        capacity_ <= static_cast<int>(sizeof(MsgHeader))) {
      munmap(ptr, map_size_);
      throw std::runtime_error("Невалидный размер segment");
    }
    buffer_ = reinterpret_cast<std::byte *>(reinterpret_cast<char *>(ctrl_) +
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
      const uint64_t read_pos = ctrl_->read_pos.load(std::memory_order_relaxed);
      const uint64_t write_ready =
          ctrl_->write_ready.load(std::memory_order_acquire);
      if (read_pos == write_ready) {
        return std::nullopt;
      }
      if (write_ready - read_pos < sizeof(MsgHeader)) {
        return std::nullopt;
      }
      MsgHeader header{};
      read(buffer_, static_cast<std::size_t>(capacity_),
           static_cast<std::size_t>(read_pos), &header, sizeof(header));
      const std::optional<std::size_t> message_total_size =
          validate_header(header, read_pos, write_ready);
      if (!message_total_size.has_value()) {
        ctrl_->read_pos.store(write_ready, std::memory_order_release);
        return std::nullopt;
      }
      const std::size_t message_size = static_cast<std::size_t>(header.len);
      const bool match = (header.type == expected_type);
      std::vector<std::byte> result;
      if (match) {
        result.resize(message_size);
        read(buffer_, static_cast<std::size_t>(capacity_),
             static_cast<std::size_t>(read_pos + sizeof(MsgHeader)),
             result.data(), message_size);
      }
      ctrl_->read_pos.store(read_pos + *message_total_size,
                            std::memory_order_release);
      if (match) {
        return result;
      }
    }
  }

private:
  std::optional<std::size_t> validate_header(const MsgHeader &header,
                                             uint64_t read_pos,
                                             uint64_t write_ready) const {
    if (header.len < 0) {
      return std::nullopt;
    }
    const std::size_t message_size = static_cast<std::size_t>(header.len);
    const std::size_t message_total_size = sizeof(MsgHeader) + message_size;
    if (message_total_size > static_cast<std::size_t>(capacity_) ||
        read_pos + message_total_size > write_ready) {
      return std::nullopt;
    }
    return message_total_size;
  }

  ControlBlock *ctrl_;
  std::byte *buffer_;
  int capacity_;
  std::size_t map_size_;
};
