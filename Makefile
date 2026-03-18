CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread -I.

all: run_tests run_benchmark

run_tests: test.cpp
	$(CXX) $(CXXFLAGS) test.cpp -o run_tests -lgtest -lgtest_main -pthread

run_benchmark: benchmark.cpp
	$(CXX) $(CXXFLAGS) benchmark.cpp -o run_benchmark -lbenchmark -lpthread

test: run_tests
	./run_tests

benchmark: run_benchmark
	./run_benchmark

clean:
	rm -f run_tests run_benchmark
