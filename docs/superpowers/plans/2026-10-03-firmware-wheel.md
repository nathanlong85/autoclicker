# firmware/ Wheel-Reading Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Human-in-the-loop note:** Tasks 1-5 are pure C++, host-testable, no
> hardware needed. Tasks 6-8 need Nate at the physical hardware (the donor
> mouse board, a nice!nano, jumper wires) — there is no automated test for
> these, "pass" means Nate observed the expected result and reported it
> back. Colin is not involved in this plan — it's entirely `firmware/`
> (Claude's), per the project's division of labor.

**Goal:** A tested, working `Wheel` (GPIO interrupt glue) built on two pure
classes — `BurstGrouper` and `WheelPulseDecoder` — that reads the donor
mouse's single-pin wheel sensor and classifies rotation into a direction
signal, calibrated against real nice!nano hardware (not just the logic
analyzer used during investigation).

**Architecture:** `BurstGrouper` (pure: edge timestamps → completed burst
sizes) feeds `WheelPulseDecoder` (pure: burst sizes → rolling-window
direction classification). `Wheel` wraps a GPIO interrupt around both —
thin, untested glue, same tier as `Button`/`PairingButton`. Two things this
plan explicitly cannot settle by unit testing alone — the decode window/
threshold constants, and how many bursts map to one physical detent — get
hands-on calibration tasks instead of invented numbers.

**Tech Stack:** C++17 for the pure classes (host-tested via `make test`,
doctest v2.4.11), Arduino/`adafruit:nrf52:feather52840` FQBN for `Wheel` and
the calibration sketch (see `docs/hardware-bringup-log.md` for why this FQBN
and not a native "nice!nano" one).

**Spec:** `docs/superpowers/specs/2026-09-10-ble-autoclicker-design.md`
(see its "Correction — scroll wheel is not a quadrature encoder" section),
backed by `docs/scroll-wheel-investigation.md`.

## Global Constraints

- `firmware/`'s pure classes (`BurstGrouper`, `WheelPulseDecoder`) must
  compile with plain `g++`/`clang++` — no `Arduino.h`, same rule `core/`
  follows. `Wheel` itself does include `Arduino.h` — it's glue, not pure.
- Burst-grouping gap: edges **less than or equal to** 20µs apart belong to
  the same burst (strictly greater than 20µs starts a new one) — this gap
  is fixed regardless of wheel spin speed, because it comes from `LQ1`'s own
  internal clock, not mechanical movement (bursts are ~480µs apart while
  spinning; the 2-3 pulses within one burst are under 1µs apart — 20µs sits
  comfortably between those two scales either way. See
  `docs/scroll-wheel-investigation.md`'s "Resolution" section).
- Starting decode constants (window = 100 bursts, threshold = 1.00% triple
  rate) come from logic-analyzer data only. **Not yet calibrated against
  real nice!nano GPIO-interrupt hardware** — Tasks 6-8 do that, Task 9
  updates the constants if the real numbers disagree.
- No change to `core/Clicker::scroll(int detents)`'s contract. This plan
  does not wire `Wheel` into `Clicker` yet — that needs the detent-mapping
  data from Task 8, which doesn't exist until this plan runs.

## Review Focus

1. **A single noise/bounce edge landing just past the 20µs burst-gap
   boundary** could merge two real separate bursts into one, or split one
   real burst into two — pinned down by `BurstGrouper`'s exact-boundary test
   (Task 2).
2. **Classifying before enough data exists** (first <100 bursts after
   startup or after a long idle gap) must report "unknown," not a
   confident-looking wrong guess — covered in Task 3.
3. **A single stray triple-pulse burst (noise, not a real direction signal)
   landing in an otherwise clean window** sits exactly at the chosen 1.00%
   threshold for a 100-burst window — covered (and explicitly flagged as a
   known calibration risk, not hidden) in Task 4.
4. **Old bursts from a previous spin session must age out** of the rolling
   window once enough new bursts arrive, so a long-idle board doesn't keep
   judging new rotation against ancient data — covered in Task 4.
5. **The ISR dropping edges under real interrupt latency** (a risk no unit
   test can catch, since it's a hardware timing question) — covered by
   Task 7's sanity check against the logic analyzer's known burst rate
   before trusting any calibration numbers gathered on real hardware.

## File Structure

```
autoclicker/
  firmware/
    BurstGrouper.h/.cpp         — pure: edge timestamps -> burst sizes
    WheelPulseDecoder.h/.cpp    — pure: burst sizes -> direction
    Wheel.h/.cpp                — GPIO interrupt + both of the above, thin
  test/
    doctest.h                  — vendored (this plan creates it if core/
                                  hasn't already)
    test_main.cpp               — ditto
    test_burst_grouper.cpp
    test_wheel_pulse_decoder.cpp
  Makefile                      — ditto; see Task 1's note on merging with
                                   core-clicker's plan later
  scratch/
    wheel_calibration/
      wheel_calibration.ino     — throwaway, Tasks 7-8 only
```

---

### Task 1: Shared test scaffolding + first `BurstGrouper` test

**Files:**
- Create: `test/doctest.h` (vendored — **skip this step if it already
  exists**, i.e. if `core-clicker`'s plan ran first)
- Create: `test/test_main.cpp` (**skip if it already exists**)
- Create: `Makefile` (**if it already exists** — meaning `core-clicker`'s
  Task 1 ran first — **modify** it instead: add `firmware/BurstGrouper.cpp
  firmware/WheelPulseDecoder.cpp test/test_burst_grouper.cpp
  test/test_wheel_pulse_decoder.cpp` to `TEST_SRCS` and `-Ifirmware` to
  `CXXFLAGS`, rather than recreating the file)
- Create: `firmware/BurstGrouper.h`
- Create: `firmware/BurstGrouper.cpp`
- Test: `test/test_burst_grouper.cpp`

**Interfaces:**
- Produces: `class BurstGrouper` with `BurstGrouper()` and `bool
  pulse(uint32_t now_us, int* completed_burst_size)`. Later tasks
  (`WheelPulseDecoder`, `Wheel`) consume this signature directly.

- [ ] **Step 1: Vendor doctest (skip if `test/doctest.h` already exists)**

```bash
curl -fsSL -o test/doctest.h https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
```
Expected: `test/doctest.h` exists, several thousand lines (`wc -l
test/doctest.h`), not an HTML error page.

- [ ] **Step 2: Write the doctest main-file (skip if it already exists)**

```cpp
// test/test_main.cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```

- [ ] **Step 3: Write the Makefile (or modify, per the note above)**

```makefile
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
```

- [ ] **Step 4: Write the `BurstGrouper` interface**

```cpp
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
```

- [ ] **Step 5: Write the failing test**

```cpp
// test/test_burst_grouper.cpp
#include "doctest.h"
#include "BurstGrouper.h"

TEST_CASE("a two-pulse burst completes once the next burst's first edge arrives") {
  BurstGrouper g;
  int size = -1;

  CHECK_FALSE(g.pulse(0, &size));      // first edge of burst 1
  CHECK_FALSE(g.pulse(1, &size));      // 1us gap -- still burst 1

  CHECK(g.pulse(500, &size));          // 499us gap -- burst 1 just completed
  CHECK(size == 2);
}
```

- [ ] **Step 6: Run it to verify it fails**

Run: `make test`
Expected: FAIL — linker error, undefined symbols for `BurstGrouper`
(`BurstGrouper.cpp` doesn't exist yet).

- [ ] **Step 7: Write the minimal implementation**

```cpp
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
```

- [ ] **Step 8: Run test to verify it passes**

Run: `make test`
Expected: PASS — `[doctest] test cases: 1 | 1 passed | 0 failed`

- [ ] **Step 9: Commit**

```bash
git add test/doctest.h test/test_main.cpp test/test_burst_grouper.cpp \
        Makefile firmware/BurstGrouper.h firmware/BurstGrouper.cpp
git commit -m "test(firmware): scaffold build/test pipeline, BurstGrouper basic case"
```

---

### Task 2: `BurstGrouper` — three-pulse bursts and the exact gap boundary

**Files:**
- Test: `test/test_burst_grouper.cpp`

**Interfaces:** No signature changes.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("a three-pulse burst reports size 3") {
  BurstGrouper g;
  int size = -1;

  CHECK_FALSE(g.pulse(1000, &size));
  CHECK_FALSE(g.pulse(1001, &size));
  CHECK_FALSE(g.pulse(1002, &size));
  CHECK(g.pulse(1500, &size));
  CHECK(size == 3);
}

TEST_CASE("a gap of exactly kBurstGapUs still belongs to the same burst") {
  BurstGrouper g;
  int size = -1;

  CHECK_FALSE(g.pulse(0, &size));
  CHECK_FALSE(g.pulse(BurstGrouper::kBurstGapUs, &size));  // gap == 20, not > 20
  CHECK(g.pulse(1000, &size));
  CHECK(size == 2);  // both edges counted in the same burst
}
```

- [ ] **Step 2: Run tests to verify they pass already or fail correctly**

Run: `make test`
Expected: PASS — Task 1's implementation already handles both cases
correctly (the three-pulse case just extends the same accumulation logic;
the boundary case exercises the `gap > kBurstGapUs`, not `>=`, comparison
already written). This task exists to pin down and document that boundary
choice with an explicit test, not because new production code is needed.

- [ ] **Step 3: Commit**

```bash
git add test/test_burst_grouper.cpp
git commit -m "test(firmware): lock in BurstGrouper's 3-pulse and exact-boundary behavior"
```

---

### Task 3: `WheelPulseDecoder` — window fill and basic classification

**Files:**
- Create: `firmware/WheelPulseDecoder.h`
- Create: `firmware/WheelPulseDecoder.cpp`
- Test: `test/test_wheel_pulse_decoder.cpp`

**Interfaces:**
- Consumes: burst sizes (`int`) from `BurstGrouper::pulse()`'s
  `completed_burst_size` output.
- Produces: `class WheelPulseDecoder` with `WheelPulseDecoder()`, `Direction
  update(int burst_size)`, `Direction current() const`, and the
  `enum class Direction { kUnknown, kHighTripleRate, kLowTripleRate }`.
  `Wheel` (Task 5) consumes all of this.

- [ ] **Step 1: Write the `WheelPulseDecoder` interface**

```cpp
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
```

- [ ] **Step 2: Write the failing tests**

```cpp
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
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `make test`
Expected: FAIL — linker error, `WheelPulseDecoder` has no implementation
yet.

- [ ] **Step 4: Write the implementation**

```cpp
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
```

- [ ] **Step 5: Run tests to verify all pass**

Run: `make test`
Expected: PASS — `5 | 5 passed | 0 failed` (plus the 1 from Task 1/2 =
`6 | 6 passed | 0 failed`).

- [ ] **Step 6: Commit**

```bash
git add firmware/WheelPulseDecoder.h firmware/WheelPulseDecoder.cpp \
        test/test_wheel_pulse_decoder.cpp
git commit -m "feat(firmware): WheelPulseDecoder classifies triple-burst rate"
```

---

### Task 4: `WheelPulseDecoder` — threshold boundary and window eviction

**Files:**
- Test: `test/test_wheel_pulse_decoder.cpp`

**Interfaces:** No signature changes.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("exactly one triple in a 100-burst window hits the threshold boundary") {
  WheelPulseDecoder d;
  WheelPulseDecoder::Direction result = WheelPulseDecoder::Direction::kUnknown;
  for (int i = 0; i < WheelPulseDecoder::kWindowSize; ++i) {
    int burst_size = (i == 0) ? 3 : 2;
    result = d.update(burst_size);
  }
  // Documents current (>=) behavior at the exact threshold -- a single
  // stray triple is enough to cross it. Flagged in the class comment
  // above as a calibration risk to revisit in Task 9, not asserted here
  // as definitely the right call.
  CHECK(result == WheelPulseDecoder::Direction::kHighTripleRate);
}

TEST_CASE("old bursts age out of the window after kWindowSize new ones") {
  WheelPulseDecoder d;
  for (int i = 0; i < WheelPulseDecoder::kWindowSize; ++i) {
    d.update((i % 25 == 0) ? 3 : 2);  // fill with an elevated rate
  }
  WheelPulseDecoder::Direction result = WheelPulseDecoder::Direction::kUnknown;
  for (int i = 0; i < WheelPulseDecoder::kWindowSize; ++i) {
    result = d.update(2);  // now feed a full window of clean bursts
  }
  CHECK(result == WheelPulseDecoder::Direction::kLowTripleRate);
}
```

- [ ] **Step 2: Run tests to verify they pass already or fail correctly**

Run: `make test`
Expected: PASS — Task 3's ring-buffer eviction logic already handles both
cases. This task exists to pin down and document this behavior with
explicit tests, not because new production code is needed.

- [ ] **Step 3: Commit**

```bash
git add test/test_wheel_pulse_decoder.cpp
git commit -m "test(firmware): lock in WheelPulseDecoder's threshold boundary and eviction"
```

---

### Task 5: `Wheel` — GPIO interrupt glue

**Files:**
- Create: `firmware/Wheel.h`
- Create: `firmware/Wheel.cpp`

**Interfaces:**
- Consumes: `BurstGrouper`, `WheelPulseDecoder` from Tasks 1-4, unchanged.
- Produces: `class Wheel` with `Wheel()`, `void begin()`, `Direction
  update()`, `uint32_t totalBursts() const`, `uint32_t totalTriples()
  const`. The calibration sketch (Tasks 7-8) and eventually
  `autoclicker.ino` consume this.

**Not unit tested** — same tier as `Button`/`PairingButton` per the design
doc's testing table (GPIO/interrupt glue, verified by hands-on hardware
checks, not `make test`).

- [ ] **Step 1: Write `Wheel.h`**

```cpp
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
// PIN ASSIGNMENT NOT YET CONFIRMED (see implementation plan Task 6):
// kPulsePin below is a placeholder. Confirm the nice!nano's actual
// silkscreen label against docs/hardware-bringup-log.md's D-number/P-port
// gotcha (no real nice!nano board definition exists -- Arduino D-numbers
// map through the Feather 52840 variant.cpp, not the silkscreen) before
// trusting this constant on real hardware.
class Wheel {
 public:
  static constexpr uint8_t kPulsePin = 2;  // PLACEHOLDER -- confirm at wiring time (Task 6)

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
```

- [ ] **Step 2: Write `Wheel.cpp`**

```cpp
// firmware/Wheel.cpp
#include "Wheel.h"

volatile uint32_t Wheel::edge_buffer_[Wheel::kEdgeBufferSize];
volatile int Wheel::edge_write_index_ = 0;
volatile int Wheel::edge_read_index_ = 0;

Wheel::Wheel() : grouper_(), decoder_(), total_bursts_(0), total_triples_(0) {}

void Wheel::begin() {
  pinMode(kPulsePin, INPUT);
  attachInterrupt(digitalPinToInterrupt(kPulsePin), handleInterrupt, RISING);
}

void Wheel::handleInterrupt() {
  int next = (edge_write_index_ + 1) % kEdgeBufferSize;
  if (next == edge_read_index_) return;  // buffer full -- drop rather than overwrite
  edge_buffer_[edge_write_index_] = micros();
  edge_write_index_ = next;
}

WheelPulseDecoder::Direction Wheel::update() {
  while (edge_read_index_ != edge_write_index_) {
    uint32_t timestamp = edge_buffer_[edge_read_index_];
    edge_read_index_ = (edge_read_index_ + 1) % kEdgeBufferSize;

    int completed_size;
    if (grouper_.pulse(timestamp, &completed_size)) {
      total_bursts_++;
      if (completed_size >= 3) total_triples_++;
      decoder_.update(completed_size);
    }
  }
  return decoder_.current();
}
```

- [ ] **Step 3: Confirm it compiles for the target board (no upload yet)**

Run:
```bash
arduino-cli compile --fqbn adafruit:nrf52:feather52840 --library firmware firmware
```
(Adjust if `arduino-cli` wants a `.ino` to compile against — if so, skip
this step and let Task 7's calibration sketch be the first real compile
check, since it's the first thing that actually includes `Wheel.h` from a
sketch.)

- [ ] **Step 4: Commit**

```bash
git add firmware/Wheel.h firmware/Wheel.cpp
git commit -m "feat(firmware): Wheel wraps GPIO interrupt around BurstGrouper + WheelPulseDecoder"
```

---

### Task 6: Hands-on — wire the sensor to the nice!nano

**Files:** None — physical wiring only.

- [ ] **Step 1: Look up the pin mapping**

Find `variants/feather_nrf52840_express/variant.cpp` in the installed
`adafruit:nrf52` core (per `docs/hardware-bringup-log.md`'s Task 4 note —
same file that mapped Arduino D7 to P1.02 for the bare BLE mouse sketch).
Find which silkscreen-labeled nice!nano pin corresponds to `Wheel::kPulsePin`
(Arduino D2 as currently written). If D2 isn't a convenient/available pin
on the nice!nano's physical layout, pick a different one and update
`kPulsePin` in `firmware/Wheel.h` to match — any GPIO works for
`attachInterrupt` on the nRF52840 (unlike AVR boards, there's no
INT0/INT1-only restriction).

- [ ] **Step 2: Wire it up**

With the donor mouse's own battery installed (needed to power `LQ1`/`LD2`,
same as every logic-analyzer capture in the investigation) and the nice!nano
**unpowered** (no USB connected yet):
- Connect `LQ1`'s `bottom` pin to the nice!nano pin found in Step 1.
- Connect a shared ground: `TP18` (or any confirmed mouse-board GND point)
  to a nice!nano GND pin.
- Do **not** connect `middle` — confirmed dead by the investigation, not
  needed.

- [ ] **Step 3: Sanity-check before powering anything**

With a multimeter, confirm continuity on both new connections (sensor pin
to nice!nano pin; mouse GND to nice!nano GND) and confirm there's no
short between the two new wires. Report back what you measured before
plugging in USB.

---

### Task 7: Hands-on — calibration sketch, Experiment A (direction-rate sanity check)

**Files:**
- Create: `scratch/wheel_calibration/wheel_calibration.ino`

- [ ] **Step 1: Write the calibration sketch**

```cpp
// scratch/wheel_calibration/wheel_calibration.ino
// Throwaway calibration sketch for firmware/Wheel -- NOT part of
// firmware/. Confirms the burst-rate direction classification works on
// real nice!nano GPIO-interrupt hardware (not just the logic analyzer
// used during investigation), and measures how many pulse-bursts
// correspond to one physical wheel detent. See
// docs/superpowers/plans/2026-10-03-firmware-wheel.md Tasks 7-8.
//
// Serial commands: send 'd' + Enter to toggle detent-counting mode
// (Task 8); default mode on boot is live direction/rate reporting
// (Task 7).

#include "Wheel.h"

Wheel wheel;

bool detent_mode = false;
uint32_t last_report_ms = 0;
uint32_t last_total_bursts_seen = 0;
uint32_t last_burst_ms = 0;
uint32_t detent_burst_count = 0;
bool detent_in_progress = false;
const uint32_t kDetentIdleTimeoutMs = 100;

WheelPulseDecoder::Direction last_direction = WheelPulseDecoder::Direction::kUnknown;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  wheel.begin();
  Serial.println("Ready. Spin the wheel. Send 'd' to toggle detent-counting mode.");
}

void reportDirectionMode(uint32_t now) {
  WheelPulseDecoder::Direction direction = wheel.update();
  if (direction != WheelPulseDecoder::Direction::kUnknown &&
      direction != last_direction) {
    Serial.print("Direction changed to: ");
    Serial.println(direction == WheelPulseDecoder::Direction::kHighTripleRate
                        ? "HIGH triple rate"
                        : "LOW triple rate");
    last_direction = direction;
  }

  if (now - last_report_ms >= 1000) {
    last_report_ms = now;
    uint32_t bursts = wheel.totalBursts();
    uint32_t burst_rate = bursts - last_total_bursts_seen;  // bursts in the last ~1s
    last_total_bursts_seen = bursts;
    Serial.print("total bursts=");
    Serial.print(bursts);
    Serial.print(" total triples=");
    Serial.print(wheel.totalTriples());
    Serial.print(" bursts/sec(approx)=");
    Serial.println(burst_rate);
  }
}

void reportDetentMode(uint32_t now) {
  uint32_t bursts = wheel.totalBursts();
  if (bursts != last_total_bursts_seen) {
    uint32_t new_bursts = bursts - last_total_bursts_seen;
    detent_burst_count += new_bursts;
    last_total_bursts_seen = bursts;
    last_burst_ms = now;
    detent_in_progress = true;
  }

  if (detent_in_progress && (now - last_burst_ms >= kDetentIdleTimeoutMs)) {
    Serial.print("Detent complete: ");
    Serial.print(detent_burst_count);
    Serial.println(" bursts");
    detent_burst_count = 0;
    detent_in_progress = false;
  }
}

void loop() {
  uint32_t now = millis();

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'd') {
      detent_mode = !detent_mode;
      detent_burst_count = 0;
      detent_in_progress = false;
      last_total_bursts_seen = wheel.totalBursts();
      Serial.print("Detent-counting mode: ");
      Serial.println(detent_mode ? "ON" : "OFF");
    }
  }

  if (detent_mode) {
    reportDetentMode(now);
  } else {
    reportDirectionMode(now);
  }
}
```

- [ ] **Step 2: Upload and run Experiment A**

Upload via `arduino-cli compile --fqbn adafruit:nrf52:feather52840 --upload
scratch/wheel_calibration` (double-tap-equivalent RST/GND jumper trick from
`docs/hardware-bringup-log.md` if needed to enter bootloader mode first).
Open Serial Monitor at 115200 baud.

With the mouse's own battery installed and now the nice!nano also powered
(via USB), spin the wheel steadily one way for ~5 seconds, then the other
way for ~5 seconds, same as the investigation's stopwatch-timed captures.

**Expected, and report back exactly what you see:**
- The `bursts/sec(approx)` figure while actively spinning should be in the
  same ballpark as the investigation's logic-analyzer figure (roughly
  2000-2600/sec). If it's dramatically lower, the ISR is likely dropping
  edges — stop and flag this before trusting anything else from this
  sketch (Review Focus item 5).
- "Direction changed to: ..." should print once per real direction
  reversal, not randomly flip during steady spinning in one direction.

---

### Task 8: Hands-on — calibration sketch, Experiment B (bursts per detent)

**Files:** None — uses the sketch from Task 7 as-is (detent mode already built in).

- [ ] **Step 1: Switch to detent-counting mode**

With the sketch from Task 7 still running, send `d` followed by Enter in
the Serial Monitor. Confirm it prints "Detent-counting mode: ON".

- [ ] **Step 2: Measure bursts-per-detent, one notch at a time**

Turn the wheel exactly one physical detent (one click-feel), then wait —
after about 100ms with no new activity, the sketch prints "Detent complete:
N bursts". Repeat this for **at least 15 separate detents**, mixing both
directions and a couple of different turning speeds, and record every "N
bursts" value reported.

- [ ] **Step 3: Report the results**

Give Claude the full list of recorded "N bursts" values (and which
direction/speed each one was, if you kept track) so Task 9 can decide the
detent-to-burst mapping — whether it's a roughly constant ratio, scales
with turning speed, or needs a different approach entirely (e.g. detecting
the pause between detents rather than counting bursts, if the ratio turns
out too inconsistent to use directly).

---

### Task 9: Apply calibration results

**Files:**
- Modify: `firmware/WheelPulseDecoder.h` (constants, if Task 7's real-world
  numbers disagree with the logic-analyzer-derived starting values)
- Modify: `firmware/Wheel.h`/`.cpp` (direction-to-physical-rotation mapping,
  and a detent-counting mechanism based on Task 8's data)
- Modify: `test/test_wheel_pulse_decoder.cpp` (if constants changed)

This task's exact content depends on Tasks 7-8's real results, which don't
exist yet — writing concrete steps now would mean guessing at data this
plan deliberately doesn't invent (see Global Constraints). Once Task 8's
burst-count list comes back:

- [ ] **Step 1: Re-run `make test` after any constant changes to confirm
  nothing regresses**
- [ ] **Step 2: Update `WheelPulseDecoder.h`'s doc comment to remove the
  "NOT YET CALIBRATED" warning, replaced with the real figures and the
  date they were confirmed**
- [ ] **Step 3: Decide and implement the detent-counting approach in
  `Wheel`, informed by Task 8's data**
- [ ] **Step 4: Commit**

```bash
git add firmware/WheelPulseDecoder.h firmware/Wheel.h firmware/Wheel.cpp \
        test/test_wheel_pulse_decoder.cpp
git commit -m "feat(firmware): calibrate wheel decode constants against real hardware"
```

---

## Definition of Done

- [ ] `make test` passes with all `BurstGrouper`/`WheelPulseDecoder` test
      cases green.
- [ ] `firmware/BurstGrouper.h`/`.cpp` and `firmware/WheelPulseDecoder.h`/
      `.cpp` have zero Arduino/hardware dependencies — confirm with `grep
      -ri arduino firmware/BurstGrouper.* firmware/WheelPulseDecoder.*`
      returning nothing.
- [ ] Every Review Focus item above has a corresponding test (items 1-4) or
      an explicit hands-on check (item 5).
- [ ] Task 6's physical wiring is in place and multimeter-confirmed.
- [ ] Task 7's real-hardware burst rate roughly matches the logic
      analyzer's figures, and direction classification visibly tracks real
      spin reversals on the actual nice!nano.
- [ ] Task 8's bursts-per-detent data has been gathered and Task 9 has
      applied it — `WheelPulseDecoder`'s "NOT YET CALIBRATED" warning is
      gone, replaced with real, dated figures.
- [ ] `Wheel` is not yet wired into `autoclicker.ino`/`Clicker` — that's
      follow-on work once this plan's calibration is done, not part of
      this plan.
