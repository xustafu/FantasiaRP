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

  updateAudio();

  ledsColors[3].blue = cv2 / 16; //red to magenta as the sine gets louder
  ledsColors[2].blue = cv2 / 16;
  ledsColors[1].green = cv4 / 16; //red to yellow as the wet signal increases
  ledsColors[0].green = cv4 / 16;
  updateLEDs();
}