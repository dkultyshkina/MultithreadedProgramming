#include "apply_function.hpp"
#include <benchmark/benchmark.h>
#include <random>

std::vector<int> generateData(size_t size) {
  std::vector<int> data(size);
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1, 100);
  for (auto &val : data) {
    val = dis(gen);
  }
  return data;
}

void lightFunction(int &x) { x = x * 2 + 1; }

void heavyFunction(int &x) {
  volatile int result = 0;
  for (int i = 0; i < 100; ++i) {
    for (int j = 0; j < 10; ++j) {
      result += (x * i) / (j + 1) + (i * j) / (x + 1);
    }
  }
  x = result;
}

static void BM_LightFunction_SmallData(benchmark::State &state) {
  auto data = generateData(10);
  std::function<void(int &)> func = lightFunction;
  for (auto _ : state) {
    auto dataCopy = data;
    ApplyFunction(dataCopy, func, state.range(0));
  }
}
BENCHMARK(BM_LightFunction_SmallData)->Arg(1)->Arg(8);

static void BM_LightFunction_LargeData(benchmark::State &state) {
  auto data = generateData(1000000);
  std::function<void(int &)> func = lightFunction;
  for (auto _ : state) {
    auto dataCopy = data;
    ApplyFunction(dataCopy, func, state.range(0));
  }
}
BENCHMARK(BM_LightFunction_LargeData)->Arg(1)->Arg(8);

static void BM_HeavyFunction_SmallData(benchmark::State &state) {
  auto data = generateData(10);
  std::function<void(int &)> func = heavyFunction;
  for (auto _ : state) {
    auto dataCopy = data;
    ApplyFunction(dataCopy, func, state.range(0));
  }
}
BENCHMARK(BM_HeavyFunction_SmallData)->Arg(1)->Arg(8);

static void BM_HeavyFunction_LargeData(benchmark::State &state) {
  auto data = generateData(10000);
  std::function<void(int &)> func = heavyFunction;
  for (auto _ : state) {
    auto dataCopy = data;
    ApplyFunction(dataCopy, func, state.range(0));
  }
}
BENCHMARK(BM_HeavyFunction_LargeData)->Arg(1)->Arg(8);

BENCHMARK_MAIN();