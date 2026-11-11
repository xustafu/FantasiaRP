// Pony ENV envelope engine
//
// AudioStream with no inputs and two outputs (0 = L, 1 = R), computed per
// sample inside update() (I2S interrupt, 44.1 kHz, 128-sample blocks).
// Control side (loop / trigger ISR) only sets parameters / requests triggers.
//
// Step 1: linear AD, fixed times.
// Step 2: EOC 4 ms pulse at the end of decay.
// Shapes, modes, ranges come in later steps (docs/FIRMWARE_PLAN.md).
#pragma once
#include "utils.hpp"

#define EOC_PULSE_US 4000 // EOC trigger length, timed by a hardware alarm

// EOC pulse: rising edge from the audio interrupt, falling edge from a timer
// alarm, so the length doesn't depend on the 2.9 ms audio block.
volatile bool eocHigh = false;
volatile alarm_id_t eocAlarm = 0;

int64_t eocOffCallback(alarm_id_t, void *)
{
  setEOC(false);
  eocHigh = false;
  eocAlarm = 0;
  return 0; // don't repeat
}

void fireEOC()
{
  if (eocAlarm > 0) cancel_alarm(eocAlarm); // retrigger extends the pulse
  setEOC(true);
  eocHigh = true;
  eocAlarm = add_alarm_in_us(EOC_PULSE_US, eocOffCallback, nullptr, true);
}

class PonyEnv : public AudioStream
{
public:
  enum Stage : uint8_t { IDLE, ATTACK, DECAY };

  PonyEnv() : AudioStream(0, NULL) {}

  // --- control side (call from loop or an ISR) ---
  void setAttack(float seconds) { attackInc = timeToInc(seconds); }
  void setDecay(float seconds)  { decayInc = timeToInc(seconds); }
  // peak voltage at the output jacks
  void setMaxVolts(float volts) { dacScale = jackVoltsToDac(volts) * 32767.0f; }
  void trigger() { trigPending = true; }

  Stage stage() const { return (Stage)curStage; }
  float level() const { return env; } // 0..1
  bool eocActive() const { return eocHigh; }

  // --- audio side ---
  virtual void update(void)
  {
    audio_block_t *blockL = allocate();
    if (!blockL) return;
    audio_block_t *blockR = allocate();
    if (!blockR) { release(blockL); return; }

    if (trigPending)
    {
      trigPending = false;
      curStage = ATTACK; // retrigger restarts attack from the current level (D3)
    }

    const float aInc = attackInc, dInc = decayInc, scale = dacScale;
    float e = env;
    uint8_t st = curStage;
    bool endOfCycle = false;

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
    {
      if (st == ATTACK)
      {
        e += aInc;
        if (e >= 1.0f) { e = 1.0f; st = DECAY; }
      }
      else if (st == DECAY)
      {
        e -= dInc;
        if (e <= 0.0f)
        {
          e = 0.0f;
          st = IDLE;
          endOfCycle = true;
        }
      }
      int16_t s = (int16_t)(e * scale);
      blockL->data[i] = s;
      blockR->data[i] = s;
    }

    if (endOfCycle) fireEOC();

    env = e;
    curStage = st;

    transmit(blockL, 0);
    transmit(blockR, 1);
    release(blockL);
    release(blockR);
  }

private:
  static float timeToInc(float seconds)
  {
    if (seconds < 0.0001f) seconds = 0.0001f;
    return 1.0f / (seconds * AUDIO_SAMPLE_RATE_EXACT);
  }

  volatile float attackInc = timeToInc(0.1f);
  volatile float decayInc = timeToInc(0.5f);
  volatile float dacScale = 0.0f;   // 0 V until setMaxVolts()
  volatile bool trigPending = false;
  volatile uint8_t curStage = IDLE;
  volatile float env = 0.0f;
};
