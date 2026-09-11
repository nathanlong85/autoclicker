# Hardware Bring-Up Log

## Task 1: Board identification — 2026-09-11

- USB identification: not detected as a Nordic USB device via
  `system_profiler SPUSBDataType` (board was enumerated as a CircuitPython
  CDC/MSC device instead, not a distinct "Nordic Semiconductor" USB entry).
- UF2 bootloader drive present: yes, name: `NICENANO` (board has no physical
  reset button — the nice!nano v2 doesn't ship with one — so bootloader mode
  was entered via the CircuitPython REPL over serial:
  `microcontroller.on_next_reset(microcontroller.RunMode.BOOTLOADER)` followed
  by `microcontroller.reset()`, instead of a double-tap reset).
- Bootloader version (from `INFO_UF2.TXT` on the drive):
  ```
  UF2 Bootloader 0.6.0 lib/nrfx (v2.0.0) lib/tinyusb (0.10.1-41-gdf0cda2d) lib/uf2 (remotes/origin/configupdate-9-gadbb8c7)
  Model: nice!nano
  Board-ID: nRF52840-nicenano
  SoftDevice: S140 version 6.1.1
  Date: Jun 19 2021
  ```
- Conclusion: board confirmed as nice!nano v2 (Board-ID `nRF52840-nicenano`)
  in UF2 bootloader mode, ready for Task 2.

## Task 2: Toolchain install — 2026-09-11

- Used `arduino-cli` (v1.5.1, Homebrew) instead of the Arduino IDE GUI —
  already installed, faster for this bring-up.
- Adafruit board index added via
  `arduino-cli config add board_manager.additional_urls
  https://adafruit.github.io/arduino-board-index/package_adafruit_index.json`.
- Adafruit nRF52 board package version: 1.7.0 (`adafruit:nrf52@1.7.0`),
  installed via `arduino-cli core install adafruit:nrf52`.
- Board selected: **substitution used** — "nice!nano" is not present in
  `arduino-cli board listall`'s Adafruit entries (this core doesn't ship a
  nice!nano-specific board definition). Using
  **"Adafruit Feather nRF52840 Express"** (FQBN `adafruit:nrf52:feather52840`)
  instead, per the plan's fallback — pin/feature-compatible, same nRF52840
  chip and Adafruit UF2 bootloader.
- Bluefruit library: present, bundled with the core install at
  `~/Library/Arduino15/packages/adafruit/hardware/nrf52/1.7.0/libraries/Bluefruit52Lib`
  (not a separate Library Manager install).

## Task 3: Blink — 2026-09-11

- Used `arduino-cli compile --fqbn adafruit:nrf52:feather52840 --upload`
  instead of the Arduino IDE GUI (consistent with using `arduino-cli` for
  Task 2). Sketch written ad hoc (standard `LED_BUILTIN` blink) rather than
  copied from IDE examples menu, since we're not using the IDE.
- Upload succeeded: yes — "Device programmed." Sketch uses 21244 bytes (2%)
  of program storage, 3096 bytes (1%) of dynamic memory.
- LED blinking observed: yes (confirmed by Colin), steady ~1-second on/off.
- Any errors encountered and how resolved: none.

## Task 4: Bare BLE HID mouse — 2026-09-11

- Sketch uploaded successfully: yes, via `arduino-cli compile --fqbn
  adafruit:nrf52:feather52840 --upload` (127940 bytes / 15% program storage,
  15656 bytes / 6% dynamic memory).
- **No physical reset button, and no bootloader-entry trick available once a
  non-CircuitPython sketch is running** (the CircuitPython REPL trick from
  Task 1 only works while CircuitPython is installed; it was overwritten by
  Blink in Task 3). Re-entering bootloader mode between flashes now requires
  briefly double-touching a jumper wire between the board's RST pin and a GND
  pin (mimics a double-tap reset). This will be needed for every future
  reflash until firmware persistence/OTA is set up — worth remembering.
- **Pin numbering gotcha:** the sketch's `BUTTON_PIN = 7` is an *Arduino*
  digital pin number under the `feather52840` variant we're compiling
  against, which is a different physical board than the nice!nano (no
  official nice!nano board definition exists in `adafruit:nrf52@1.7.0`). The
  nice!nano's silkscreen labels pins by raw chip address (`Pport.pin`
  notation, e.g. "107" = P1.07). Per
  `variants/feather_nrf52840_express/variant.cpp`, Arduino D7 = **P1.02**, so
  the physical pin to jumper to GND is the one labeled **"102"** on the
  nice!nano, not "7" or "107". This mismatch will need to be resolved
  properly (either find/adopt a real nice!nano variant, or hand-map every
  pin we use) before wiring the real switches/encoder in `firmware/`.
- Mac pairing: pass. Note: macOS initially showed the device via its
  "keyboard setup assistant" on first connect (a known quirk with generic
  BLE HID peripherals before macOS settles on their reported device type) —
  resolved on its own, Bluetooth menu bar icon then correctly showed a mouse
  glyph.
- iPhone pairing: pass.
- Android pairing: pass.
- Click registered on each platform: yes — Mac, iPhone, Android all
  confirmed via jumpering P1.02 to GND.
- Conclusion: BLE HID mouse pipeline confirmed working end-to-end on all
  three target platforms. Ready to begin `core/` and `firmware/`
  implementation — with two carryover items to resolve first: (1) no true
  nice!nano board definition, so all future pin references need to go
  through the `variant.cpp` D-number → P-port.pin mapping; (2) no physical
  reset button, so the RST/GND jumper trick is the standing procedure for
  re-entering the bootloader.
