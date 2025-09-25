// Fantasia RP template
//
// check in utils.hpp for some built-in goodies
#include "utils.hpp"

// Place the code generated in Audio web here:

// end of generated code

void setup() {

  setupAudio();

  setupCVs();
  setupDisplay();
  setupEncoder();
  setupButtons();
  setupGates();


  Serial.println("=== FantasiA ===");
}


void loop() {

  readCVs();
  //printCVs();

  readEncoder();
  printEncoder();

  readButtons();
  // printButtons();

  readGateIn();
  // printGateIn();

  mixer1.gain(0, (float)cv1 / 4095.0f); // Input gain
  mixer1.gain(1, (float)cv2 / 4095.0f); // Sine wave gain
  mixer2.gain(0, (float)cv3 / 4095.0f); // Dry signal
  mixer2.gain(1, (float)cv4 / 4095.0f); // Wet signal
}