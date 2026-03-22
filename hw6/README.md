## Домашнее задание для лекции по теме "Structured concurrency"

### Задание на 10 баллов
Напишите свою реализацию ThreadPool. При этом метод Submit, с помощью которого вы отправляете задачу в ThreadPool, должен возвращать аналог std::future, который вы также должны сами реализовать.

# РЕШЕНИЕ

```
./thread_pool_test
[==========] Running 8 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 8 tests from ThreadPoolTest
[ RUN      ] ThreadPoolTest.Creation
[       OK ] ThreadPoolTest.Creation (0 ms)
[ RUN      ] ThreadPoolTest.CreationWithZero
[       OK ] ThreadPoolTest.CreationWithZero (0 ms)
[ RUN      ] ThreadPoolTest.SimpleTask
[       OK ] ThreadPoolTest.SimpleTask (0 ms)
[ RUN      ] ThreadPoolTest.TaskWithArgs
[       OK ] ThreadPoolTest.TaskWithArgs (0 ms)
[ RUN      ] ThreadPoolTest.VoidTask
[       OK ] ThreadPoolTest.VoidTask (0 ms)
[ RUN      ] ThreadPoolTest.MultipleTasks
[       OK ] ThreadPoolTest.MultipleTasks (26 ms)
[ RUN      ] ThreadPoolTest.ExceptionHandling
[       OK ] ThreadPoolTest.ExceptionHandling (0 ms)
[ RUN      ] ThreadPoolTest.FutureWait
[       OK ] ThreadPoolTest.FutureWait (100 ms)
[----------] 8 tests from ThreadPoolTest (127 ms total)

[----------] Global test environment tear-down
[==========] 8 tests from 1 test suite ran. (127 ms total)
[  PASSED  ] 8 tests.
```

1. Creation - проверка корректного создания пула с заданным количеством потоков;
2. CreationWithZero - проверка создания пула с 0 количеством потоков;
3. SimpleTask - проверка выполнения задачи с возвратом значения;
4. TaskWithArgs - проверка передачи аргументов в задачу;
5. VoidTask - проверка выполнения задачи без возвращаемого значения;
6. MultipleTasks - проверка большого количества задач;
7. FutureWait - проверка методов ожидания Future;
8. FutureState - проверка изменения состояния Future с течением времени.