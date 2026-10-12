CXX := c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Ifirmware -Itest

TEST_SRCS := test/test_main.cpp test/test_burst_grouper.cpp \
             test/test_wheel_pulse_decoder.cpp \
             firmware/BurstGrouper.cpp firmware/WheelPulseDecoder.cpp
TEST_BIN := build/test_runner

.PHONY: test clean

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) -o $(TEST_BIN)

clean:
	rm -rf build
