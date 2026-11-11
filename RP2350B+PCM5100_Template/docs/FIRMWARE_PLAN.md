# Pony ENV — firmware implementation plan (B3 hardware)

Implements the behaviour described in `Pony_ENV_Hardware_Reference.pdf`, on the
B3 board as built: pin map in `src/utils.hpp`, analog rework in `HW_MODS_B3.md`
(ENV 0–10 V, VCA CV scaled in hardware, LVL ADC 0–3.0 V).

Stack: Arduino (earlephilhower core) + pico-audio. The envelope is a custom
`AudioStream` computed per sample at 44.1 kHz. No DaisySP port; the curve idea
from its `AdEnv::SetCurve` is reused.

## Hardware reference doc vs B3 board

Where the reference doc and the board disagree, the firmware follows the board:

| Topic | Reference doc | B3 board |
|---|---|---|
| Pot GPIOs | 45 = Attack, 47 = LVL | 45 = LVL, 46 = Decay, 47 = Attack |
| DAC channels | L → VCA + Env Atte, R → Env Out | L → ENV jack + VCA CV, R → ENV_ATTE |
| Selectors | pull-up, active low | common = 3.3 V, active HIGH (`INPUT_PULLDOWN`) |
| Trigger In | not specified | inverted (LOW = trigger high), ext. pull-up R65 |
| EOC | GPIO HIGH → gate high | inverted (GPIO LOW → jack high ~+11 V) |
| CV summing | CV adds to pot | inverting summer: positive CV subtracts (see D2) |

## Decisions

Defaults in use until decided otherwise:

| # | Question | Default |
|---|---|---|
| D1 | Which output LVL scales, which one drives the VCA (VCA is hardwired to L / ENV jack) | L (ENV + VCA) = envelope × LVL; R (ENV_ATTE) = full envelope. One line in firmware to swap. |
| D2 | CV polarity: positive CV currently lowers the reading | Fix in hardware with IC4B as a −1 inverter before R28 (and the A/D equivalents). Not fixable in firmware: with the pot at minimum, CV can't push the reading lower. |
| D3 | Retrigger while running | Restart attack from the current level (no click) |
| D4 | Ping meaning | Each trigger toggles between rising (attack time) and falling (decay time) |
| D5 | Time ranges, A/D pot min → max | Fast 1 ms–250 ms · Medium 5 ms–2 s · Slow 20 ms–10 s · Extended 100 ms–60 s |
| D6 | EOC | 4 ms pulse (hardware alarm) at end of decay (one-shot) / end of each cycle (loop) |
| D7 | TAP function | Manual trigger only for now |

D1 and D4 are needed before steps 5–6.

## Steps

Each step is its own build/flash, committed once its test passes.

### 1. Envelope engine skeleton
- `PonyEnv` AudioStream class: computes the envelope per sample in `update()` and
  writes both DAC channels. Replaces the `AudioDcOut` objects. Linear AD with
  fixed 100 ms / 500 ms times.
- Remove `delay(10)` from `loop()`; the loop only reads controls and sets parameters.

**Test**
- [ ] Button press → scope shows 0 → 10 V → 0 at ENV, ~100 ms rise / ~500 ms fall
- [ ] Peak 10.0 V ±0.1 V on the meter (checks `OUT_VOLTS_PER_FS`)
- [ ] Output sits at 0 V when idle and at boot

### 2. Trigger input + EOC
- GPIO12 `attachInterrupt` on FALLING (inverted input) sets a flag read by the
  engine at the next block. Button uses the same path.
- EOC on GPIO18 (inverted), 4 ms pulse: set at end of decay by the engine, cleared by a hardware alarm.

**Test**
- [ ] 1 Hz clock into TRIGG, scope CH1 = trigger, CH2 = ENV: envelope starts ≤ ~3 ms after the edge (one audio block)
- [ ] No missed triggers up to 100 Hz (count over 10 s)
- [ ] EOC goes high for 4 ms at end of decay, ~+11 V (needs a clock slower than ~1.5 Hz with the default 100/500 ms times)

### 3. Time controls + ranges
- ATTACK / DECAY → time on an exponential curve, so the pots feel even across
  their travel. RANGE picks min/max (D5).
- One-pole smoothing on the ADC readings.
- Switch between positions (−1) → keep the last valid position.

**Test**
- [ ] Scope cursors at pot min / middle / max × 4 ranges, filled in below, within ±10% of D5
- [ ] Long envelope with the pots still: no drift or wobble
- [ ] Moving the RANGE switch slowly: no glitches

| Range | A min | A mid | A max | D min | D mid | D max |
|---|---|---|---|---|---|---|
| Fast | | | | | | |
| Medium | | | | | | |
| Slow | | | | | | |
| Extended | | | | | | |

### 4. Shapes
- Lin / Exp / Log / Exp-Log via a curve function on the 0–1 phase of each segment.

**Test**
- [ ] Scope capture for each SHAPE position (ATK ≈ DEC ≈ 500 ms), compared with the reference doc
- [ ] Changing SHAPE mid-envelope: no step or jump in the output

### 5. Modes
- One-shot (AD), ASR (rise to the top, sustain while the gate is high, release on
  gate off), Loop, Ping (D4). Retrigger rule from D3.

**Test**
- [ ] One-shot: a 1 ms trigger gives a full A+D
- [ ] ASR: a 2 s gate sustains at 10 V for 2 s, then releases on the decay time; a gate shorter than the attack releases from wherever the envelope has reached
- [ ] Loop: cycles continuously, EOC pulses every cycle
- [ ] Ping: alternates rise and fall on each trigger
- [ ] Retrigger halfway through decay: smooth curve, no drop to 0

### 6. Level, outputs, VCA
- LVL scales the output chosen in D1, using the measured ADC span. Smooth the
  gain changes (no zipper noise).

**Test**
- [ ] Sweep LVL: ENV peak goes 0 → 10 V, ENV_ATTE stays at 10 V (or the opposite, per D1)
- [ ] 1 kHz sine into Audio In L/R, looping envelope: the VCA output follows, closes completely at 0 V and opens fully at 10 V
- [ ] No clicks at segment boundaries

### 7. CV inputs
- Apply D2 in hardware, measure the ADC response to CV, re-calibrate each
  channel's min/max.

**Test**
- [ ] Bench supply 0 / 2.5 / 5 / 10 V into each CV jack with the pot at minimum: serial readings change as expected, times/level follow
- [ ] LFO into LVL CV: modulation is fast enough (C6 now 2.2 nF)

### 8. NeoPixel
- Brightness follows the envelope, one colour per mode. Update at ~60 Hz from
  `loop()`, never from the audio callback.

**Test**
- [ ] LED visibly follows a slow envelope, off when idle
- [ ] Repeat the step 2 100 Hz trigger test with the LED running: no missed triggers, no audio glitches

### 9. Integration / soak
- Remove the debug prints, or put them behind a `#define`.

**Test**
- [ ] Loop mode with LFO-modulated CVs for 1 hour: no crashes, stuck EOC, drift or USB serial dropouts
- [ ] Power-up: output 0 V, no EOC pulse
