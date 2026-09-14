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
- **Power switch (v1, added 2026-09-13):** the donor mouse's bottom switch is rewired
  between the nice!nano's **EN** pin and GND, not in series with the battery lead. EN
  low disables the onboard regulator (cutting power to the nRF52840/GPIO) while leaving
  BAT+/BAT- — and USB-C charging through them — untouched. A switch in series with BAT+
  was the first instinct but would have blocked charging while off, since BAT+ feeds
  the onboard charge IC directly. EN pin location to be confirmed against this board's
  silkscreen during wiring — not expected to differ from the Feather-family reference
  design nice!nano follows, but not yet visually verified.
- **Inputs:** left switch, right switch, scroll-wheel quadrature encoder, wheel-click
  switch (pairing button, v1, added 2026-09-13) — desoldered from the donor mouse and
  rewired to nRF52840 GPIO. Pin assignments are decided during implementation, not this
  design. The scroll wheel is an **optical** quadrature encoder (light-interrupter +
  slotted disc), not mechanical contacts — same digital two-channel quadrature
  interface either way, but wiring/tracing it out differs from a simple switch.
- **Language/toolchain:** Arduino IDE, Adafruit nRF52 board core ("nice!nano"),
  Adafruit Bluefruit BLE HID library. Not CircuitPython — fast, reliable click timing
  is the project's core requirement, and CircuitPython's BLE HID + timing at the fast
  end of the range (20 ms) was judged too uncertain for that.

## Scope

**v1:** clicking behavior, an RGB LED speed/connection indicator (see below, added
2026-09-10), and a pairing button (see below, added 2026-09-13). No persistence, no
low-power mode, no OLED display yet.

**Deferred to v2+:** flash persistence of click speed across power cycles, deep
sleep/low-power mode, and a 0.42" I2C OLED (72×40, SSD1306, driven via the U8g2
library — Adafruit_SSD1306 doesn't handle this panel's RAM-window offset cleanly)
showing the exact interval once Nate/Colin get one — the RGB LED is the stand-in
until then. None of these affect the v1 architecture; they're additive.

**Persistence approach decided now (2026-09-13), implementation still deferred:** v2's
saved click-speed will write to internal flash on a ~500 ms debounce after the wheel
settles, rather than on a shutdown signal. This was decided alongside the v1 power
switch specifically so that choice (a plain EN/GND cutoff, no GPIO involvement) doesn't
need to change when v2 adds persistence — there's no "about to lose power" moment for
firmware to catch, since the stored value is always already current.

Mouse cursor movement is out of scope entirely — this is a clicker, not a mouse
replacement. The HID descriptor only needs to declare buttons.

## Behavior

- **Left button:** always a plain left-click passthrough to the host. Unaffected by
  autoclick state.
- **Right button — hold-to-run:** while physically held, streams left-clicks at the
  current interval; releasing stops immediately. No toggle, no persisted "armed" state.
  The host never receives an actual right-click — the button is fully repurposed. The
  first click on press is immediate (not delayed by a full interval), for
  responsiveness. Confirmed by Colin (2026-09-10).
- **Scroll wheel:** adjusts the click interval, **5 ms per detent** (Colin wanted finer
  control over a bigger jump), clamped to [20 ms, 150 ms] (50–6.7 clicks/sec), default
  50 ms (20 clicks/sec) at startup — both confirmed by Colin. No scroll events reach the
  host. **Rolling the wheel up = slower** (Colin's choice — counterintuitive vs. a
  typical "scroll up = more/faster" convention, so `firmware/`'s `Wheel` must map
  physical "up" rotation to a *negative* detent value going into `Clicker::scroll()`,
  where positive still means faster internally).
- Manual left clicks work normally on top of an active autoclick stream; they don't
  interfere with the click timer.
- **Wheel-click button — pairing (v1, added 2026-09-13):** the scroll wheel's own click
  switch, otherwise unused, doubles as a pairing button. Holding it for a continuous
  5000 ms disconnects the current BLE connection, clears stored bond data, and resumes
  advertising — a full "forget and re-pair," not just a disconnect, so a still-nearby
  old host can't win the reconnect race against whatever host connects next. Releasing
  before 5000 ms does nothing. There is no distinct "pairing mode" LED state — once the
  bond is wiped, the board is simply advertising with no connection, which is already
  the existing "not paired" state below.

