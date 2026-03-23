CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pthread -O2
LDFLAGS = -lgtest -lgtest_main -pthread
TARGET = thread_pool_test
SOURCES = test.cpp
HEADERS = thread_pool.hpp

all: clean $(TARGET) run

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)