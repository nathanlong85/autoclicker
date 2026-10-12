// firmware/BurstGrouper.cpp
#include "BurstGrouper.h"

BurstGrouper::BurstGrouper()
    : last_edge_us_(0), current_burst_size_(0), have_edge_(false) {}

bool BurstGrouper::pulse(uint32_t now_us, int* completed_burst_size) {
  if (!have_edge_) {
    have_edge_ = true;
    current_burst_size_ = 1;
    last_edge_us_ = now_us;
    return false;
  }

  uint32_t gap = now_us - last_edge_us_;
  last_edge_us_ = now_us;

  if (gap > kBurstGapUs) {
    *completed_burst_size = current_burst_size_;
    current_burst_size_ = 1;
    return true;
  }

  current_burst_size_++;
  return false;
}
