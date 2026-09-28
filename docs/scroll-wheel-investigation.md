# Scroll Wheel Sensor Investigation

**Status as of 2026-09-17: unresolved, paused pending a logic analyzer.**

## Context

The donor mouse is a **Logitech M325** (board silkscreen: `210-001578 Rev 001`,
codenamed "Gyro Dagger" per the silkscreen). Goal: determine whether its
scroll wheel can be reused as-is for the autoclicker's speed control, per the
design doc's directional behavior (up = slower, down = faster, confirmed by
Colin — see `docs/superpowers/specs/2026-09-10-ble-autoclicker-design.md`).

This turned into a multi-day investigation because the wheel's direction
sensing does not behave the way any documented Logitech scroll wheel design
is supposed to. **Current bottom line: the sensor appears to expose only one
real data channel, which should be physically impossible for a wheel that
demonstrably scrolls in both directions. This contradiction is not yet
resolved.**

## Hardware identified on the board

- **`LD2`** — 2-leg component, clear/translucent top. IR LED emitter for the
  wheel's optical interrupter. High confidence.
- **`LQ1`** — 3-leg component, black/opaque top, mounted facing `LD2` with a
  gap between them where the wheel's slotted disc passes. The
  optical *receiver*. This is the component at the center of the mystery.
- **`SW3`** — the wheel's click/press switch (a standard 4-pin tact switch,
  same package as `SW5`/`SW6`, the left/right buttons). Repurposed as the
  pairing button per the design doc addendum — unrelated to this
  investigation.
- **`SW10`** (back of board) — identified as the mouse's power switch. Feeds
  the corrected design-doc power-switch approach (GPIO input + nRF52840
  System OFF sleep — see the design doc's 2026-09-14 correction; there is no
  EN pin on the nice!nano).
- **`U1` "Capella003"** — a Capella Microsystems optical IC positioned where
  the mouse's cursor-tracking sensor sat. Unrelated to the scroll wheel
  (confirmed by Nate: it's directly above where the tracking sensor's
  removed IR emitter was). Not investigated further — cursor movement is out
  of scope for this project entirely.
- Tracking sensor's IR emitter (a separate small daughterboard, connected via
  a 2-lead ribbon to connector `J3`) has been **removed** — the only
  component physically removed from the donor board so far. Out of scope,
  not needed.

## `LQ1`'s three pins — what's actually been measured

Referred to throughout as "top," "middle," "bottom" by their physical
position on the package:

| Pin | Multimeter (static) | Confirmed via |
|---|---|---|
| top | 0V, never changes | **Continuity-confirmed hard-wired to board GND** (`TP18`) |
| middle | Constant ~2.2–2.8V | Never varies — not even during active spinning (see tests below) |
| bottom | 0V baseline, responds to blocking the beam | Real signal, correlates with rotation |

## Tests performed, in order, and what each one ruled out

1. **Static multimeter probing + finger-blocking test.** Top inert, middle
   constant, bottom changed when the beam was blocked. Initially read as
   "middle = VCC, bottom = single data channel." *(This is also explainable
   by "one of two real channels happened to be resting in a blocked state,"
   which is why this alone didn't settle anything — see test 4.)*

2. **`wheel_diag.ino` v1 — single channel, polling (`analogRead`+`digitalRead`
   on `bottom` only).** Confirmed real pulses correlated with actual wheel
   spinning. Also found: `bottom`'s "high" state only reaches ~620/1023
   (~2V), which sits marginally below the nRF52840's ~2.3V digital-HIGH
   threshold at 3.3V logic — `digitalRead()` never once registered `1`
   despite `analogRead()` clearly showing the signal. **Implication for
   final firmware:** this signal can't be read with plain `digitalRead()`
   as-is; either read it via `analogRead()` with a custom threshold, or
   repower the LED/pull-ups from the nice!nano's own 3.3V rail with
   purpose-picked resistor values (bypassing the original board's ~2.2V
   regulation) — a real decision still to make once/if this investigation
   resolves.

3. **Separate UP vs. DOWN captures + alternation-pattern analysis.** Tested
   the theory that the disc's slots are asymmetrically spaced (a known
   technique letting a single channel infer direction from pulse-width
   timing). Result: alternation score ~60% for both directions (should be
   ~90%+ and *different* between directions if real) — **theory not
   supported by the data.**

4. **Two-channel polling capture (`bottom` + `middle` simultaneously,
   pins `002`/`031`).** Directly tested whether `middle` pulses during real
   rotation (ruling out the "it was just resting in a blocked state" gap in
   test 1). Result: `middle` stayed in a tight 603–640 band for the *entire*
   capture, including periods where `bottom` was clearly pulsing from real
   spinning. **No correlation with rotation at all.**

