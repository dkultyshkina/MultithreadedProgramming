# Домашнее задание для лекции по теме "Системный вызов futex, устройство кэша, модель памяти C++"

В этой домашней работе два варианта:

* Вариант #1 -- реализовать аналог std::mutex с использованием системного вызова futex.

# РЕШЕНИЕ

```
[==========] Running 7 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 7 tests from FutexMutexTest
[ RUN      ] FutexMutexTest.BasicLockUnlock
[       OK ] FutexMutexTest.BasicLockUnlock (0 ms)
[ RUN      ] FutexMutexTest.TryLockSuccess
[       OK ] FutexMutexTest.TryLockSuccess (0 ms)
[ RUN      ] FutexMutexTest.TryLockFailure
[       OK ] FutexMutexTest.TryLockFailure (0 ms)
[ RUN      ] FutexMutexTest.TryLockAfterUnlock
[       OK ] FutexMutexTest.TryLockAfterUnlock (0 ms)
[ RUN      ] FutexMutexTest.DoubleUnlockIsSafe
[       OK ] FutexMutexTest.DoubleUnlockIsSafe (0 ms)
[ RUN      ] FutexMutexTest.StateTransition
[       OK ] FutexMutexTest.StateTransition (0 ms)
[ RUN      ] FutexMutexTest.MultipleThreadsIncrement
[       OK ] FutexMutexTest.MultipleThreadsIncrement (1 ms)
[----------] 7 tests from FutexMutexTest (2 ms total)

[----------] Global test environment tear-down
[==========] 7 tests from 1 test suite ran. (2 ms total)
[  PASSED  ] 7 tests.
```

Тест	Что проверяет	Почему это важно

1. BasicLockUnlock	- базовый захват и освобождение мьютекса,	проверка, что основные операции работают;
2. TryLockSuccess	- корректность неблокирующего захвата;
3. TryLockFailure -	проверка отказа при конкуренции;
4. TryLockAfterUnlock	- проверка повторного использования;
5. DoubleUnlockIsSafe	- повторный unlock;
6. StateTransition -	проверка переходов между состояниями;
7. MultipleThreadsIncrement - проверяет атомарность инкремента.