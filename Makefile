CXX := c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Ifirmware -Itest

TEST_SRCS := test/test_main.cpp test/test_burst_grouper.cpp \
             firmware/BurstGrouper.cpp
TEST_BIN := build/test_runner

.PHONY: test clean

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) -o $(TEST_BIN)

clean:
	rm -rf build
