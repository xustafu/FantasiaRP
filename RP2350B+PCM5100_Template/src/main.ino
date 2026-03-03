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
  setupCVs();
  setupNeopixel();
  setupDisplay();
  setupButtons();
  AudioMemory(200); // copy paste del ejemplo
  // InitiaLlize waveform
  waveform1.begin(WAVEFORM_SQUARE);
  waveform1.amplitude(1.0); 
  waveform1.frequency(440);
  waveform1.offset(1.0);
  waveform2.begin(WAVEFORM_SQUARE);
  waveform2.amplitude(1.0);
  waveform2.frequency(440);
  waveform2.offset(0.1);
  //Start Codec Output
  output.begin(0,1,2);
  Serial.println("=== FantasiA ===");
}


void loop() {
  readCVs();
  printNeoPixel();
  printSevenSegment();
  // printCVs();
  printButtons();
}

// TODO: get correct pins for segment display encoder and buttons