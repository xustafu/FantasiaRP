// Fantasia RP template
//
// check in utils.hpp for some built-in goodies
#include "utils.hpp"

// Minimal DC source (pico-audio's AudioSynthWaveformDc has no update() implementation)
class AudioDcOut : public AudioStream
{
public:
  AudioDcOut() : AudioStream(0, NULL), level(0) {}
  // n = DAC sample value -1..1
  void amplitude(float n)
  {
    n = constrain(n, -1.0f, 1.0f);
    level = (int16_t)(n * 32767.0f);
  }
  virtual void update(void)
  {
    audio_block_t *block = allocate();
    if (!block) return;
    int16_t v = level;
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) block->data[i] = v;
    transmit(block);
    release(block);
  }
private:
  volatile int16_t level;
};

// Place the code generated in Audio web here:
// ============== Audio ==============


AudioDcOut               dcEnv;      // left  -> ENV jack + VCA CV
AudioDcOut               dcEnvAtte;  // right -> ENV_ATTE jack
AudioOutputI2S           output;
AudioConnection          patchCord1(dcEnv, 0, output, DAC_CH_ENV);
AudioConnection          patchCord2(dcEnvAtte, 0, output, DAC_CH_ENV_ATTE);
// GUItool: end automatically generated code


// end of generated code

// VCA test: ENV (and ENV_ATTE) output a fixed DC level set by the LEVEL pot.
// Pot at max -> VCA fully open. Measured with meter on ENV_C: VCA reaches max
// amplitude at 1.55V. LVL 0..4095 is scaled to 0..1.55V.
// NOTE: the LEVEL pot stage (IC4A, gain -1, 3.3V rail) saturates the ADC at ~1/3
// of the pot travel, so levelCV reaches 4095 well before the pot end.
#define VCA_TEST_MAX_VOLTS 1.55f
// Measured LVL ADC span (IC4A can't reach the rails, and saturates ~half pot turn)
#define LVL_ADC_MIN 43
#define LVL_ADC_MAX 4027

float levelVolts = 0.0f;

void setDcVolts(float volts)
{
  float dac = jackVoltsToDac(volts); // inversion handled here
  dcEnv.amplitude(dac);
  dcEnvAtte.amplitude(dac);
}

void setup() {
  Serial.begin(115200);
  setupControls();
  AudioMemory(200); // copy paste del ejemplo
  setDcVolts(0.0f);
  //Start Codec Output
  output.begin(PIN_I2S_BCK, PIN_I2S_LRCK, PIN_I2S_DATA);
  Serial.println("=== FantasiA === VCA test");
}


void loop() {
  readControls();

  float lvlNorm = (levelCV - LVL_ADC_MIN) / float(LVL_ADC_MAX - LVL_ADC_MIN);
  levelVolts = constrain(lvlNorm, 0.0f, 1.0f) * VCA_TEST_MAX_VOLTS;
  setDcVolts(levelVolts);

  ledsColors[0].green = trigButtonPressed() ? 255 : 0;
  updateNeopixel();

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 200)
  {
    lastPrint = millis();
    float dac = jackVoltsToDac(levelVolts);
    Serial.printf("LVL %4d -> DAC %+.4f FS (PCM5100 %+.3f Vpk) -> ENV %.2f V\n",
                  levelCV, dac, dac * 2.97f, levelVolts);
  }
  delay(10);
}
