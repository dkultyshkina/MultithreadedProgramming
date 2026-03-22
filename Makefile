CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra
LDFLAGS = -pthread
TARGET = dfs
SRCS = main.cpp

all: clean $(TARGET) run

$(TARGET): $(SRCS) dfs.hpp
	$(CXX) $(CXXFLAGS) -o $@ main.cpp $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(TARGET)