**RGB LED indicator (v1, added 2026-09-10; remapped 2026-09-13):** a common-4-leg RGB
LED shows connection and speed status, standing in until an OLED is added in v2:

| State | Color |
|---|---|
| Not paired (advertising) — never-paired or just wiped via the pairing button | Blue, slow breathing pulse |
| Paired — idle or autoclicking, no distinction | Gradient by current speed: green (fastest, 20 ms) → yellow (mid) → red (slowest, 150 ms) |

The gradient direction flips traffic-light convention (green = go fast, red = slow
down) from the original green-slowest/red-fastest heat-map mapping — more intuitive for
a 12-year-old who hasn't worked with heat maps. The paired-idle/paired-clicking
distinction (previously: idle showed the LED off) is dropped — the LED always shows
current speed while paired, matching the eventual v2 OLED always showing the exact
interval.

Exact GPIO pins and common-anode-vs-cathode polarity are decided during Step 0/wiring
once the LED is in hand — not an architectural detail.

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
    PairingButton.h/.cpp           — GPIO + Debouncer + LongPressDetector, thin
                                      (v1, added 2026-09-13)
    LongPressDetector.h/.cpp       — pure long-press timing logic (Claude's)
                                      (v1, added 2026-09-13)
    Mouse.h/.cpp                   — thin wrapper over Adafruit Bluefruit HID,
                                      incl. forgetAndRestartAdvertising() (2026-09-13)
    LedColorPicker.h/.cpp          — pure: (connected, intervalMs) -> RGB
                                      (clicking dropped 2026-09-13)
    SpeedLed.h/.cpp                — thin: 3x analogWrite() around LedColorPicker
    autoclicker.ino                — setup()/loop(), wires everything together
  test/
    doctest.h                      — vendored single-header test framework
    test_clicker.cpp               — Colin's tests for core/
    test_debouncer.cpp             — Claude's tests for firmware/'s pure pieces
    test_quadrature_decoder.cpp
    test_hid_button_state.cpp
    test_long_press_detector.cpp   — (v1, added 2026-09-13)
    test_led_color_picker.cpp
  Makefile                         — `make test` builds + runs test/ natively (macOS,
                                      no Arduino toolchain)
  CLAUDE.md
```

### `core::Clicker` (Colin's, object-oriented)

```cpp
class Clicker {
public:
  void rightButton(bool down);
  void scroll(int detents);
  void update(uint32_t now_ms);

  bool shouldClickNow() const;   // one-shot: true only on the tick a click fires
  uint16_t intervalMs() const;
};
```

`Clicker` has no `leftButton()` method — left-click passthrough is fully handled by
`firmware/`, which mirrors the physical left button's raw down/up state 1:1 into the
BLE HID report every loop tick (press sends press, held stays held, release sends
release). It behaves exactly like a normal wired mouse button and never touches
`Clicker`'s state, so `Clicker` doesn't need to know it exists.

Non-blocking: `update()` is called every `loop()` iteration (thousands of times/sec)
and only acts once enough time has actually passed — there is no `delay()` anywhere in
the system. State held internally: current interval, last-click timestamp, whether the
right button is held.

Logic:
- `rightButton(down)` just records held/not-held; no timer starts here.
- `scroll(detents)`: `intervalMs -= detents * 5`, clamped to [20, 150]. Positive
  `detents` means faster (lower interval) — `firmware/`'s `Wheel` is responsible for
  turning "physical up rotation" into a *negative* value, per Colin's chosen direction.
- `update(now_ms)`: if held and `now_ms - lastClickTime >= intervalMs` (or this is the
  first tick since becoming held), mark a click due and record `lastClickTime`.

Edge cases the test suite must cover: immediate click on press; no click and no leftover
state after an early release; a speed change mid-stream takes effect on the *next*
click without corrupting timing; clamping at both ends of the range. (A manual left
click during an active right-hold isn't a `Clicker` test case — it's satisfied by
`firmware/` mirroring the left button independently, verified in the end-to-end
manual checklist instead.)

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
- **`LedColorPicker`** — pure: `RGB colorFor(bool connected, uint16_t intervalMs)`
  (dropped the `clicking` bool 2026-09-13 — paired no longer distinguishes idle from
  clicking). Not paired → blue (breathing handled by `SpeedLed`'s timing, not this
  function — it just returns "blue" as the target color to breathe with); paired →
  green→yellow→red gradient interpolated from `intervalMs` across [20, 150] (green =
  fastest/20ms, red = slowest/150ms). `SpeedLed` wraps three `analogWrite()` calls (PWM,
  one per RGB leg) around it — that's the only untested part.
- **`LongPressDetector`** (v1, added 2026-09-13) — pure: `bool update(bool held,
  uint32_t now_ms)`, returns true exactly once after `held` has been continuously true
  for 5000 ms, and resets as soon as `held` goes false. `PairingButton` wraps one
  `digitalRead()` + a `Debouncer` + this around it — that glue is the only untested
  part, same tier as `Button`/`Wheel`.
- **`Mouse::forgetAndRestartAdvertising()`** (v1, added 2026-09-13) — thin: wraps
  `Bluefruit.disconnect()` + `Bluefruit.clearBonds()` + restarting advertising. Called
  once, when `PairingButton` reports a triggered long-press. Untested glue, same tier as
  the rest of `Mouse`.
- **`autoclicker.ino`** — owns `setup()`/`loop()`, reads `Button`/`Wheel`/
  `PairingButton`, drives `Clicker`, drives `Mouse` and `SpeedLed`. No behavioral logic
  of its own.

Bluefruit's `BLEHidAdafruit` handles BLE mouse advertising and the HID descriptor; no
hand-rolled HID report format is needed. Connection state for `SpeedLed` comes from
`Bluefruit.connected()`.

## Testing

| Layer | Tests | Author |
|---|---|---|
| `core::Clicker` | Unit, `test/test_clicker.cpp`, host-run via `make test` | Colin |
| `firmware::Debouncer`, `QuadratureDecoder`, `HidButtonState`, `LedColorPicker`, `LongPressDetector` | Unit, host-run via `make test` | Claude |
| `Button`, `Wheel`, `PairingButton`, `Mouse`, `SpeedLed` (one-line GPIO/BLE/PWM glue) | Not unit tested — covered by step 0 and final-assembly manual checks | — |
| End-to-end (real board, real inputs, real Bluetooth) | Manual checklist: pairs on Mac/iPhone/Android; left/right/wheel behavior matches spec; holding the wheel-click button 5s forgets and re-pairs; LED shows the right color/state for both connection states across the speed range; power switch cuts power without blocking USB-C charging; fits in shell | Nate + Colin |

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

## Open questions — resolved by Colin (2026-09-10)

All five parameter/behavior choices flagged for Colin are now answered and reflected
above:

1. Hold-to-run (not a toggle) — **confirmed**.
2. Scroll direction — **up = slower**.
3. Step size — **5 ms/detent** (finer control over a bigger jump).
4. Default startup speed — **50 ms / 20 clicks-per-sec** — matches the original guess.
5. Speed range — **20–150 ms confirmed as-is**, no change to the fastest end.

He answered by picking from a set of plain-language options, not by reading this
document.

## v1 addendum — resolved by Nate (2026-09-13)

Three additions worked out with Nate, all reflected above:

1. Pairing button on the wheel-click switch — hold 5s → disconnect + clear bond +
   re-advertise (a full forget-and-re-pair, not just a disconnect).
2. LED speed gradient direction flipped to traffic-light convention (green=fastest,
   red=slowest), and the paired-idle/paired-clicking distinction dropped.
3. Power switch wired to the nice!nano's EN pin (regulator disable) rather than in
   series with the battery lead, so USB-C charging isn't blocked while the switch is
   off — caught before it became a wiring mistake, since BAT+ feeds the onboard charge
   IC directly. Decided alongside this: v2's flash persistence will write on a
   debounced settle rather than on a shutdown signal, so this switch choice doesn't
   need to change when that lands.

## Out of scope for this design

- Pin assignments (implementation detail, decided during step 0/wiring).
- v2 features listed above.
- OpenSpec/Comet-style process — this project uses brainstorming → design doc →
  writing-plans → implementation directly; no phase-gated workflow.
