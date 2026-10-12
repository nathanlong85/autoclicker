// firmware/WheelPulseDecoder.cpp
#include "WheelPulseDecoder.h"

WheelPulseDecoder::WheelPulseDecoder()
    : write_index_(0),
      filled_count_(0),
      triple_count_(0),
      current_(Direction::kUnknown) {
  for (int i = 0; i < kWindowSize; ++i) window_[i] = 0;
}

WheelPulseDecoder::Direction WheelPulseDecoder::update(int burst_size) {
  bool is_triple = burst_size >= 3;

  if (filled_count_ == kWindowSize) {
    if (window_[write_index_] >= 3) triple_count_--;
  } else {
    filled_count_++;
  }

  window_[write_index_] = burst_size;
  if (is_triple) triple_count_++;
  write_index_ = (write_index_ + 1) % kWindowSize;

  if (filled_count_ < kWindowSize) {
    current_ = Direction::kUnknown;
    return current_;
  }

  int rate_hundredths_percent = (triple_count_ * 10000) / kWindowSize;
  current_ = (rate_hundredths_percent >= kTripleRateThresholdHundredthsPercent)
                 ? Direction::kHighTripleRate
                 : Direction::kLowTripleRate;
  return current_;
}

WheelPulseDecoder::Direction WheelPulseDecoder::current() const {
  return current_;
}
