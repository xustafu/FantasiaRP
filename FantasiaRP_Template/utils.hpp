// some quality of life utils
#pragma once
#include "variants/FantasiaRP/pins_arduino.h"
#include <Arduino.h>
#include <pico-audio.h>
#include <Adafruit_NeoPixel.h>
// #include "button.h"
#include <SegmentDisplay.h>
#include <PicoEncoder.h>

// ============== CVs ==============

// you can call readCVs() to get the values for all cvs
// then use them thanks to the cv1 ... cv5 variables (they are ints from 0 to 4095 if you called setupCVs, otherwise 0-1024)
static const uint8_t CV1 = A5;
static const uint8_t CV2 = A4;
static const uint8_t CV3 = A3;
static const uint8_t CV4 = A2;
static const uint8_t CV5 = A1;

int cv1 = 0;
int cv2 = 0;
int cv3 = 0;
int cv4 = 0;
int cv5 = 0;

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
// ============== CVs ==============

// ============ DISPLAY ============
// Display LEDs Declaration
SegmentDisplay segmentDisplay(30, 32, 33, 28, 31, 26, 29, 9);

void setupDisplay() {
  // Set Display LEDs ports as Outputs
  pinMode(30, OUTPUT);
  pinMode(32, OUTPUT);
  pinMode(33, OUTPUT);
  pinMode(28, OUTPUT);
  pinMode(31, OUTPUT);
  pinMode(26, OUTPUT);
  pinMode(29, OUTPUT);
  pinMode(9, OUTPUT);

  segmentDisplay.displayHex(16, false);
}
// ============ DISPLAY ============


// ============ ENCODER ============
// Encoder enc(0, 1);
// long oldEncPos = -999;
PicoEncoder encoder;

void setupEncoder() {
  encoder.begin(4);
}


// ============ ENCODER ============