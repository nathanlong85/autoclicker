// firmware/WheelPulseDecoder.h
#pragma once

// Pure: classifies wheel rotation from a stream of completed burst sizes
// (see BurstGrouper). Tracks a rolling window of the most recent
// kWindowSize bursts and reports which "triple rate" band the window's
// fraction of 3-or-more-pulse bursts falls into.
//
// Starting constants from docs/scroll-wheel-investigation.md's
// "Resolution" section (logic-analyzer data only): one rotation direction
// showed a 0-0.4% triple rate, the other 1.4-4.9%, with zero overlap
// across every capture taken. kWindowSize=100 bursts is about 50ms of
// continuous spinning at the observed ~2kHz burst rate -- fast enough to
// feel responsive. kTripleRateThresholdHundredthsPercent=100 (1.00%) sits
// between the two observed bands.
//
// NOT YET CALIBRATED against real nice!nano GPIO-interrupt hardware --
// only logic-analyzer data backs these numbers so far. See the
// implementation plan's Tasks 6-9 before trusting this in the real build.
// A single stray triple inside an otherwise-clean 100-burst window hits
// exactly 1.00%, right at the threshold -- a known sensitivity to tune
// away during calibration, not a bug to silently paper over here.
//
// Deliberately reports signal-domain results, not "up"/"down": mapping a
// triple-rate band to a physical rotation direction depends on how the
// sensor ends up wired to the nice!nano, decided during calibration (Task
// 9). Wheel.cpp owns that mapping, the same way it owns turning physical
// "up" into a negative detent value for Clicker::scroll() per the design
// doc.
class WheelPulseDecoder {
 public:
  enum class Direction { kUnknown, kHighTripleRate, kLowTripleRate };

  static constexpr int kWindowSize = 100;
  static constexpr int kTripleRateThresholdHundredthsPercent = 100;  // 1.00%

  WheelPulseDecoder();

  // Call once per completed burst (burst_size from BurstGrouper::pulse()).
  // Returns kUnknown until kWindowSize bursts have been seen at all.
  Direction update(int burst_size);

  // The most recent classification update() computed, without feeding new
  // data. kUnknown before the first full window. Useful for a caller (like
  // Wheel::update()) that wants "the current state" on every call, not
  // just on the calls where a burst happened to complete.
  Direction current() const;

 private:
  int window_[kWindowSize];
  int write_index_;
  int filled_count_;
  int triple_count_;
  Direction current_;
};
