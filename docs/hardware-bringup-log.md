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
