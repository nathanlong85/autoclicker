# core/Clicker Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Human-in-the-loop note:** this is Colin's code. Whoever executes this plan
> should pair with him at the keyboard — he writes/types the test and the
> implementation with guidance, not the other way around. Read/write access to
> `core/` and `test/` only; nothing here touches BLE, GPIO, or `firmware/`.

**Goal:** A fully unit-tested `Clicker` class implementing the auto-click
state machine (right-button hold-to-run, scroll-adjustable speed), runnable
and testable entirely on a Mac with no hardware or Arduino toolchain.

**Architecture:** A single `Clicker` class in `core/`, built and tested with
plain `g++`/`clang++` via a `Makefile`, using the doctest testing framework
(single vendored header, no install). No dependency on Arduino headers
anywhere in `core/` or `test/`.

**Tech Stack:** C++17, doctest (vendored header, v2.4.11), `make`.

**Spec:** `docs/superpowers/specs/2026-09-10-ble-autoclicker-design.md`

## Global Constraints

- `core/` and `test/` must compile with plain `g++`/`clang++` — no Arduino
  headers, no `Arduino.h`, no board-specific types. `uint32_t`/`uint16_t`
  come from `<cstdint>`.
- Click interval range: `[20, 150]` ms, clamped, in steps of `10` ms per
  scroll detent, default `50` ms.
- `Clicker` has no `leftButton()` method — left-click passthrough is entirely
  `firmware/`'s responsibility (see design doc).
- Every behavior in the design doc's "Edge cases the test suite must cover"
  list gets a test in this plan.

---

### Task 1: Project scaffolding — vendor doctest, Makefile, first passing test

**Files:**
- Create: `test/doctest.h` (vendored, not hand-written)
- Create: `test/test_main.cpp`
- Create: `Makefile`
- Create: `core/Clicker.h`
- Create: `core/Clicker.cpp`
- Test: `test/test_clicker.cpp`

**Interfaces:**
- Produces: `class Clicker` with `Clicker()`, `void rightButton(bool down)`,
  `void scroll(int detents)`, `void update(uint32_t now_ms)`,
  `bool shouldClickNow() const`, `uint16_t intervalMs() const`. Later tasks
  fill in behavior; this task establishes the shape and build pipeline.

- [ ] **Step 1: Vendor doctest**

```bash
curl -fsSL -o test/doctest.h https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
```
Expected: `test/doctest.h` exists and is a large (~6000 line) single header
file. Verify with `wc -l test/doctest.h` — expect several thousand lines, not
an HTML error page.

- [ ] **Step 2: Write the doctest main-file**

```cpp
// test/test_main.cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```

- [ ] **Step 3: Write the Makefile**

```makefile
CXX := c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Icore -Itest

TEST_SRCS := test/test_main.cpp test/test_clicker.cpp core/Clicker.cpp
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

- [ ] **Step 4: Write the Clicker interface**

```cpp
// core/Clicker.h
#pragma once
#include <cstdint>

class Clicker {
public:
  Clicker();

  void rightButton(bool down);
  void scroll(int detents);
  void update(uint32_t now_ms);

  bool shouldClickNow() const;
  uint16_t intervalMs() const;

private:
  uint16_t interval_ms_;
  uint32_t last_click_time_;
  bool right_held_;
  bool click_due_;
  bool clicked_since_held_;
};
```

- [ ] **Step 5: Write the failing test**

```cpp
// test/test_clicker.cpp
#include "doctest.h"
#include "Clicker.h"

TEST_CASE("a new Clicker defaults to 50ms interval and never clicks unheld") {
  Clicker c;
  CHECK(c.intervalMs() == 50);
  c.update(0);
  CHECK_FALSE(c.shouldClickNow());
}
```

- [ ] **Step 6: Run it to verify it fails**

Run: `make test`
Expected: FAIL — linker error, undefined symbols for `Clicker::Clicker()`,
`Clicker::update(unsigned int)`, etc. (Clicker.cpp doesn't exist yet.)

- [ ] **Step 7: Write the minimal implementation**

```cpp
// core/Clicker.cpp
#include "Clicker.h"

namespace {
constexpr uint16_t kDefaultIntervalMs = 50;
}

Clicker::Clicker()
    : interval_ms_(kDefaultIntervalMs),
      last_click_time_(0),
      right_held_(false),
      click_due_(false),
      clicked_since_held_(false) {}

void Clicker::rightButton(bool down) {
  right_held_ = down;
}

