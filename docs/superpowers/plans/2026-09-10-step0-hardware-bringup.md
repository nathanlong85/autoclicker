# Step 0: Hardware Bring-Up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Human-in-the-loop note:** every task in this plan requires a person at the
> physical hardware (plugging in USB, pressing reset, pairing a phone). There is
> no automated test runner for these tasks — "pass" means Nate/Colin observed the
> expected physical result and reports it back. Treat each numbered step as a
> live checkpoint with them, not something to run unattended.

**Goal:** Confirm the nice!nano v2 board, toolchain, and BLE HID mouse pairing
all work end-to-end, before any `core/`/`firmware/` code is written.

**Architecture:** No application architecture yet — this plan produces a running
Arduino toolchain and a throwaway "hello, BLE mouse" sketch used only to prove
the pipeline works. Nothing here ships in the final firmware.

**Tech Stack:** Arduino IDE, Adafruit nRF52 board core, Adafruit Bluefruit BLE
library, nice!nano v2 (nRF52840).

**Spec:** `docs/superpowers/specs/2026-09-10-ble-autoclicker-design.md`

## Global Constraints

- Board selection in Arduino IDE: "nice!nano" (per prior verified diagnostics —
  confirm the exact board list entry in Task 2; if it's absent, fall back to
  "Adafruit Feather nRF52840 Express," which is pin/feature-compatible).
- Bootloader: Adafruit UF2 bootloader, already present and verified working
  (drag-and-drop UF2 flashing and CDC serial DFU both confirmed in prior
  diagnostics). Do not attempt to reflash or erase the bootloader.
- Board currently runs CircuitPython — flashing any Arduino sketch overwrites
  it. This is expected and fine; CircuitPython isn't needed for this project.
- Colin participates in every task below (per project rules) — assign him
  concrete parts: pressing reset, running the pairing step on a phone,
  reading Serial Monitor output, checking things off.

---

### Task 1: Identify the board and its bootloader state, log findings

**Files:**
- Create: `docs/hardware-bringup-log.md`

**Interfaces:** None — no code yet.

- [ ] **Step 1: Plug in the board via USB-C and check what macOS sees**

Run (Nate, via `!`):
```bash
system_profiler SPUSBDataType | grep -B 5 -A 10 -i "nordic\|nice\|nrf"
```
Expected: an entry showing a Nordic Semiconductor USB device, OR nothing
matches (which likely means it's in bootloader/CDC mode, not a "device" USB
class — that's fine, check step 2 next).

- [ ] **Step 2: Check for the UF2 bootloader drive**

Run:
```bash
ls /Volumes/
```
Expected: a volume named something like `NICENANOBOOT` if the board is
currently sitting in bootloader mode. If it's not there, double-tap the reset
button on the board (Colin does this) and re-run — this is how you force it
into bootloader mode.

- [ ] **Step 3: Record the findings**

Create `docs/hardware-bringup-log.md`:

```markdown
# Hardware Bring-Up Log

## Task 1: Board identification — <date>

- USB identification: <paste relevant system_profiler output or "not detected
  as USB device; found via UF2 drive instead">
- UF2 bootloader drive present: yes/no, name: <e.g. NICENANOBOOT>
- Bootloader version (if shown in INFO_UF2.TXT on the drive): <value>
- Conclusion: board confirmed as nice!nano v2 in UF2 bootloader mode, ready
  for Task 2.
```

Fill in the actual values observed, not placeholders.

- [ ] **Step 4: Commit**

```bash
git add docs/hardware-bringup-log.md
git commit -m "docs: log step 0 task 1 — board identification"
```

---

### Task 2: Install the Arduino toolchain and select the board

**Files:**
- Modify: `docs/hardware-bringup-log.md` (append)

**Interfaces:** None — no code yet.

- [ ] **Step 1: Install Arduino IDE**

If not already installed: `! brew install --cask arduino-ide` (or download
from arduino.cc if Homebrew cask is unavailable). Confirm it launches.

- [ ] **Step 2: Add the Adafruit board index**

In Arduino IDE: Preferences → "Additional Boards Manager URLs" → add:
```
https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
```

- [ ] **Step 3: Install the Adafruit nRF52 board package**

Tools → Board → Boards Manager → search "Adafruit nRF52" → Install.

- [ ] **Step 4: Select the board**

Tools → Board → look for "nice!nano" under the Adafruit nRF52 boards list.

- If present: select it. This confirms the note in Global Constraints was
  correct.
- If absent: select "Adafruit Feather nRF52840 Express" instead (nice!nano is
  pin/feature-compatible) and note this substitution in the log.

- [ ] **Step 5: Install the Bluefruit library**

Sketch → Include Library → Manage Libraries → search "Adafruit Bluefruit
nRF52 Libraries" → Install (this usually comes bundled with the board
package — confirm it's present rather than reinstalling if so).

- [ ] **Step 6: Record findings and commit**

Append to `docs/hardware-bringup-log.md`:
```markdown
## Task 2: Toolchain install — <date>

- Arduino IDE version: <value, from Arduino IDE → About>
- Adafruit nRF52 board package version: <value>
- Board selected: <"nice!nano" or "Adafruit Feather nRF52840 Express (used as
  nice!nano substitute)">
- Bluefruit library version: <value>
```

```bash
git add docs/hardware-bringup-log.md
git commit -m "docs: log step 0 task 2 — toolchain install"
```

---

### Task 3: Flash Blink and confirm the toolchain round-trips

**Files:**
- Modify: `docs/hardware-bringup-log.md` (append)

**Interfaces:** None — no application code yet.

- [ ] **Step 1: Open the Blink example**

File → Examples → 01.Basics → Blink.

- [ ] **Step 2: Select the correct port**

Tools → Port → select the nice!nano's serial port (macOS shows it as
`/dev/cu.usbmodem*`). If no port shows, double-tap reset to get back into
bootloader mode and retry.

