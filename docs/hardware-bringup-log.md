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
