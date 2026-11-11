// Pony ENV firmware (B3 hardware)
//
// Implementation plan and bench tests: docs/FIRMWARE_PLAN.md
// Hardware map: utils.hpp
//
// Step 1: envelope engine skeleton, linear AD, 0..10 V on ENV and ENV_ATTE.
// Step 2: TRIGG jack (interrupt) and button fire the envelope; EOC 4 ms pulse.
// Step 3: ATTACK / DECAY pots set the times (exponential curve), RANGE switch
//         picks the time range.
#include "utils.hpp"
#include "pony_env.hpp"

// ============== Audio ==============
PonyEnv                  env;
AudioOutputI2S           output;
AudioConnection          patchCord1(env, 0, output, DAC_CH_ENV);
AudioConnection          patchCord2(env, 1, output, DAC_CH_ENV_ATTE);
// ===================================

#define ENV_MAX_VOLTS 10.0f
// LVL ADC span after R1/R28 -> 110k (used again from step 6)
#define LVL_ADC_MIN 43
#define LVL_ADC_MAX 3700

// ATTACK / DECAY ADC span. Estimated from the bench readings (max ~4040);
// re-measure the min/max at the ends of the slider travel and update.
#define AD_ADC_MIN 43
#define AD_ADC_MAX 4030

// ---- Time ranges (D5): pot min -> max, in seconds ----
struct TimeRange { float minS; float maxS; const char *name; };
static const TimeRange RANGES[4] = {
  {0.001f, 0.25f, "FAST"},
  {0.005f, 2.0f,  "MED"},
  {0.020f, 10.0f, "SLOW"},
  {0.100f, 60.0f, "EXT"},
};

// pot position 0..1 -> time, exponential so equal travel = equal ratio
float potToSeconds(float norm, const TimeRange &r)
{
  return r.minS * powf(r.maxS / r.minS, norm);
}

float adcNorm(float raw, float lo, float hi)
{
  return constrain((raw - lo) / (hi - lo), 0.0f, 1.0f);
}

// ---- Control state ----
#define CONTROL_RATE_US 1000   // controls processed at 1 kHz
#define POT_SMOOTH 0.05f       // one-pole coefficient at 1 kHz (~20 ms)
float atkSmooth = 0.0f, decSmooth = 0.0f;
int8_t rangeSel = 1;           // last valid RANGE position (default MED)
float attackS = 0.1f, decayS = 0.5f;

// TRIGG jack is inverted (Q7): jack rising edge = GPIO falling edge
volatile uint32_t trigCount = 0;
void onTrigIn()
{
  env.trigger();
  trigCount++;
}

void updateTimes()
{
  if (rangeSwitch >= 0) rangeSel = rangeSwitch; // between positions -> keep last
  const TimeRange &r = RANGES[rangeSel];
  // sliders read high at the short end, so invert: top of travel = longest time
  attackS = potToSeconds(1.0f - adcNorm(atkSmooth, AD_ADC_MIN, AD_ADC_MAX), r);
  decayS  = potToSeconds(1.0f - adcNorm(decSmooth, AD_ADC_MIN, AD_ADC_MAX), r);
  env.setAttack(attackS);
  env.setDecay(decayS);
}

void setup() {
  Serial.begin(115200);
  setupControls();
  AudioMemory(200);
  readControls();
  atkSmooth = attackCV;   // start the filters at the current pot position
  decSmooth = decayCV;
  updateTimes();
  env.setMaxVolts(ENV_MAX_VOLTS);
  attachInterrupt(digitalPinToInterrupt(PIN_TRIG_IN), onTrigIn, FALLING);
  output.begin(PIN_I2S_BCK, PIN_I2S_LRCK, PIN_I2S_DATA);
  Serial.println("=== Pony ENV === step 3: times + range");
}

void loop() {
  readGates();
  if (trigButtonPushed()) env.trigger();

  static uint32_t lastControl = 0;
  if (micros() - lastControl >= CONTROL_RATE_US)
  {
    lastControl = micros();
    readCVs();
    readSwitches();
    atkSmooth += POT_SMOOTH * (attackCV - atkSmooth);
    decSmooth += POT_SMOOTH * (decayCV - decSmooth);
    updateTimes();
  }

  // LED: green while the envelope runs, blue flash on EOC
  ledsColors[0].red = 0;
  ledsColors[0].green = env.stage() != PonyEnv::IDLE ? 255 : 0;
  ledsColors[0].blue = env.eocActive() ? 255 : 0;
  static uint32_t lastLed = 0;
  if (millis() - lastLed >= 16) { lastLed = millis(); updateNeopixel(); }

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 200)
  {
    lastPrint = millis();
    static const char *stageNames[] = {"IDLE", "ATTACK", "DECAY"};
    Serial.printf("%-6s %5.2f V | RANGE %c %-4s | ATK %4d -> %8.1f ms | DEC %4d -> %8.1f ms | trigs %lu\n",
                  stageNames[env.stage()], env.level() * ENV_MAX_VOLTS,
                  switchChar(rangeSwitch), RANGES[rangeSel].name,
                  attackCV, attackS * 1000.0f, decayCV, decayS * 1000.0f,
                  (unsigned long)trigCount);
  }
}
