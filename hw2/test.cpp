#include "apply_function.hpp"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>

TEST(ApplyFunctionTest, SimpleIncrement) {
  std::vector<int> data = {1, 2, 3, 4, 5};
  std::function<void(int &)> increment = [](int &x) { x += 1; };
  ApplyFunction(data, increment, 2);
  std::vector<int> expected = {2, 3, 4, 5, 6};
  EXPECT_EQ(data, expected);
}

TEST(ApplyFunctionTest, SingleThread) {
  std::vector<int> data = {10, 20, 30};
  std::function<void(int &)> multiply = [](int &x) { x *= 2; };
  ApplyFunction(data, multiply, 1);
  std::vector<int> expected = {20, 40, 60};
  EXPECT_EQ(data, expected);
}

TEST(ApplyFunctionTest, MoreThreadsThanElements) {
  std::vector<int> data = {100, 200};
  std::function<void(int &)> decrement = [](int &x) { x -= 1; };
  ApplyFunction(data, decrement, 5);
  std::vector<int> expected = {99, 199};
  EXPECT_EQ(data, expected);
}

TEST(ApplyFunctionTest, EmptyVector) {
  std::vector<int> data;
  std::function<void(int &)> transform = [](int &x) { x = 0; };
  EXPECT_NO_THROW(ApplyFunction(data, transform, 4));
  EXPECT_TRUE(data.empty());
}

TEST(ApplyFunctionTest, ZeroThreads) {
  std::vector<int> data = {1, 2, 3};
  std::function<void(int &)> transform = [](int &x) { x = 42; };
  ApplyFunction(data, transform, 0);
  std::vector<int> expected = {1, 2, 3};
  EXPECT_EQ(data, expected);
}

TEST(ApplyFunctionTest, HeavyFunction) {
  std::vector<int> data(100, 1);
  std::atomic<int> counter{0};
  std::function<void(int &)> heavyFunc = [&counter](int &x) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    x += 1;
    counter++;
  };
  ApplyFunction(data, heavyFunc, 4);
  EXPECT_EQ(counter.load(), 100);
  for (const auto &val : data) {
    EXPECT_EQ(val, 2);
  }
}

TEST(ApplyFunctionTest, StatefulFunction) {
  std::vector<int> data(10, 0);
  int state = 0;
  std::function<void(int &)> stateful = [&state](int &x) { x = ++state; };
  ApplyFunction(data, stateful, 3);
  for (size_t i = 0; i < data.size(); ++i) {
    EXPECT_GT(data[i], 0);
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}