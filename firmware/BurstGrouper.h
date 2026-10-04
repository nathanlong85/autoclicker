// firmware/BurstGrouper.h
#pragma once
#include <cstdint>

// Pure: groups a stream of rising-edge timestamps (microseconds, from
// micros() on real hardware) into pulse "bursts". Consecutive edges no
// more than kBurstGapUs apart belong to the same burst; a bigger gap means
// the previous burst has ended. See
// docs/scroll-wheel-investigation.md's "Resolution" section: bursts land
// about 480us apart while spinning, and the 2 (sometimes 3) pulses within
// one burst are under 1us apart -- 20us cleanly separates the two
// regardless of spin speed, since intra-burst spacing comes from the
// sensor's own internal clock, not mechanical movement.
class BurstGrouper {
 public:
  static constexpr uint32_t kBurstGapUs = 20;

  BurstGrouper();

  // Call once per detected rising edge, with its timestamp. Returns true
  // exactly when this edge's gap from the previous one confirms the prior
  // burst has ended, writing that burst's pulse count to
  // *completed_burst_size. Returns false (leaving *completed_burst_size
  // untouched) while still accumulating the current burst. The final,
  // in-progress burst when input stops is never flushed -- callers don't
  // need it.
  bool pulse(uint32_t now_us, int* completed_burst_size);

 private:
  uint32_t last_edge_us_;
  int current_burst_size_;
  bool have_edge_;
};
