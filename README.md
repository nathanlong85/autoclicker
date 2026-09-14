# autoclicker

A Bluetooth auto-clicker built into a gutted mouse shell, for Colin (12) to use with
Geometry Dash on PC, iPhone, and Android. A father-son project: Colin owns the clicker
logic, Nate owns soldering/assembly, Claude owns the BLE/HID stack, flashing,
debugging, and the hardware-facing firmware.

## How it works

- **Left button** — a plain left-click passthrough, exactly like a normal mouse button.
- **Right button** — hold it down and the mouse streams left-clicks at an adjustable
  speed; let go and it stops immediately.
- **Scroll wheel** — adjusts click speed. Rolling up slows it down, rolling down
  speeds it up, in small steps.
- **RGB LED** — shows status at a glance: blue breathing pulse while waiting to pair,
  off once paired and idle, and a green→yellow→red gradient by current speed while
  autoclicking. A stand-in for a small OLED screen planned for later.

## Hardware

- **Board:** nice!nano v2 (Nordic nRF52840, Cortex-M4F @ 64 MHz, 1 MB flash / 256 KB
  RAM), Adafruit UF2 bootloader.
- **Battery:** 1050 mAh 3.7 V LiPo, salvaged, soldered to the board's BAT pads.
  Charges over USB-C via the board's onboard PMIC.
- **Enclosure:** a gutted real mouse — original button switches, scroll encoder, and
  shell are reused; the original mouse PCB is removed and the nice!nano + battery
  mounted inside instead.
- **Toolchain:** Arduino IDE, Adafruit nRF52 board core, Adafruit Bluefruit BLE HID
  library.

## Status

Design and implementation plans are written and approved; hardware bring-up and
firmware implementation haven't started yet.

- [x] Brainstorm scope and behavior
- [x] Write design doc
- [x] Write implementation plans (hardware bring-up, `core/Clicker`)
- [ ] Step 0: verify board, toolchain, and BLE HID pairing on real hardware
- [ ] `core/Clicker`: the autoclick state machine, unit-tested (Colin)
- [ ] `firmware/`: buttons, wheel, BLE mouse, LED — wired together on the board
- [ ] Wiring: desolder original mouse switches/encoder, wire to the nice!nano
- [ ] Battery soldered in, everything fits in the mouse shell
- [ ] Live test with Colin playing Geometry Dash

**v2 ideas (not started):** click-speed persistence across power cycles, a low-power
sleep mode, and a small OLED screen showing the exact speed once we get one.

## Repo layout (as it will look once code exists)

```
core/       — the Clicker class: pure C++, no hardware, no Arduino. Colin's code,
              unit-tested on a Mac with no board attached.
firmware/   — the hardware layer: buttons, scroll wheel, BLE HID mouse, RGB LED.
              Thin GPIO/BLE glue plus small pure/tested logic pieces.
test/       — doctest-based unit tests for core/ and firmware/'s pure pieces.
docs/superpowers/
  specs/    — the design doc.
  plans/    — step-by-step implementation plans.
```

## Building and testing

Once `core/` exists:

```bash
make test
```

Compiles and runs the unit tests natively on macOS — no Arduino toolchain or board
required. Firmware itself is built and flashed the normal way, through the Arduino
IDE.

## Docs

- [Design doc](docs/superpowers/specs/2026-09-10-ble-autoclicker-design.md)
- [Step 0: hardware bring-up plan](docs/superpowers/plans/2026-09-10-step0-hardware-bringup.md)
- [core/Clicker implementation plan](docs/superpowers/plans/2026-09-10-core-clicker.md)
- [Project rules](CLAUDE.md)
