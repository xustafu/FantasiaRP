// some quality of life utils
#pragma once
#include "variants/FantasiaRP/pins_arduino.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <pico-audio.h>
#include <SegmentDisplay.h>
// #include "button.h"
//#include <PicoEncoder.h>



// ============== Neopixel ==========

// Which pin on rp2350 is connected to the NeoPixels?
#define PINLED 39

// How many NeoPixels are attached to the line?
#define NUMPIXELS 2
Adafruit_NeoPixel pixels(NUMPIXELS, PINLED, NEO_GRB + NEO_KHZ800);

// ============== CVs ==============

// you can call readCVs() to get the values for all cvs
// then use them thanks to the cv1 ... cv5 variables (they are ints from 0 to 4095 if you called setupCVs, otherwise 0-1024)
static const uint8_t CV1 = A3;
static const uint8_t CV2 = A7;
static const uint8_t CV3 = A4;
static const uint8_t CV4 = A5;
static const uint8_t CV5 = A6;

int cv1 = 0;
int cv2 = 0;
int cv3 = 0;
int cv4 = 0;
int cv5 = 0;


// ============ DISPLAY ============
// Display LEDs Declaration (16, 7, 6, 13, 12, 15, 14, 5)
SegmentDisplay segmentDisplay(12, 13, 6, 5, 7, 16, 14, 15);

void setupDisplay() {
  // Set Display LEDs ports as Outputs
  pinMode(16, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(5, OUTPUT);

  segmentDisplay.displayHex(16, false);
}

void setupNeopixel() {

  pixels.begin(); // INITIALIZE NeoPixel object
}

void setupCVs() {
  analogReadResolution(12);
}

void readCVs(uint8_t crop = 0) {
  cv1 = analogRead(CV1);
  cv2 = analogRead(CV2);
  cv3 = analogRead(CV3);
  cv4 = analogRead(CV4);
  cv5 = analogRead(CV5);
  if (crop > 0)
  {
    cv1 = max(crop, cv1);
    cv2 = max(crop, cv2);
    cv3 = max(crop, cv3);
    cv4 = max(crop, cv4);
    cv5 = max(crop, cv5);
  }
}
void printNeoPixel() {
    pixels.clear(); // Set all pixel colors to 'off'
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 150, 0));
    pixels.show();   // Send the updated pixel colors to the hardware.
    delay(500); // Pause for a moment to see the change
    Serial.print("Pixel "); //Print LED colors in Serial Monitor
    Serial.print(i);
    Serial.print(": ");
    Serial.print(pixels.getPixelColor(i));
    Serial.print("\n");
  }
}
void printCVs() {
    Serial.print("CV 1: ");
    Serial.print(cv1);
    Serial.print("\n");
    Serial.print("CV 2: ");
    Serial.print(cv2);
    Serial.print("\n");
    Serial.print("CV 3: ");
    Serial.print(cv3);
    Serial.print("\n");
    Serial.print("CV 4: ");
    Serial.print(cv4);
    Serial.print("\n");
    Serial.print("CV 5: ");
    Serial.print(cv5);
    Serial.print("\n");
}
void printSevenSegment() {
segmentDisplay.testDisplay();
}
// ============== CVs ==============



// ============ ENCODER ============
// Encoder enc(0, 1);
// long oldEncPos = -999;
//PicoEncoder encoder;

//void setupEncoder() {
//  encoder.begin(4);
//}


// ============ ENCODER ============