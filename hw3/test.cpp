#include "buffered_channel.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

std::vector<int> Flatten(const std::vector<std::vector<int>> &nested) {
  std::vector<int> flat;
  for (const auto &vec : nested) {
    flat.insert(flat.end(), vec.begin(), vec.end());
  }
  return flat;
}

void VerifyData(const std::vector<std::vector<int>> &sent,
                const std::vector<std::vector<int>> &received) {
  auto all_sent = Flatten(sent);
  auto all_recv = Flatten(received);
  std::sort(all_sent.begin(), all_sent.end());
  std::sort(all_recv.begin(), all_recv.end());
  ASSERT_EQ(all_sent, all_recv);
}

void RunTests(int producers, int consumers, int buffer_size, int duration_ms) {
  std::vector<std::thread> workers;
  BufferedChannel<int> channel(buffer_size);
  std::atomic<int> next_value{0};
  std::atomic<bool> is_closed{false};
  std::atomic<int> active_producers{producers};
  std::vector<std::vector<int>> produced(producers);
  std::vector<std::vector<int>> consumed(consumers);
  std::atomic<int> sends_after_close{0};
  std::atomic<int> receives_after_close{0};
  for (int i = 0; i < producers; ++i) {
    workers.emplace_back([&, i]() {
      try {
        while (!is_closed.load()) {
          int val = next_value.fetch_add(1);
          channel.Send(val);
          produced[i].push_back(val);
        }
      } catch (const std::runtime_error &e) {
        sends_after_close.fetch_add(1);
      }
      active_producers.fetch_sub(1);
    });
  }
  for (int i = 0; i < consumers; ++i) {
    workers.emplace_back([&, i]() {
      while (true) {
        auto val = channel.Recv();
        if (!val) {
          receives_after_close.fetch_add(1);
          break;
        }
        consumed[i].push_back(*val);
      }
    });
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
  is_closed.store(true);
  channel.Close();
  for (auto &w : workers) {
    w.join();
  }
  VerifyData(produced, consumed);
  EXPECT_GE(sends_after_close.load(), 0);
  EXPECT_GE(receives_after_close.load(), 0);
  if (producers == 1 && consumers == 1) {
    EXPECT_EQ(produced[0].size(), consumed[0].size());
  }
  size_t total_produced = 0, total_consumed = 0;
  for (const auto &v : produced)
    total_produced += v.size();
  for (const auto &v : consumed)
    total_consumed += v.size();
  EXPECT_EQ(total_produced, total_consumed);
}

TEST(Correctness, OneToOne) { RunTests(1, 1, 1, 300); }

TEST(Correctness, OneToMany) { RunTests(1, 4, 2, 500); }

TEST(Correctness, ManyToOne) { RunTests(4, 1, 2, 500); }

TEST(Correctness, ManyToMany) { RunTests(4, 4, 4, 500); }

TEST(Buffer, Zero) { RunTests(2, 2, 0, 500); }

TEST(Buffer, Tiny) { RunTests(4, 4, 1, 500); }

TEST(Buffer, Small) { RunTests(8, 8, 2, 500); }

TEST(Buffer, Medium) { RunTests(6, 6, 10, 500); }

TEST(Buffer, Large) { RunTests(4, 4, 100, 500); }

TEST(Edge, CloseEmpty) {
  BufferedChannel<int> channel(5);
  channel.Close();
  auto val = channel.Recv();
  EXPECT_FALSE(val.has_value());
  EXPECT_THROW(channel.Send(42), std::runtime_error);
}

TEST(Edge, CloseWithData) {
  BufferedChannel<int> channel(5);
  for (int i = 0; i < 3; ++i) {
    channel.Send(i);
  }
  channel.Close();
  for (int i = 0; i < 3; ++i) {
    auto val = channel.Recv();
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, i);
  }
  EXPECT_FALSE(channel.Recv().has_value());
}

TEST(Edge, DoubleClose) {
  BufferedChannel<int> channel(5);
  channel.Close();
  channel.Close();
}