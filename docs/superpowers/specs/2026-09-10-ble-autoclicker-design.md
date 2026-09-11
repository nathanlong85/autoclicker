# BLE Auto-Clicker — Design

**Status:** approved, ready for implementation planning
**Date:** 2026-09-10

## Overview

A Bluetooth auto-clicker built into a gutted mouse shell, for Colin (12, Nate's son) to
use with Geometry Dash on PC, iPhone, and Android. Father-son project: Colin owns the
clicker logic, Nate owns soldering/assembly, Claude owns BLE/HID, flashing, debugging,
and the hardware-facing firmware. Full hardware/firmware background is in
`~/Downloads/PROJECT_HANDOFF.md`.

## Hardware

- **Board:** nice!nano v2 (Nordic nRF52840, Cortex-M4F @ 64 MHz, 1 MB flash / 256 KB
  RAM), running the Adafruit UF2 bootloader. Currently flashed with CircuitPython —
  will be overwritten.
- **Battery:** 1050 mAh 3.7 V LiPo soldered to BAT pads; USB-C charging via onboard PMIC.
- **Inputs:** left switch, right switch, scroll-wheel quadrature encoder — desoldered
  from the donor mouse and rewired to nRF52840 GPIO. Pin assignments are decided during
  implementation, not this design.
- **Language/toolchain:** Arduino IDE, Adafruit nRF52 board core ("nice!nano"),
  Adafruit Bluefruit BLE HID library. Not CircuitPython — fast, reliable click timing
  is the project's core requirement, and CircuitPython's BLE HID + timing at the fast
  end of the range (20 ms) was judged too uncertain for that.

## Scope

**v1:** clicking behavior only, no persistence, no low-power mode, no display.

**Deferred to v2+:** flash persistence of click speed across power cycles, deep
sleep/low-power mode, a 0.42" I2C OLED (72×40, SSD1306, driven via the U8g2 library —
Adafruit_SSD1306 doesn't handle this panel's RAM-window offset cleanly) showing the
current interval, LED feedback at min/max speed. None of these affect the v1
architecture; they're additive.

Mouse cursor movement is out of scope entirely — this is a clicker, not a mouse
replacement. The HID descriptor only needs to declare buttons.

## Behavior

- **Left button:** always a plain left-click passthrough to the host. Unaffected by
  autoclick state.
- **Right button — hold-to-run:** while physically held, streams left-clicks at the
  current interval; releasing stops immediately. No toggle, no persisted "armed" state.
  The host never receives an actual right-click — the button is fully repurposed. The
  first click on press is immediate (not delayed by a full interval), for
  responsiveness. Pending final confirmation from Colin (see Open Questions).
- **Scroll wheel:** adjusts the click interval, ~10 ms per detent, clamped to
  [20 ms, 150 ms] (50–6.7 clicks/sec), default 50 ms (20 clicks/sec) at startup. No
  scroll events reach the host. Direction (which way = faster) and step size are
  tunable — see Open Questions.
- Manual left clicks work normally on top of an active autoclick stream; they don't
  interfere with the click timer.

## Architecture

Three units, one small shared contract:

```
autoclicker/
  core/
    Clicker.h / Clicker.cpp        — the clicker brain (Colin's)
  firmware/
    Button.h/.cpp                  — GPIO + Debouncer, thin
    Debouncer.h/.cpp               — pure debounce logic (Claude's)
    Wheel.h/.cpp                   — GPIO + QuadratureDecoder, thin
    QuadratureDecoder.h/.cpp       — pure quadrature decode logic (Claude's)
    HidButtonState.h/.cpp          — composes "held" + "pulse" into HID report state
    Mouse.h/.cpp                   — thin wrapper over Adafruit Bluefruit HID
    autoclicker.ino                — setup()/loop(), wires everything together
  test/
    doctest.h                      — vendored single-header test framework
    test_clicker.cpp               — Colin's tests for core/
    test_debouncer.cpp             — Claude's tests for firmware/'s pure pieces
    test_quadrature_decoder.cpp
    test_hid_button_state.cpp
  Makefile                         — `make test` builds + runs test/ natively (macOS,
                                      no Arduino toolchain)
  CLAUDE.md
```

### `core::Clicker` (Colin's, object-oriented)

