// Fantasia RP template
//
// check in utils.hpp for some built-in goodies
#include "utils.hpp"

// Place the code generated in Audio web here:

// end of generated code

void setup() {
  setupCVs();
  // setupDisplay();

  Serial.println("=== FantasiA ===");
}


void loop() {
  readCVs();
  printCVs();
}

// TODO: get correct pins for segment display encoder and buttons