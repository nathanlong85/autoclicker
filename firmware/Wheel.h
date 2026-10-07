// firmware/Wheel.h
#pragma once
#include <Arduino.h>
#include "BurstGrouper.h"
#include "WheelPulseDecoder.h"

// Thin: owns the GPIO interrupt on the wheel sensor's single data pin
// (LQ1 "bottom" -- see docs/scroll-wheel-investigation.md) and wires it
// through BurstGrouper then WheelPulseDecoder. Untested glue, same tier
// as Button/PairingButton -- verified by the implementation plan's
// hands-on calibration tasks and the final manual checklist, not
// `make test`.
//
// kPulsePin is Arduino D18 = P0.02 = the nice!nano pad silkscreened "002"
// (no real nice!nano board definition exists -- Arduino D-numbers map
// through the Feather 52840 variant.cpp, not the silkscreen; see
// docs/hardware-bringup-log.md's D-number/P-port gotcha). Same pin the
// scroll-wheel investigation's interrupt test (test 6) already used for
// LQ1 "bottom". Not D2: that is P0.10, an NFC pin.
class Wheel {
 public:
  static constexpr uint8_t kPulsePin = 18;  // P0.02, silkscreen "002"

  Wheel();

  void begin();  // call once from setup() -- attaches the interrupt

  // Call every loop() iteration. Drains any edges the ISR recorded since
  // the last call and returns the current direction classification
  // (kUnknown if the window hasn't filled yet).
  WheelPulseDecoder::Direction update();

  // Running totals since begin() -- for the calibration sketch (Tasks 7-8)
  // and future debugging. Not reset by update().
  uint32_t totalBursts() const { return total_bursts_; }
  uint32_t totalTriples() const { return total_triples_; }

 private:
  static void handleInterrupt();  // ISR -- Arduino requires a free function

  BurstGrouper grouper_;
  WheelPulseDecoder decoder_;
  uint32_t total_bursts_;
  uint32_t total_triples_;

  // Shared with the ISR -- volatile. Sized generously: the investigation
  // saw ~2000 bursts/sec while spinning, so even a slow loop() iteration
  // shouldn't starve a few-hundred-entry buffer.
  static constexpr int kEdgeBufferSize = 256;
  static volatile uint32_t edge_buffer_[kEdgeBufferSize];
  static volatile int edge_write_index_;
  static volatile int edge_read_index_;
};
