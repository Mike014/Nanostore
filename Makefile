CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -g
STATIC = -static
DEFINE = -DCATCH_AMALGAMATED_CUSTOM_MAIN
SANITIZE = -fsanitize=address,undefined

SRC = src/main.cpp src/wal.cpp src/storage_engine.cpp
TEST_SRC = tests/catch_amalgamated.cpp tests/test_storage.cpp src/wal.cpp src/storage_engine.cpp
TARGET = nanostore
TEST_TARGET = tests/test_storage.exe

all:
	mkdir -p data
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

test:
	mkdir -p data
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $(TEST_TARGET) $(DEFINE) $(STATIC)
	rm -f data/test*.log
	./$(TEST_TARGET)

test-san:
	mkdir -p data
	$(CXX) $(CXXFLAGS) $(SANITIZE) $(TEST_SRC) -o $(TEST_TARGET) $(DEFINE)
	rm -f data/test*.log
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET)
	rm -f data/*.log