```cpp
class Clicker {
public:
  void leftButton(bool down);
  void rightButton(bool down);
  void scroll(int detents);
  void update(uint32_t now_ms);

  bool shouldClickNow() const;   // one-shot: true only on the tick a click fires
  uint16_t intervalMs() const;
};
```

Non-blocking: `update()` is called every `loop()` iteration (thousands of times/sec)
and only acts once enough time has actually passed — there is no `delay()` anywhere in
the system. State held internally: current interval, last-click timestamp, whether the
right button is held.

Logic:
- `leftButton()` only affects passthrough (handled by `firmware/`, not `Clicker`'s
  click-stream state).
- `rightButton(down)` just records held/not-held; no timer starts here.
- `scroll(detents)`: `intervalMs -= detents * 10`, clamped to [20, 150].
- `update(now_ms)`: if held and `now_ms - lastClickTime >= intervalMs` (or this is the
  first tick since becoming held), mark a click due and record `lastClickTime`.

Edge cases the test suite must cover: immediate click on press; no click and no leftover
state after an early release; a speed change mid-stream takes effect on the *next*
click without corrupting timing; clamping at both ends of the range; a manual left
click during an active stream doesn't disturb it.

### `firmware/` (Claude's)

- **`Debouncer`** — pure: `bool update(bool raw_reading, uint32_t now_ms)`. `Button`
  wraps one `digitalRead()` call around it — that one line is the only untested part of
  input handling.
- **`QuadratureDecoder`** — pure: `int update(bool pin_a, bool pin_b)` → -1/0/+1 detents.
  `Wheel` wraps two `digitalRead()` calls around it.
- **`HidButtonState`** — pure: composes a real held-left-button state with autoclick
  "pulses" into the correct sequence of HID press/release reports, so an autoclick
  pulse firing while the player is genuinely holding left doesn't emit a spurious
  release. `Mouse` wraps the actual Bluefruit `sendReport()` call around it.
- **`autoclicker.ino`** — owns `setup()`/`loop()`, reads `Button`/`Wheel`, drives
  `Clicker`, drives `Mouse`. No behavioral logic of its own.

Bluefruit's `BLEHidAdafruit` handles BLE mouse advertising and the HID descriptor; no
hand-rolled HID report format is needed.

## Testing

| Layer | Tests | Author |
|---|---|---|
| `core::Clicker` | Unit, `test/test_clicker.cpp`, host-run via `make test` | Colin |
| `firmware::Debouncer`, `QuadratureDecoder`, `HidButtonState` | Unit, host-run via `make test` | Claude |
| `Button`, `Wheel`, `Mouse` (one-line GPIO/BLE glue) | Not unit tested — covered by step 0 and final-assembly manual checks | — |
| End-to-end (real board, real inputs, real Bluetooth) | Manual checklist: pairs on Mac/iPhone/Android; left/right/wheel behavior matches spec; fits in shell; charges over USB-C | Nate + Colin |

`make test` compiles `core/` and `firmware/`'s pure classes plus `test/` with the host
`g++`/`clang++` — no Arduino involvement, runs in well under a second. Firmware is
still built/flashed the normal Arduino IDE way.

## Step 0 — hardware bring-up (before any `core/`/`firmware/` code)

Driven by Claude, Nate as hands-on-hardware:

1. Identify the board and confirm bootloader mode.
2. Install Arduino IDE + Adafruit nRF52 board core, select board "nice!nano".
3. Flash the stock Blink example — validates the toolchain end to end.
4. Flash a bare Bluefruit BLE HID mouse sketch (no `Clicker` yet) and confirm pairing +
   a manual click on Mac, iPhone, and Android.

Only once all four are green does `core/`/`firmware/` implementation start.

## Open questions (for Colin, not architectural)

These don't change the design — they're parameter/behavior choices Colin makes by
feel once he's using the real device:

1. Confirm hold-to-run (vs. a toggle) feels right for Geometry Dash.
2. Which scroll direction should increase speed.
3. Whether 10 ms/detent feels right, or should be finer/coarser.
4. Whether 50 ms is the right default startup speed.
5. Whether the 20–150 ms range is the right fastest/slowest bound.

## Out of scope for this design

- Pin assignments (implementation detail, decided during step 0/wiring).
- v2 features listed above.
- OpenSpec/Comet-style process — this project uses brainstorming → design doc →
  writing-plans → implementation directly; no phase-gated workflow.
