CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2
LDFLAGS = -pthread

TARGET = test_futex_mutex
SOURCES = test.cpp

all: $(TARGET)

$(TARGET): $(SOURCES) futex_mutex.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS) -lgtest -lgtest_main

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)