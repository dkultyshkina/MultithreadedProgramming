#include "futex_mutex.hpp"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

TEST(FutexMutexTest, BasicLockUnlock) {
  FutexMutex mutex;
  EXPECT_NO_THROW(mutex.lock());
  EXPECT_NO_THROW(mutex.unlock());
}

TEST(FutexMutexTest, TryLockSuccess) {
  FutexMutex mutex;
  EXPECT_TRUE(mutex.try_lock());
  mutex.unlock();
}

TEST(FutexMutexTest, TryLockFailure) {
  FutexMutex mutex;
  EXPECT_TRUE(mutex.try_lock());
  EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

TEST(FutexMutexTest, TryLockAfterUnlock) {
  FutexMutex mutex;
  EXPECT_TRUE(mutex.try_lock());
  mutex.unlock();
  EXPECT_TRUE(mutex.try_lock());
  mutex.unlock();
}

TEST(FutexMutexTest, DoubleUnlockIsSafe) {
  FutexMutex mutex;
  mutex.lock();
  mutex.unlock();
  EXPECT_NO_THROW(mutex.unlock());
}

TEST(FutexMutexTest, StateTransition) {
  FutexMutex mutex;
  EXPECT_TRUE(mutex.try_lock());
  std::thread t([&]() { EXPECT_FALSE(mutex.try_lock()); });
  t.join();
  mutex.unlock();
  EXPECT_TRUE(mutex.try_lock());
  mutex.unlock();
}

TEST(FutexMutexTest, MultipleThreadsIncrement) {
  FutexMutex mutex;
  int shared_counter = 0;
  const int threads_count = 8;
  const int increments_per_thread = 10000;
  std::vector<std::thread> workers;
  for (int i = 0; i < threads_count; ++i) {
    workers.emplace_back([&]() {
      for (int j = 0; j < increments_per_thread; ++j) {
        mutex.lock();
        ++shared_counter;
        mutex.unlock();
      }
    });
  }
  for (auto &t : workers) {
    t.join();
  }
  EXPECT_EQ(shared_counter, threads_count * increments_per_thread);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}