CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pthread -O2
LDFLAGS = -pthread

TEST = buffered_channel_test
BENCHMARK = buffered_channel_benchmark

TEST_SRC = test.cpp
BENCHMARK_SRC = benchmark.cpp

all: $(TEST) $(BENCHMARK)

$(TEST): $(TEST_SRC) buffered_channel.hpp
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $(TEST) -lgtest -lgtest_main $(LDFLAGS)

$(BENCHMARK): $(BENCHMARK_SRC) buffered_channel.hpp
	$(CXX) $(CXXFLAGS) $(BENCHMARK_SRC) -o $(BENCHMARK) -lbenchmark $(LDFLAGS)

test: $(TEST)
	./$(TEST)

benchmark: $(BENCHMARK)
	./$(BENCHMARK)

clean:
	rm -f $(TEST) $(BENCHMARK)

.PHONY: all test benchmark clean