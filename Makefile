CXX = g++
CXXFLAGS = -std=c++20 -pthread -O2 -Wall -Wextra
LDFLAGS = -lrt

all: clean producer consumer run

producer: producer.cpp queue.h
	$(CXX) $(CXXFLAGS) -o producer producer.cpp $(LDFLAGS)

consumer: consumer.cpp queue.h
	$(CXX) $(CXXFLAGS) -o consumer consumer.cpp $(LDFLAGS)

clean:
	rm -f producer consumer

run:
	@echo "Начало producer..."
	@./producer &
	@sleep 1
	@echo "Начало consumer..."
	@./consumer
	@sleep 1
	@wait

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./producer
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./consumer