#include "buffered_channel.hpp"
#include <atomic>
#include <benchmark/benchmark.h>
#include <thread>
#include <vector>

const int kCount = 500000;

static void BM_BufferedChannel(benchmark::State &state) {
  int buffer_size = state.range(0);
  int senders = state.range(1);
  int receivers = state.range(2);
  int64_t expected_sum = static_cast<int64_t>(kCount) * (kCount - 1) / 2;
  for (auto _ : state) {
    BufferedChannel<int> channel(buffer_size);
    std::atomic<int64_t> total_sum{0};
    std::vector<std::thread> send_threads;
    for (int i = 0; i < senders; ++i) {
      send_threads.emplace_back([&channel, i, senders]() {
        for (int j = i; j < kCount; j += senders) {
          channel.Send(j);
        }
      });
    }
    std::vector<std::thread> recv_threads;
    for (int i = 0; i < receivers; ++i) {
      recv_threads.emplace_back([&channel, &total_sum]() {
        while (auto value = channel.Recv()) {
          total_sum += *value;
        }
      });
    }
    for (auto &t : send_threads)
      t.join();
    channel.Close();
    for (auto &t : recv_threads)
      t.join();
    if (total_sum != expected_sum) {
      state.SkipWithError("Wrong sum");
    }
  }
}

static void CustomArgs(benchmark::internal::Benchmark *b) {
  int threads = std::thread::hardware_concurrency();
  int half = threads / 2;
  b->Args({2, 2, threads - 2});
  b->Args({10, half, threads - half});
  b->Args({100000, half, threads - half});
}

BENCHMARK(BM_BufferedChannel)
    ->Apply(CustomArgs)
    ->MeasureProcessCPUTime()
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond)
    ->MinTime(0.5);

BENCHMARK_MAIN();