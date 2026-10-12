// Fantasia RP template
//
// check in utils.hpp for some built-in goodies
#include "utils.hpp"

// Place the code generated in Audio web here:

// end of generated code

void setup() {

  // delay(5000);

  setupAudio();

  setupCVs();
  setupDisplay();
  setupEncoder();
  setupButtons();
  setupGates();

  setupLEDs();

  Serial.println("=== FantasiA ===");
}


void loop() {

  readCVs();
  // printCVs();

  readEncoder();
  //printEncoder();

  readButtons();
  // printButtons();

  readGateIn();
  // printGateIn();

  updateAudio(); // also sets the LED colors as a VU meter

  updateLEDs();
}