- [ ] **Step 3: Upload**

Click Upload. Expected: compiles without error, uploads without error, and
the message ends with something like "Upload complete."

- [ ] **Step 4: Confirm the physical result (Colin checks this)**

Expected: the onboard LED blinks on/off in a steady ~1-second rhythm. Colin
confirms and reports back.

- [ ] **Step 5: Record findings and commit**

```markdown
## Task 3: Blink — <date>

- Upload succeeded: yes/no
- LED blinking observed: yes/no (confirmed by Colin)
- Any errors encountered and how resolved: <text, or "none">
```

```bash
git add docs/hardware-bringup-log.md
git commit -m "docs: log step 0 task 3 — blink flash confirmed"
```

---

### Task 4: Flash a bare BLE HID mouse sketch and confirm cross-platform pairing

**Files:**
- Create: `docs/superpowers/scratch/bare_ble_mouse/bare_ble_mouse.ino` (a
  throwaway sketch — not part of the final firmware; lives outside `core/`
  and `firmware/` so it's obviously not app code)
- Modify: `docs/hardware-bringup-log.md` (append)

**Interfaces:** None — this sketch is deliberately disposable and shares no
code with `core/`/`firmware/`.

- [ ] **Step 1: Write the bare BLE HID mouse sketch**

```cpp
// bare_ble_mouse.ino
// Throwaway sketch for step 0 bring-up only. Confirms Bluefruit BLE HID
// mouse advertising, pairing, and a manual click work on real hardware and
// real host platforms, before any core/firmware/ code exists.

#include <bluefruit.h>

BLEDis bledis;
BLEHidAdafruit blehid;

const int BUTTON_PIN = 7;  // any convenient GPIO with an onboard/jumper button;
                            // exact pin doesn't matter for this throwaway test

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("Autoclicker Bringup Test");

  bledis.setManufacturer("Bringup Test");
  bledis.setModel("Step 0");
  bledis.begin();

  blehid.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_MOUSE);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop() {
  static bool wasDown = false;
  bool isDown = (digitalRead(BUTTON_PIN) == LOW);

  if (isDown && !wasDown) {
    blehid.mouseButtonPress(MOUSE_BUTTON_LEFT);
  } else if (!isDown && wasDown) {
    blehid.mouseButtonRelease();
  }
  wasDown = isDown;
}
```

- [ ] **Step 2: Upload it**

Same as Task 3: select port, Upload. Expected: compiles, uploads without
error.

- [ ] **Step 3: Pair with a Mac**

System Settings → Bluetooth → find "Autoclicker Bringup Test" → Connect.
Expected: pairs successfully, appears as a mouse. Press the button wired to
`BUTTON_PIN` (or briefly short it to ground with a jumper wire if nothing's
wired yet) — expected: a left click registers on the Mac (e.g. click-drag a
window, or click into a text field and see the cursor respond).

- [ ] **Step 4: Pair with an iPhone**

Settings → Bluetooth → find and connect. iOS treats a paired BLE HID mouse as
a pointer device — expected: connects, and pressing the button produces a
click (visible as the on-screen pointer/cursor if iOS shows one, or by its
effect, e.g. tapping a button in an app).

- [ ] **Step 5: Pair with an Android device**

Settings → Bluetooth → find and connect. Expected: connects, click registers
the same way.

- [ ] **Step 6: Record findings and commit**

```markdown
## Task 4: Bare BLE HID mouse — <date>

- Sketch uploaded successfully: yes/no
- Mac pairing: pass/fail, notes: <text>
- iPhone pairing: pass/fail, notes: <text>
- Android pairing: pass/fail, notes: <text>
- Click registered on each platform: yes/no per platform
- Conclusion: BLE HID mouse pipeline confirmed working end-to-end. Ready to
  begin core/ and firmware/ implementation.
```

```bash
git add docs/superpowers/scratch/bare_ble_mouse docs/hardware-bringup-log.md
git commit -m "docs: log step 0 task 4 — BLE HID mouse pairing confirmed on Mac/iPhone/Android"
```

**If any platform fails to pair or click:** stop and load
`superpowers:systematic-debugging` before touching the sketch again — don't
guess at fixes.
