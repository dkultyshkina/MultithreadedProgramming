# Домашнее задание для лекции по теме "Процессы и потоки"

Реализуйте следующий метод:
```cpp
template <typename T>
void ApplyFunction(std::vector<T>& data, const std::function<void(T&)>& transform, const int threadCount = 1);

```

Данный метод должен применить переданную функцию `transform` к каждому элементу вектора `data`. `threadCount` задает количество потоков, которое нужно использовать для применения функции. Если число потоков превышает число элементов, то число потоков следует взять равным числу элементов.

Напишите тесты для вашей реализации с использованием [gtest](https://google.github.io/googletest/).

Напишите бенчмарк для вашей реализации с использованием [benchmark](https://google.github.io/benchmark/user_guide.html).

В бенчмарке отразите две ситуации -- когда однопоточная версия стабильно быстрее многопоточной и обратную ситуацию. Достигните этого как с помощью подбора размера вектора `data`, так и с помощью подбора функции `transform`.


# Cборка

## Сборка, запуск тестов и бенчмарки

```bash
make 
```

```bash
make all
```

## Очистка

```bash
make clean
```

## Собрать и запустить тесты
```bash 
make run-test 
make test
```

## Собрать и запустить бенчмарки
```bash
make run-benchmark 
make benchmark
```

# Очистить
```bash
make clean
```

# Результат

## Тесты
```
[==========] Running 7 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 7 tests from ApplyFunctionTest
[ RUN      ] ApplyFunctionTest.SimpleIncrement
[       OK ] ApplyFunctionTest.SimpleIncrement (0 ms)
[ RUN      ] ApplyFunctionTest.SingleThread
[       OK ] ApplyFunctionTest.SingleThread (0 ms)
[ RUN      ] ApplyFunctionTest.MoreThreadsThanElements
[       OK ] ApplyFunctionTest.MoreThreadsThanElements (0 ms)
[ RUN      ] ApplyFunctionTest.EmptyVector
[       OK ] ApplyFunctionTest.EmptyVector (0 ms)
[ RUN      ] ApplyFunctionTest.ZeroThreads
[       OK ] ApplyFunctionTest.ZeroThreads (0 ms)
[ RUN      ] ApplyFunctionTest.HeavyFunction
[       OK ] ApplyFunctionTest.HeavyFunction (26 ms)
[ RUN      ] ApplyFunctionTest.StatefulFunction
[       OK ] ApplyFunctionTest.StatefulFunction (0 ms)
[----------] 7 tests from ApplyFunctionTest (27 ms total)

[----------] Global test environment tear-down
[==========] 7 tests from 1 test suite ran. (27 ms total)
[  PASSED  ] 7 tests.
```
## Benchmark
```
Running ./run_benchmark
Run on (16 X 4507 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 512 KiB (x8)
  L3 Unified 16384 KiB (x1)
Load Average: 0.99, 0.73, 0.76
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
-----------------------------------------------------------------------
Benchmark                             Time             CPU   Iterations
-----------------------------------------------------------------------
BM_LightFunction_SmallData/1      28500 ns        16125 ns        43618
BM_LightFunction_SmallData/8     189869 ns       184127 ns         3696
BM_LightFunction_LargeData/1    2447059 ns       176411 ns         3923
BM_LightFunction_LargeData/8     884845 ns       444960 ns         1569
BM_HeavyFunction_SmallData/1      58161 ns        15736 ns        44411
BM_HeavyFunction_SmallData/8     193651 ns       188066 ns         3652
BM_HeavyFunction_LargeData/1   27623461 ns        35188 ns          100
BM_HeavyFunction_LargeData/8    4955303 ns       267916 ns         1000
```

1. BM_LightFunction_SmallData (легкая функция, 10 элементов)

Однопоточная версия быстрее на маленьком количестве элементов, так как есть накладные расходы на создание потоков.

2. BM_LightFunction_LargeData (легкая функция, 1,000,000 элементов)

Для легкой функции с большим количеством элементов быстрее использовать многопоточный вариант.

3. BM_HeavyFunction_SmallData (тяжелая функция, 10 элементов)

Однопоточная версия быстрее на маленьком количестве элементов, так как есть накладные расходы на создание потоков.

4. BM_HeavyFunction_LargeData (тяжелая функция, 10,000 элементов)

Для тяжелой функции с большим количеством элементов быстрее использовать многопоточный вариант.