void Clicker::scroll(int detents) {
  (void)detents;  // implemented in Task 5
}

void Clicker::update(uint32_t now_ms) {
  (void)now_ms;
  click_due_ = false;  // implemented further in Task 2
}

bool Clicker::shouldClickNow() const { return click_due_; }
uint16_t Clicker::intervalMs() const { return interval_ms_; }
```

- [ ] **Step 8: Run test to verify it passes**

Run: `make test`
Expected: PASS — `[doctest] test cases: 1 | 1 passed | 0 failed`

- [ ] **Step 9: Commit**

```bash
git add test/doctest.h test/test_main.cpp test/test_clicker.cpp Makefile core/Clicker.h core/Clicker.cpp
git commit -m "test(core): scaffold Clicker build/test pipeline with doctest"
```

---

### Task 2: Immediate click on right-button press

**Files:**
- Modify: `core/Clicker.cpp`
- Test: `test/test_clicker.cpp`

**Interfaces:**
- Consumes: `Clicker` from Task 1 (no signature changes).
- Produces: `rightButton()`/`update()` now set `shouldClickNow()` true on the
  very first `update()` after a press — later tasks build on this.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("holding right immediately clicks on the next update") {
  Clicker c;
  c.rightButton(true);
  c.update(0);
  CHECK(c.shouldClickNow());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test`
Expected: FAIL — second test case fails, `shouldClickNow()` returns false.

- [ ] **Step 3: Implement it**

```cpp
void Clicker::update(uint32_t now_ms) {
  click_due_ = false;
  if (!right_held_) return;

  if (!clicked_since_held_) {
    click_due_ = true;
    clicked_since_held_ = true;
    last_click_time_ = now_ms;
  }
}
```

- [ ] **Step 4: Run tests to verify both pass**

Run: `make test`
Expected: PASS — `2 | 2 passed | 0 failed`

- [ ] **Step 5: Commit**

```bash
git add core/Clicker.cpp test/test_clicker.cpp
git commit -m "feat(core): immediate click on right-button press"
```

---

### Task 3: Click repeats at the current interval while held

**Files:**
- Modify: `core/Clicker.cpp`
- Test: `test/test_clicker.cpp`

