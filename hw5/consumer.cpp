#include "queue.h"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>

int main() {
  const std::string name = "/my_queue";
  std::cout << "\n CONSUMER \n" << std::endl;
  int attempts = 0;
  bool connected = false;
  while (attempts < 50 && !connected) {
    try {
      ConsumerNode consumer(name);
      std::cout << "[OK] Consumer присоединился к очереди" << std::endl;
      connected = true;

      constexpr int kPayloadMessages = 3;
      int received = 0;
      while (received < kPayloadMessages) {
        auto result = consumer.pop(1);
        if (result) {
          if (result->size() > 128) {
            std::cout << "[Получено] Большое сообщение: " << result->size()
                      << " байт" << std::endl;
          } else {
            std::string msg(reinterpret_cast<const char *>(result->data()),
                            result->size());
            std::cout << "[Получено] " << msg << std::endl;
          }
          received++;
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
      }
      while (true) {
        auto cmd = consumer.pop(2);
        if (cmd) {
          std::string msg(reinterpret_cast<const char *>(cmd->data()),
                          cmd->size());
          std::cout << "[Получено] " << msg << std::endl;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      break;
    } catch (const std::exception &e) {
      attempts++;
      std::cout << "Ждём... (" << attempts << "/50): " << e.what() << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  if (!connected) {
    std::cerr << "Не удалось подключиться к очереди" << std::endl;
    return 1;
  }
  std::cout << "\n CONSUMER закончился \n" << std::endl;
  if (shm_unlink(name.c_str()) == -1 && errno != ENOENT) {
    std::perror("shm_unlink");
  }
  return 0;
}