5. **Sanity check with the original RF receiver.** With the diagnostic tap
   wire attached to `bottom`, the original mouse's wheel stopped scrolling
   entirely (buttons still worked). Removing the tap wire restored normal
   scrolling. Conclusion: the sensor/disc themselves are healthy — the tap
   wire's mere presence (not damage; removing it fixed everything) was
   enough to disrupt the original circuit's own reading, likely from added
   capacitance or a non-ideal connection. **Practical implication:** any
   future direct probing of these pads should have the nice!nano end fully
   *disconnected*, not just unpowered, and the original RF receiver and any
   nice!nano tap should never be tested at the same time.

6. **Interrupt-based capture (`attachInterrupt(..., CHANGE)` on both
   pins).** The definitive test — interrupts fire on the actual electrical
   edge and cannot miss a transition regardless of duration, unlike polling.
   Result over a ~20-second real spin session: **`bottom_edges` = 74,000+,
   `middle_edges` = 0, exactly, the entire time.** This rules out "it pulses
   too fast for polling to catch" as well.

## External research

- [Super User: How does a Logitech scroll wheel detect movement?](https://superuser.com/questions/1271240/how-does-a-logitech-scroll-wheel-detect-movement) —
  describes the standard design: two sensors, quadrature truth table.
- [EDN: Mouse Encoder Hack](https://www.edn.com/mouse-encoder/) — confirms a
  single sensor cannot determine direction; two offset output lines are
  required.
- A Google AI Mode conversation (2026-09-16, Nate) proposed `LQ1` is a
  dual-element sensor sharing 3 pins (GND + two open-collector data lines,
  externally pulled up) — internally consistent with test 1's data, but
  **directly disproven by test 6** (zero interrupt edges on `middle` during
  real spinning rules out a second data element on that pin).
- A promising-looking Hackaday forum thread with a near-identical title
  ("mouse scroll wheel, only single IR emitter/detector pair?") could not be
  retrieved — the site is dead.
- No source found anywhere describes a legitimate way for a genuinely
  single-channel sensor to convey direction. This is treated as
  near-certain: direction requires two real signals, full stop.

## The unresolved contradiction

- `LQ1` empirically has exactly one usable data pin (`bottom`); `middle` is
  inert under every test we could throw at it, including a test (hardware
  interrupts) that cannot physically miss a real transition.
- Yet the original mouse, wired to its own RF receiver, unambiguously
  scrolls in both directions.
- No second sensor element has been found anywhere on the board or the wheel
  bracket assembly despite repeated close inspection (front, back, and the
  bracket's two springs — confirmed mechanical-only, one for the click-back
  spring, one for the detent feel, neither electrically connected to
  anything).

This is a genuine, unresolved contradiction, not a dead end from lack of
effort — three independent tests (asymmetric-timing analysis, two-channel
polling, and interrupt-based capture) all converge on "single channel," and
none of the available research offers a mechanism that would make that
compatible with the mouse's observed behavior.

## Current plan

Nate is ordering a cheap USB logic analyzer (24MHz/8-channel,
`sigrok`/`fx2lafw` + PulseView — e.g. the SparkFun PID-15033 or a Comidox
clone) to get an actual waveform capture, which sidesteps every sampling-rate
limitation of the microcontroller-based tests above. This tool is also
intended for a separate project (a PS2 controller adapter using a Pico 2W),
so it's a dual-purpose purchase, not a one-off.

**Once the analyzer arrives:** capture `bottom` and `middle` simultaneously
while spinning, and look directly at the waveform shape/timing rather than
inferring from sampled data.

**If the analyzer still doesn't turn up a second channel:** the practical
fallback is a different donor mouse, this time visually checked *before* any
teardown work for a **mechanical contact-based** wheel encoder (two metal
wiper contacts on a segmented ring, no optics involved) — trivially
verifiable with a plain multimeter and free of every ambiguity this
investigation has run into. See the design/brainstorming discussion around
2026-09-15 in project history for the reasoning (mechanical encoders are the
easier, lower-risk choice for any *future* donor mouse, even though this
current M325 turned out to be optical).

## Logic analyzer capture procedure

Written 2026-09-28, before the first capture. Items marked **(unverified)** are
assumptions to confirm on the bench, not known facts.

### Why this capture may look different from what we expect

- **`bottom` may be a strobe, not a wheel signal.** Test 6 logged 74,000+ edges
  in ~20 s (~3,700 edges/s). A hand-spun wheel with a few dozen slots should
  produce a few hundred edges/s at most. One possible explanation: the original
  circuit pulses the LED (`LD2`) to save battery, and `bottom` is showing that
  strobe rather than slot transitions. If so, the "single channel" result could
  be an artifact of us sampling asynchronously to a strobe. **(unverified)**
- **This is why the capture includes `LD2`,** and why the sample rate is high.
- **Threshold risk.** `bottom` only reaches ~2 V high and `middle` sits at
  ~2.2–2.8 V. The analyzer's input threshold may be near that range, same as
  the nRF52840 problem in test 2. If a channel looks stuck or noisy, suspect
  the threshold before suspecting the sensor. **(unverified)**

### Software (Mac)

1. `brew install sigrok-cli`. PulseView is not in Homebrew core; get it from
   sigrok.org if we want the GUI. `sigrok-cli` alone is enough, since it writes
   `.sr` files that we can analyze with a script.
2. The analyzer needs the `fx2lafw` firmware, which it loads on connect. Run
   `sigrok-cli --scan`. If the device shows up as `fx2lafw`, we're done. If not,
   the firmware files are missing and need to be installed from sigrok.org.
   **(unverified: whether Homebrew's libsigrok bundles them)**

### Wiring

Mouse board stays **as it was in test 5's baseline**: original battery, original
RF receiver plugged into the PC, nice!nano fully **disconnected** (not just
unpowered). The analyzer is a passive observer.

| Analyzer channel | Connect to | Why |
|---|---|---|
| GND | `TP18` (board GND, same net as `LQ1` top) | Shared reference. Required. |
| CH0 | `LQ1` bottom | The known-live pin |
| CH1 | `LQ1` middle | The "inert" pin |
| CH2 | `LD2` pin 1 | Checks for LED strobing |
| CH3 | `LD2` pin 2 | Same, other leg |

- Use the analyzer's mini-grabbers or a fine-tip probe. Don't solder new tap
  wires; test 5 showed added wire/capacitance can break the original circuit.
- Don't power the mouse from anything but its own battery while the analyzer's
  USB ground is attached.
- **Sanity check first:** with everything attached, does the mouse still scroll
  on the PC? If not, that's test 5 again. It's a useful finding on its own, so
  record it and stop; don't force it.

### Captures

Sample rate 4 MHz for the first pass. That's 1000x the observed edge rate and
well under the analyzer's 24 MHz ceiling. Raise it if the first capture shows
edges only one sample wide.

Save each as `scratch/wheel_diag/captures/<name>.sr`. Don't commit them. Add
the directory to `.gitignore`, like the `wheel_log*.csv` files.

| # | Name | Action | What we're looking for |
|---|---|---|---|
| 1 | `idle` | Wheel untouched, 5 s | Is anything toggling with no input? A steady strobe on `LD2`/`bottom` confirms the strobe theory. |
| 2 | `spin_up` | Steady slow spin one way, ~5 s | Edge rate and shape on each channel |
| 3 | `spin_down` | Same, other way | Does anything differ from `spin_up`? Timing, phase, which channel moves? |
| 4 | `one_notch` | A single slow detent click, one direction | The cleanest look at what one step does |
| 5 | `blocked` | Hold a piece of card in the beam gap, 3 s | Confirms `bottom` is the receiver output and shows the blocked-state levels |

Command shape **(unverified until we see the device's real name)**:
`sigrok-cli -d fx2lafw --config samplerate=4m -C D0,D1,D2,D3 --time 5s -o scratch/wheel_diag/captures/idle.sr`

### Reading the results

- **If `LD2` is strobing:** the receiver output only means something when
  sampled in sync with the strobe. Re-analyze `bottom`/`middle` gated by `LD2`.
  `middle` may turn out to be live once we do that.
- **If `middle` toggles in any capture:** tests 4 and 6 were wrong somehow.
  Compare its phase against `bottom` between `spin_up` and `spin_down`.
  That's the quadrature we've been missing.
- **If `middle` is dead in every capture and `LD2` is steady:** the
  contradiction is real and the fallback applies (mechanical-encoder donor).
- **If channels look stuck at one level:** rule out the threshold problem
  first. Compare against the multimeter levels from the table above.

## Related scratch files

- `scratch/wheel_diag/wheel_diag.ino` — the current (interrupt-based)
  diagnostic sketch. Throwaway, not part of `core/`/`firmware/`. Has gone
  through three iterations (single-channel polling → two-channel polling →
  two-channel interrupts); only the latest is kept in the working tree, but
  see git history / this doc for what the earlier versions tested.
- `wheel_log*.csv` — raw capture output from the sketch above.
  Gitignored (large, regenerable, not source) — not committed.
