#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <future>
#include <thread>
#include <vector>

template <typename T>
void ApplyFunction(std::vector<T> &data,
                   const std::function<void(T &)> &transform,
                   const int threadCount = 1) {
  if (data.empty() || threadCount <= 0) {
    return;
  }
  int numThreads = std::min(threadCount, static_cast<int>(data.size()));
  size_t blockSize = data.size() / numThreads;
  size_t remainder = data.size() % numThreads;
  std::vector<std::thread> threads;
  size_t start = 0;
  for (int i = 0; i < numThreads; ++i) {
    size_t currentBlockSize =
        blockSize + (i < static_cast<int>(remainder) ? 1 : 0);
    if (currentBlockSize > 0) {
      threads.emplace_back([&data, transform, start, currentBlockSize]() {
        for (size_t j = start; j < start + currentBlockSize; ++j) {
          transform(data[j]);
        }
      });
    }
    start += currentBlockSize;
  }
  for (auto &thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}