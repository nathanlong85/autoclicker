# autoclicker — project rules

A Bluetooth auto-clicker built into a gutted mouse shell, for Colin (12) to use with
Geometry Dash on PC / iPhone / Android. Father-son project: Nate + Colin build, Claude
owns the hard parts.

Design doc (authoritative): `docs/superpowers/specs/2026-09-10-ble-autoclicker-design.md`.
Original hardware/firmware handoff: `~/Downloads/PROJECT_HANDOFF.md` (not in repo,
background only — superseded by the design doc and
`docs/scroll-wheel-investigation.md` wherever they disagree).

## Division of labor

- **Colin (12):** the `core/` clicker logic and how the clicker feels to use.
- **Nate:** soldering, wiring, battery, physical assembly.
- **Claude:** BLE/HID stack, flashing, debugging, `firmware/` hardware layer, wiring
  guidance, architecture decisions.

Keep hardware/architecture complexity out of Colin's way — hide it, don't present it as
a decision to him. He should see small incremental wins. When Nate asks, produce a
plain-language kid-facing decision list, not jargon.

**Default: assume Colin is participating in every step that isn't fully Claude-owned
plumbing** (BLE stack internals, flashing mechanics, low-level debugging). Don't ask
each time — Nate will say if Colin's sitting a given step out. In steps he's part of,
talk to him directly: assign him tasks, ask him questions, correct mistakes, teach
things. He's 12, smart for his age — casual and encouraging tone, not condescending,
not childish.

## Hardware

- **Board:** nice!nano v2 (Nordic nRF52840, Cortex-M4F @ 64 MHz, 1 MB flash / 256 KB RAM).
  Handoff doc calls it a "ProMicro nRF52840 clone" / once "Xiao" — it is a nice!nano v2.
  No EN pin exists on this board (confirmed against the official pinout) — see power
  switch below.
- Runs the Adafruit UF2 bootloader (0.6.0). **Step 0 is complete** (2026-09-11, see
  `docs/hardware-bringup-log.md`): toolchain confirmed, BLE HID mouse pairing confirmed
  on Mac/iPhone/Android. Two substitutions from the original plan, carried forward
  everywhere since: `arduino-cli` instead of the Arduino IDE GUI, and FQBN
  `adafruit:nrf52:feather52840` instead of a real "nice!nano" board entry (none exists
  in the installed core — Arduino D-numbers map through that board's `variant.cpp`, not
  the nice!nano's silkscreen labels; same gotcha bit the bare BLE mouse sketch's button
  pin, see the log).
- Battery: 1050 mAh 3.7 V LiPo, soldered to BAT pads. USB-C charging via onboard PMIC.
- Inputs: left switch, right switch, scroll wheel (single-pin pulse-burst signal, not
  quadrature — see `docs/scroll-wheel-investigation.md`), wheel-click switch (doubles as
  a pairing button), power switch, scroll wheel left/right tilt buttons (confirmed to
  exist 2026-10-03, not yet electrically characterized — pins/signal type unknown) — all
  rewired from the original mouse to nRF52840 GPIO. Pin assignments TBD.
- The mouse sensor's power source (how `LQ1`/`LD2` get fed once the donor board's own
  battery is gone) is still an open question as of 2026-10-03 — see the design doc.

## Firmware

- **Language:** Arduino / C++ with the Adafruit nRF52 core + Bluefruit BLE library.
  FQBN `adafruit:nrf52:feather52840` (see Hardware above — not a literal "nice!nano"
  board entry). NOT CircuitPython — fast, solid click timing is the point of the
  project.
- App links at 0x26000 (with S140 SoftDevice) when keeping the bootloader.

### Architecture

