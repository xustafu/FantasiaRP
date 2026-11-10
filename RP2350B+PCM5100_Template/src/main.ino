// Fantasia RP template
//
// check in utils.hpp for some built-in goodies
#include "utils.hpp"

// Place the code generated in Audio web here:
// ============== Audio ==============


AudioSynthWaveform       waveform1;      //xy=240.20001220703125,215.1999969482422
AudioSynthWaveform       waveform2;
AudioOutputI2S           output;           //xy=782.1999969482422,218.5999984741211
AudioConnection          patchCord1(waveform1, 0, output, 0);
AudioConnection          patchCord2(waveform2, 0, output, 1);
// GUItool: end automatically generated code


// end of generated code

void setup() {
  Serial.begin(115200);
  setupControls();
  AudioMemory(200); // copy paste del ejemplo
  // InitiaLlize waveform
  // Sine 0..2V at the jacks: 1V centre, 1V peak (inversion handled by jackVoltsToDac)
  waveform1.begin(WAVEFORM_SINE);
  waveform1.amplitude(fabsf(jackVoltsToDac(1.0f)));
  waveform1.frequency(440);
  waveform1.offset(jackVoltsToDac(1.0f));
  waveform2.begin(WAVEFORM_SINE);
  waveform2.amplitude(fabsf(jackVoltsToDac(1.0f)));
  waveform2.frequency(440);
  waveform2.offset(jackVoltsToDac(1.0f));
  //Start Codec Output
  output.begin(PIN_I2S_BCK, PIN_I2S_LRCK, PIN_I2S_DATA);
  Serial.println("=== FantasiA ===");
}


void loop() {
  readControls();
  ledsColors[0].green = trigButtonPressed() ? 255 : 0;
  updateNeopixel();
  printControls();
  delay(100);
}