#include "thread_pool.hpp"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>
#include <vector>

TEST(ThreadPoolTest, Creation) {
  ThreadPool pool(4);
  EXPECT_EQ(pool.threadsCount(), 4);
  EXPECT_EQ(pool.tasksPending(), 0);
}

TEST(ThreadPoolTest, CreationWithZero) {
  ThreadPool pool(0);
  EXPECT_GT(pool.threadsCount(), 0);
}

TEST(ThreadPoolTest, SimpleTask) {
  ThreadPool pool(2);
  auto future = pool.Submit([]() { return 73; });
  EXPECT_EQ(future.get(), 73);
}

TEST(ThreadPoolTest, TaskWithArgs) {
  ThreadPool pool(2);
  auto future = pool.Submit([](int a, int b) { return a + b; }, 5, 3);
  EXPECT_EQ(future.get(), 8);
}

TEST(ThreadPoolTest, VoidTask) {
  ThreadPool pool(2);
  std::atomic<bool> executed{false};
  auto future = pool.Submit([&executed]() { executed = true; });
  future.get();
  EXPECT_TRUE(executed);
}

TEST(ThreadPoolTest, MultipleTasks) {
  ThreadPool pool(4);
  std::vector<Future<int>> futures;
  const int taskCount = 100;
  for (int i = 0; i < taskCount; ++i) {
    futures.push_back(pool.Submit([i]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      return i * i;
    }));
  }
  for (int i = 0; i < taskCount; ++i) {
    EXPECT_EQ(futures[i].get(), i * i);
  }
}

TEST(ThreadPoolTest, ExceptionHandling) {
  ThreadPool pool(2);
  auto future = pool.Submit([]() {
    throw std::runtime_error("Test exception");
    return 73;
  });
  EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(ThreadPoolTest, FutureWait) {
  ThreadPool pool(2);
  auto future = pool.Submit([]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 100;
  });
  EXPECT_FALSE(future.is_ready());
  future.wait();
  EXPECT_TRUE(future.is_ready());
  EXPECT_EQ(future.get(), 100);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}