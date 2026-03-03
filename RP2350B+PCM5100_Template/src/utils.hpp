// some quality of life utils
#pragma once
#include "../variants/FantasiaRP/pins_arduino.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <pico-audio.h>
#include <SegmentDisplay.h>
#include <Bounce2.h>
//#include <PicoEncoder.h>

// ============== Neopixel ==========
// How many NeoPixels are attached to the line?
#define NUMPIXELS 2

struct color
{
  uint8_t red = 255;
  uint8_t green = 0;
  uint8_t blue = 0;
};

color ledsColors[NUMPIXELS];
#define LED_BRIGHTNESS 50
// Which pin on rp2350 is connected to the NeoPixels?
#ifndef PIN_LED
#define PIN_LED 39
#endif
//define our pixels
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_LED, NEO_GRB + NEO_KHZ800);

void setupNeopixel()
{
  pixels.begin(); // INITIALIZE NeoPixel object
  pixels.show();
  pixels.setBrightness(LED_BRIGHTNESS);
}

void updateNeopixel()
{
  for (uint8_t i = 0; i < 1 + NUMPIXELS; i++)
  {
    pixels.setPixelColor(i, ledsColors[i].red, ledsColors[i].green, ledsColors[i].blue);
  }
  pixels.show();
  // delay(2);
}
// ============= LEDs ==============



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

void setupCVs()
{
  analogReadResolution(12);
}

void readCVs(uint8_t crop = 0)
{
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
// ============== CVs ==============

// ============ DISPLAY ============
// based on this part number: FYS-2811buhr-21
// (https://cetest02.cn-bj.ufileos.com/100001_2003185297/1%20FYS-2811A-BX-XX.pdf)

// Display LEDs Declaration   E   D   C  DP B   A   G   F
SegmentDisplay segmentDisplay(12, 13, 6, 5, 7, 16, 14, 15);

void setupDisplay() {

  segmentDisplay.displayHex(15, false);
}
// ============ DISPLAY ============



// ============ BUTTONS ============
// Buttons 1-2 = 19, 38 ?? 
Bounce button1 = Bounce(19, 5); // 5 ms debounce time
Bounce button2 = Bounce(38, 5);

void setupButtons(uint8_t pullup = 1)
{
  if (pullup == 1)
  {
    pinMode(13, INPUT_PULLUP);
    pinMode(16, INPUT_PULLUP);
  }
  else
  {
    pinMode(13, INPUT_PULLDOWN);
    pinMode(16, INPUT_PULLDOWN);
  }
}

void readButtons()
{
  button1.update();
  button2.update();
}
// ============ BUTTONS ============


// ========== PRINT-TESTS ==========
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


void printButtons(){
  if (button1.fallingEdge())
  {
    Serial.println("Button 1 Pressed");
  } else if (button1.risingEdge())
  {
    Serial.println("Button 1 Released");
  }
  if (button2.fallingEdge())
  {
    Serial.println("Button 2 Pressed");
  }
  else if (button2.risingEdge())
  {
    Serial.println("Button 2 Released");
  }
}
// ========== PRINT-TESTS ==========



// ============ ENCODER ============
// Encoder enc(18, 44);
// long oldEncPos = -999;
//PicoEncoder encoder;

//void setupEncoder() {
//  encoder.begin(4);
//}


// ============ ENCODER ============