#include "queue.h"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  const std::string name = "/my_queue";
  std::cout << "\n CONSUMER \n" << std::endl;
  int attempts = 0;
  bool connected = false;
  while (attempts < 10 && !connected) {
    try {
      ConsumerNode consumer(name);
      std::cout << "[OK] Consumer присоединился к очереди" << std::endl;
      connected = true;
      int received = 0;
      while (received < 2) {
        auto result = consumer.pop(1);
        if (result) {
          std::string msg(reinterpret_cast<const char *>(result->data()),
                          result->size());
          std::cout << "[Получено] " << msg << std::endl;
          received++;
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
      }
      auto cmd = consumer.pop(2);
      if (cmd) {
        std::string msg(reinterpret_cast<const char *>(cmd->data()),
                        cmd->size());
        std::cout << "[Получено] " << msg << std::endl;
      }
      break;
    } catch (const std::exception &e) {
      attempts++;
      std::cout << "Ждем... (" << attempts << "/10)" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
  if (!connected) {
    std::cerr << "Ошибка" << std::endl;
    return 1;
  }
  std::cout << "\n CONSUMER закончился \n" << std::endl;
  shm_unlink(name.c_str());
  return 0;
}