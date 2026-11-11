// Pony ENV firmware (B3 hardware)
//
// Implementation plan and bench tests: docs/FIRMWARE_PLAN.md
// Hardware map: utils.hpp
//
// Step 1: envelope engine skeleton, linear AD (100 ms / 500 ms), 0..10 V on
//         ENV and ENV_ATTE.
// Step 2: TRIGG jack (interrupt) and button fire the envelope; EOC 4 ms pulse.
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

// TRIGG jack is inverted (Q7): jack rising edge = GPIO falling edge
volatile uint32_t trigCount = 0;
void onTrigIn()
{
  env.trigger();
  trigCount++;
}

void setup() {
  Serial.begin(115200);
  setupControls();
  AudioMemory(200);
  env.setAttack(0.100f);
  env.setDecay(0.500f);
  env.setMaxVolts(ENV_MAX_VOLTS);
  attachInterrupt(digitalPinToInterrupt(PIN_TRIG_IN), onTrigIn, FALLING);
  output.begin(PIN_I2S_BCK, PIN_I2S_LRCK, PIN_I2S_DATA);
  Serial.println("=== Pony ENV === step 2: trigger in + EOC");
}

void loop() {
  readControls();

  if (trigButtonPushed()) env.trigger();

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
    Serial.printf("%-6s env %.3f (%.2f V)  trigs %lu  TRIG %s  ATK %4d  DEC %4d  LVL %4d\n",
                  stageNames[env.stage()], env.level(), env.level() * ENV_MAX_VOLTS,
                  (unsigned long)trigCount, trigInHigh() ? "HI" : "lo",
                  attackCV, decayCV, levelCV);
  }
}
