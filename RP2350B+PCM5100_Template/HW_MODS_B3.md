# Pony Env B3 — analog scaling rework plan

Bench findings from the VCA test (firmware `src/main.ino`, VCA test mode) and the
part changes proposed to fix them. Values traced from `B3Kicad/Pony_env_B3.net`.

## Bench measurements (B3 unit)

| What | Value |
|---|---|
| DAC → ENV_C gain (measured) | **9.84 V per full scale** (calc. 9.49) — −0.1575 FS → 1.55 V |
| VCA fully open (AS3360 CV, metered at ENV_C) | **~1.55 V** |
| LVL ADC span | **43 … 4027** (IC4A cannot reach the 3.3 V rails) |
| LVL pot | reading saturates at ~**half the turn**, rest of travel dead |
| C6 | fitted **2.2 µF**, should be **2.2 nF** |

## Targets

- ENV jack: **0–10 V**
- VCA CV (AS3360 pins 5/10): **0–~1.6 V** (fully open at ENV = 10 V)
- LVL pot (0…−10 V) and LVL CV (0…10 V) → ADC **0–3.0 V**, full pot travel usable

## 1. DAC output amp — IC14B (ENV), IC14A (ENV_ATTE)

Now: OUTL → R71 2.2k → (C92 1n) → R42 10k → IC14B, R43 39k feedback → gain −3.20 → ~9.8 V max (short of 10 V).

| Ref | Now | New | Note |
|---|---|---|---|
| **R43** (ENV) | 39k | **43k** | gain −3.52 → ~10.8 V/FS, 10 V at 0.92 FS |
| **R56** (ENV_ATTE) | 39k | **43k** | same for the other channel |

- Alternative: R42 / R55 10k → 9.1k (gain −3.45, 10 V at 0.94 FS).
- Keep the stage inverting and unipolar (negative DAC codes). An offset to use the full bipolar range would park the output at +5 V on boot/mute.
- NE5532 on ±12 V swings ~±10.5 V: firmware caps at 10 V. Check output at worst-case −12 V rail.

## 2. VCA CV path — IC13A / IC13B

Now: ENV_C → R5 1k → IC13A (R8 1k, gain −1) → R17 10k → IC13B (R41 10k, gain −1) → AS3360 CV.
VCA sees ENV_C 1:1, and R5 loads ENV_C with 1k (10 mA at 10 V).

| Ref | Now | New | Note |
|---|---|---|---|
| **R5** | 1k | **47k** | input resistor, also unloads ENV_C (~0.2 mA) |
| **R8** | 1k | **7.5k** | gain −0.16 → 10 V gives 1.60 V at the VCA |
| C10, C11 | 2.2 nF | 2.2 nF | **verify fitted value** (C6 was wrong); ~9.6 kHz with R8 7.5k |
| R17, R41 | 10k | 10k | IC13B stays unity |

- Alternative for 2.0 V at the VCA: R5 51k / R8 10k (but VCA then saturates at ~7.8 V of envelope).

## 3. LVL pot / CV summer — IC4A (3.3 V supply)

Now: V_adc = −(R29/R1 · V_pot + R29/R28 · V_cv), R1 = R29 = 33k → gain −1 → clips at ~3.3 V of the 10 V pot.
100k pot loaded by 33k also makes the curve very non-linear.

| Ref | Now | New | Note |
|---|---|---|---|
| **R1** (pot) | 33k | **110k** | gain −0.30 → 10 V → 3.0 V, loading ratio 3.0 → 0.9 |
| **R28** (CV) | 100k | **110k** | CV gain −0.30 |
| R29 | 33k | 33k | keep |
| **C6** | 2.2 µF | **2.2 nF** | 2.2 µF gives a 2.2 Hz low-pass (pot lag, kills LVL CV modulation) |

**Open decision — CV polarity:** the CV jack feeds R28 directly, so positive CV *subtracts* from the pot (and does nothing with the pot at minimum).
For CV that *adds*, insert a −1 inverter before R28 (IC4B is unused).

ATTACK / DECAY sliders feed R34 / R39 (33k) the same way — apply the same change if their readings also max out before the end of travel.

## 4. −10 V reference checks (REG-1 LM4040-10, R11 1k from −12 V, 2 mA budget)

- [ ] Measure −10 V node with all pots at min, then all at max — must stay −10.0 V
- [ ] Confirm SPEED0 value (schematic says 10k, expected 100k). 10k pots would overload the reference.
- [ ] Measure the −12 V rail
- If lower-value pots are used, change R11 1k → 680 Ω (~2.9 mA).

## 5. Other

- **X1 (332-02)** shorts ENV_C to ENV_ATTE_C — leave open unless intended.

## Firmware follow-up after the rework

- [ ] Re-measure output gain (command a known voltage, read ENV_C) and update `OUT_VOLTS_PER_FS` in `src/utils.hpp` — expected ~−10.85 with R43 = 43k
- [ ] Re-measure LVL ADC min/max and update `LVL_ADC_MIN` / `LVL_ADC_MAX` in `src/main.ino`
- [ ] VCA test cap (`VCA_TEST_MAX_VOLTS` 1.55 V) no longer applies: envelope runs 0–10 V, VCA scaling is in hardware

## Lab checklist

- [ ] R43, R56 → 43k
- [ ] R5 → 47k, R8 → 7.5k
- [ ] R1, R28 → 110k
- [ ] C6 → 2.2 nF; check C10, C11 (2.2 nF), C70, C90 (100 pF)
- [ ] −10 V reference checks (section 4)
- [ ] Decide CV polarity (section 3)
- [ ] Firmware recalibration (above)