**Interfaces:** No signature changes.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("held right button clicks again once the interval elapses, not before") {
  Clicker c;
  c.rightButton(true);
  c.update(0);
  CHECK(c.shouldClickNow());        // immediate click

  c.update(30);
  CHECK_FALSE(c.shouldClickNow());  // 30ms < 50ms default interval

  c.update(50);
  CHECK(c.shouldClickNow());        // 50ms elapsed since last click
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test`
Expected: FAIL — third assertion fails (`update(50)` doesn't click because
`update()` currently never fires again after the first click).

- [ ] **Step 3: Implement it**

```cpp
void Clicker::update(uint32_t now_ms) {
  click_due_ = false;
  if (!right_held_) return;

  if (!clicked_since_held_) {
    click_due_ = true;
    clicked_since_held_ = true;
    last_click_time_ = now_ms;
    return;
  }

  if (now_ms - last_click_time_ >= interval_ms_) {
    click_due_ = true;
    last_click_time_ = now_ms;
  }
}
```

- [ ] **Step 4: Run tests to verify all pass**

Run: `make test`
Expected: PASS — `3 | 3 passed | 0 failed`

- [ ] **Step 5: Commit**

```bash
git add core/Clicker.cpp test/test_clicker.cpp
git commit -m "feat(core): repeat clicks at the current interval while held"
```

---

### Task 4: Releasing stops the stream with no leftover state

**Files:**
- Modify: `core/Clicker.cpp`
- Test: `test/test_clicker.cpp`

**Interfaces:** No signature changes.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("releasing right stops clicking, and a fresh press clicks immediately again") {
  Clicker c;
  c.rightButton(true);
  c.update(0);
  CHECK(c.shouldClickNow());

  c.rightButton(false);
  c.update(10);
  CHECK_FALSE(c.shouldClickNow());  // released, no click even though time passed

  c.rightButton(true);
  c.update(15);
  CHECK(c.shouldClickNow());        // fresh press clicks immediately, not after
                                     // waiting out the old interval
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test`
Expected: FAIL — third assertion fails. `clicked_since_held_` is never reset
on release, so the re-press is treated as still mid-stream and waits for the
interval instead of clicking immediately.

- [ ] **Step 3: Implement it**

```cpp
void Clicker::rightButton(bool down) {
  right_held_ = down;
  if (!down) {
    clicked_since_held_ = false;
  }
}
```

- [ ] **Step 4: Run tests to verify all pass**

Run: `make test`
Expected: PASS — `4 | 4 passed | 0 failed`

- [ ] **Step 5: Commit**

```bash
git add core/Clicker.cpp test/test_clicker.cpp
git commit -m "feat(core): reset click-stream state on release"
```

---

### Task 5: Scroll adjusts the interval, clamped to [20, 150]

**Files:**
- Modify: `core/Clicker.cpp`
- Test: `test/test_clicker.cpp`

**Interfaces:**
- `scroll(int detents)`: positive `detents` speeds up (lowers `intervalMs()`
  by `5` per detent, per Colin's "small nudges" choice), negative slows down.
  Direction of the physical wheel (which rotation produces positive vs.
  negative `detents`) is `firmware/`'s `Wheel`'s job — Colin chose "up on the
  wheel = slower," so `Wheel` maps physical-up to negative detents. That
  mapping lives entirely in `firmware/`, not here.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("scroll adjusts interval by 5ms per detent, clamped to [20, 150]") {
  Clicker faster;
  faster.scroll(2);
  CHECK(faster.intervalMs() == 40);  // 50 - 10

  Clicker clampLow;
  clampLow.scroll(10);               // would be 50 - 50 = 0
  CHECK(clampLow.intervalMs() == 20);

  Clicker clampHigh;
  clampHigh.scroll(-21);             // would be 50 + 105 = 155
  CHECK(clampHigh.intervalMs() == 150);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test`
Expected: FAIL — `scroll()` currently does nothing, so `intervalMs()` stays
50 in all three cases.

- [ ] **Step 3: Implement it**

```cpp
namespace {
constexpr uint16_t kMinIntervalMs = 20;
constexpr uint16_t kMaxIntervalMs = 150;
constexpr uint16_t kStepMs = 5;
constexpr uint16_t kDefaultIntervalMs = 50;
}  // (replaces the single-constant anonymous namespace from Task 1)

void Clicker::scroll(int detents) {
  int32_t proposed = static_cast<int32_t>(interval_ms_) - detents * kStepMs;
  if (proposed < kMinIntervalMs) proposed = kMinIntervalMs;
  if (proposed > kMaxIntervalMs) proposed = kMaxIntervalMs;
  interval_ms_ = static_cast<uint16_t>(proposed);
}
```

- [ ] **Step 4: Run tests to verify all pass**

Run: `make test`
Expected: PASS — `5 | 5 passed | 0 failed`

- [ ] **Step 5: Commit**

```bash
git add core/Clicker.cpp test/test_clicker.cpp
git commit -m "feat(core): scroll adjusts click interval, clamped to [20, 150]ms"
```

---

### Task 6: Speed change mid-stream takes effect on the next click only

**Files:**
- Test: `test/test_clicker.cpp` (no production code change expected — see note)

**Interfaces:** No signature changes.

- [ ] **Step 1: Write the test**

```cpp
TEST_CASE("changing speed mid-stream affects the next click, timing stays anchored") {
  Clicker c;
  c.rightButton(true);
  c.update(0);
  CHECK(c.shouldClickNow());   // click #1 at t=0, interval 50ms

  c.scroll(-1);                // slow down slightly: interval becomes 55ms
  CHECK(c.intervalMs() == 55);

  c.update(50);
  CHECK_FALSE(c.shouldClickNow());  // only 50ms elapsed, new interval needs 55

  c.update(55);
  CHECK(c.shouldClickNow());   // now 55ms have elapsed since click #1
}
```

- [ ] **Step 2: Run it**

Run: `make test`
Expected: **PASS immediately** — `update()` already reads `interval_ms_`
fresh on every call (Task 3's implementation), so a speed change mid-stream
is already handled correctly. This test is here as a regression guard for
that behavior, not because new production code is needed; nothing to
implement in this task.

- [ ] **Step 3: Commit**

```bash
git add test/test_clicker.cpp
git commit -m "test(core): lock in mid-stream speed-change timing behavior"
```

---

## Definition of Done

- [ ] `make test` passes with all 6 test cases green.
- [ ] Every edge case in the design doc's "Edge cases the test suite must
      cover" list has a corresponding test (Tasks 2–6 cover them all; the
      "manual left click during a stream" case is explicitly out of scope
      here per the design doc's Task-1 note above).
- [ ] `core/Clicker.h`/`.cpp` have zero Arduino/hardware dependencies —
      confirm with `grep -ri arduino core/` returning nothing.
