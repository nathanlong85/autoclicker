# autoclicker — project rules

A Bluetooth auto-clicker built into a gutted mouse shell, for Colin (12) to use with
Geometry Dash on PC / iPhone / Android. Father-son project: Nate + Colin build, Claude
owns the hard parts.

Full hardware/firmware spec: `~/Downloads/PROJECT_HANDOFF.md` (not in repo).
Design doc: `docs/superpowers/specs/` once written.

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
- Runs the Adafruit UF2 bootloader (0.6.0); currently flashed with CircuitPython 10.3.0
  (to be overwritten). UF2 drag-and-drop and CDC serial DFU both verified working.
- Battery: 1050 mAh 3.7 V LiPo, soldered to BAT pads. USB-C charging via onboard PMIC.
- Inputs: left switch, right switch, scroll quadrature encoder — all rewired from the
  original mouse to nRF52840 GPIO. Pin assignments TBD.

## Firmware

- **Language:** Arduino / C++ with the Adafruit nRF52 core + Bluefruit BLE library.
  Board selection: "nice!nano". NOT CircuitPython — fast, solid click timing is the
  point of the project.
- App links at 0x26000 (with S140 SoftDevice) when keeping the bootloader.

### Architecture

- `core/` — pure C++ state machine. No Arduino headers, no hardware. Single entry point
  `tick(inputs, now_ms)` returning outputs (non-blocking scheduler — never `delay()`).
  Compiles and unit-tests on the host (macOS). This is Colin's playground.
- `firmware/` — thin hardware layer: GPIO read + debounce, quadrature decode, BLE HID
  mouse connection. Calls `core::tick()` every loop. No game logic here.
- One small shared interface header is the only contract between the two.

### Behavior (v1)

- **Left button:** plain left-click passthrough to the host.
- **Right button:** hold-to-run dead-man's switch — streams left-clicks at the current
  interval while held, stops on release. Host never receives a real right-click.
  (Pending final confirmation from Colin.)
- **Scroll wheel:** adjusts click interval, ~10 ms per detent, clamped 20–150 ms,
  default 50 ms. No host scroll events. Direction/step size to be tuned by Colin.

### Scope

- **v1:** clicking, plus an RGB LED speed/connection indicator. No cursor movement
  (out of scope entirely).
- **LED (v1):** common 4-leg RGB LED, stand-in for an OLED until Nate/Colin get one.
  Not paired → blue breathing pulse. Paired + idle → off. Paired + autoclicking →
  green (slow) → yellow → red (fast) gradient by current interval. Pure color logic in
  `firmware/LedColorPicker` (tested); `firmware/SpeedLed` wraps 3 `analogWrite()` calls
  around it (untested glue). Pins/polarity decided during step 0/wiring.
- **v2 candidates:** flash persistence of speed, deep sleep / low power, 0.42" I2C OLED
  (72×40, SSD1306, U8g2 library) showing the exact speed value.

### Step 0 (before any core logic)

1. Verify board + bootloader.
2. Flash a blink sketch — proves the toolchain.
3. Flash a bare BLE HID mouse sketch — confirm pairing + manual click on Mac / iPhone /
   Android.

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
