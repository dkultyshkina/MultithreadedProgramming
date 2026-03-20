#include "queue.h"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  const std::string name = "/my_queue";
  shm_unlink(name.c_str());
  std::cout << "\n PRODUCER \n" << std::endl;
  try {
    ProducerNode producer(name, 8);
    std::cout << "[OK] Producer создал очередь" << std::endl;
    for (int i = 0; i < 4; i++) {
      std::string text = "Сообщение " + std::to_string(i + 1);
      std::span<const std::byte> data{
          reinterpret_cast<const std::byte *>(text.data()), text.size()};
      if (producer.push(1, data)) {
        std::cout << "Отправлено " << text << std::endl;
      } else {
        std::cout << "[Ошибка] " << text << std::endl;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    std::string cmd = "STOP";
    std::span<const std::byte> cmd_data{
        reinterpret_cast<const std::byte *>(cmd.data()), cmd.size()};

    if (producer.push(2, cmd_data)) {
      std::cout << "Отправлено " << cmd << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    producer.cleanup();
  } catch (const std::exception &e) {
    std::cerr << "Ошибка: " << e.what() << std::endl;
    return 1;
  }
  std::cout << "\n PRODUCER закончился \n" << std::endl;
  return 0;
}