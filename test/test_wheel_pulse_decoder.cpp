// test/test_wheel_pulse_decoder.cpp
#include "doctest.h"
#include "WheelPulseDecoder.h"

TEST_CASE("reports kUnknown until the window has seen kWindowSize bursts") {
  WheelPulseDecoder d;
  for (int i = 0; i < WheelPulseDecoder::kWindowSize - 1; ++i) {
    CHECK(d.update(2) == WheelPulseDecoder::Direction::kUnknown);
  }
  CHECK(d.update(2) != WheelPulseDecoder::Direction::kUnknown);
}

TEST_CASE("an all-2-pulse window classifies as low triple rate") {
  WheelPulseDecoder d;
  WheelPulseDecoder::Direction result = WheelPulseDecoder::Direction::kUnknown;
  for (int i = 0; i < WheelPulseDecoder::kWindowSize; ++i) {
    result = d.update(2);
  }
  CHECK(result == WheelPulseDecoder::Direction::kLowTripleRate);
}

TEST_CASE("a clearly-elevated triple rate classifies as high") {
  WheelPulseDecoder d;
  WheelPulseDecoder::Direction result = WheelPulseDecoder::Direction::kUnknown;
  for (int i = 0; i < WheelPulseDecoder::kWindowSize; ++i) {
    // 4 triples per 100 bursts == 4.00%, matching the investigation's
    // observed "down" band (1.4-4.9%)
    int burst_size = (i % 25 == 0) ? 3 : 2;
    result = d.update(burst_size);
  }
  CHECK(result == WheelPulseDecoder::Direction::kHighTripleRate);
}

TEST_CASE("current() reflects the last update() result without new data") {
  WheelPulseDecoder d;
  CHECK(d.current() == WheelPulseDecoder::Direction::kUnknown);
  for (int i = 0; i < WheelPulseDecoder::kWindowSize; ++i) {
    d.update(2);
  }
  CHECK(d.current() == WheelPulseDecoder::Direction::kLowTripleRate);
}