- `core/Clicker` — pure C++ state machine (Colin's). No Arduino headers, no hardware.
  `rightButton(bool)`, `scroll(int detents)`, `update(uint32_t now_ms)`,
  `shouldClickNow()`, `intervalMs()` — non-blocking, never `delay()` anywhere.
  Compiles and unit-tests on the host (macOS). This is Colin's playground.
- `firmware/` — thin hardware layer (GPIO read + debounce, wheel pulse-burst decode,
  BLE HID mouse connection) wrapped around several small pure, host-testable classes
  (`Debouncer`, `BurstGrouper`, `WheelPulseDecoder`, `HidButtonState`,
  `LongPressDetector`, `LedColorPicker`). Calls into `core::Clicker` every loop. No
  game logic here. Full file-by-file breakdown in the design doc.

### Behavior (v1)

- **Left button:** plain left-click passthrough to the host.
- **Right button:** hold-to-run dead-man's switch — streams left-clicks at the current
  interval while held, stops on release. Host never receives a real right-click.
  Confirmed by Colin.
- **Scroll wheel:** adjusts click interval, **5 ms per detent**, clamped [20, 150] ms,
  default 50 ms. No host scroll events. **Up = slower** (Colin's choice, counterintuitive
  vs. the usual scroll convention). Decoded from a single-pin pulse-burst signal, not
  quadrature — see `docs/scroll-wheel-investigation.md`.
- **Wheel-click button:** doubles as a pairing button — hold 5s → forget bond +
  re-advertise (not just a disconnect), so an old host can't win the reconnect race.
- **Power switch:** plain GPIO read, not hardware power-gating (no EN pin on this
  board). "Off" → nRF52840 System OFF sleep with a GPIO sense-wake on that pin; "on" →
  resumes. Battery stays connected throughout, so USB-C charging is unaffected by
  switch position.

### Scope

- **v1:** clicking, an RGB LED speed/connection indicator, and a pairing button. No
  cursor movement (out of scope entirely), no persistence, no low-power mode, no OLED
  yet.
- **LED (v1):** common 4-leg RGB LED, stand-in for an OLED until Nate/Colin get one.
  Not paired → blue breathing pulse. Paired (idle or autoclicking — no distinction,
  dropped 2026-09-13) → gradient by current speed: **green (fastest, 20ms) → yellow →
  red (slowest, 150ms)** — traffic-light convention (green=go fast), flipped from an
  earlier green-slowest/red-fastest heat-map mapping. Pure color logic in
  `firmware/LedColorPicker` (tested); `firmware/SpeedLed` wraps 3 `analogWrite()` calls
  around it (untested glue). Pins/polarity decided during step 0/wiring.
- **v2 candidates:** flash persistence of speed, deep sleep / low power, 0.42" I2C OLED
  (72×40, SSD1306, U8g2 library) showing the exact speed value, and a battery-level
  readout on that same OLED — triggered by a press of the left wheel-tilt button,
  tabled 2026-10-03 rather than forced onto the v1 LED (already carrying 3 meanings:
  pairing state, idle, speed gradient — a 4th would overload it). Provisional behavior
  from that discussion, pending Colin's confirmation whenever this gets picked up:
  left tilt button (not hold, just press) shows battery status for ~5s; a scroll
  during that window finishes the battery display first, then catches up on the speed
  color; a second press while already showing does nothing (doesn't restart the timer).

### Step 0 (before any core logic) — complete, 2026-09-11

See `docs/hardware-bringup-log.md` for full results and the toolchain substitutions
noted under Hardware above.

1. Verify board + bootloader. ✅
2. Flash a blink sketch — proves the toolchain. ✅
3. Flash a bare BLE HID mouse sketch — confirm pairing + manual click on Mac / iPhone /
   Android. ✅

**Current focus:** `firmware/`'s wheel-reading pieces (`BurstGrouper`,
`WheelPulseDecoder`, `Wheel`) per
`docs/superpowers/plans/2026-10-03-firmware-wheel.md`. `core/Clicker` hasn't been
started yet (plan: `docs/superpowers/plans/2026-09-10-core-clicker.md`) — Colin's,
independent of the wheel work, unblocked whenever he's available.

## Workflow

- Superpowers brainstorming → design doc → writing-plans → implementation. No OpenSpec /
  Comet (too heavy for this project).
- Testing is required and must be real — host-run C++ unit tests for `core/`, manual
  hardware checklist for `firmware/`. Unit / integration / E2E split as appropriate.
- Smallest change that solves the problem. No speculative features.
- Match local style. Flag design smells separately; don't delete unrelated dead code.

## Git

- Feature branches; standing permission to commit / push / open PRs on them.
- No `--force`, `reset --hard`, or branch deletion without explicit approval.
- No AI-attribution lines in commits, PRs, or docs.
