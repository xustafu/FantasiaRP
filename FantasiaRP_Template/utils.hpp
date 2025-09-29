// some quality of life utils
#pragma once
#include "variants/FantasiaRP/pins_arduino.h"
#include <Arduino.h>
// #include "audiotest.hpp" //included at the end of file
#include <Adafruit_NeoPixel.h>
// #include "button.h"
#include <SegmentDisplay.h>
#include <PicoEncoder.h>
#include <Bounce2.h>

// ============= LEDs ==============

struct color
{
  uint8_t red = 255;
  uint8_t green = 0;
  uint8_t blue = 0;
};
#define LED_BRIGHTNESS 50
color ledsColors[NUM_LEDS];
// CRGB leds[NUM_LEDS];
Adafruit_NeoPixel leds(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);

void setupLEDs(){
  leds.begin();
  leds.show();
  leds.setBrightness(LED_BRIGHTNESS);
}

void updateLEDs(){
  // leds.clear();
  for (uint8_t i = 0; i < 1 + NUM_LEDS; i++)
  {
    leds.setPixelColor(i, ledsColors[i].red, ledsColors[i].green, ledsColors[i].blue);
  }
  leds.show();
  // delay(2);
}

// ============= LEDs ==============



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

void readCVs(uint8_t low_threshold = 0) {
  cv1 = analogRead(CV1);
  cv2 = analogRead(CV2);
  cv3 = analogRead(CV3);
  cv4 = analogRead(CV4);
  cv5 = analogRead(CV5);
  if (low_threshold > 0)
  {
    cv1 = max(low_threshold, cv1);
    cv2 = max(low_threshold, cv2);
    cv3 = max(low_threshold, cv3);
    cv4 = max(low_threshold, cv4);
    cv5 = max(low_threshold, cv5);
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
// part number: FYS-2811buhr-21
// (https://cetest02.cn-bj.ufileos.com/100001_2003185297/1%20FYS-2811A-BX-XX.pdf)

// SegmentDisplay segmentDisplay(23, 22, 24, 19, 18, 21, 20, 25);
// E   D   C   DP  B   A   G   F
SegmentDisplay segmentDisplay(18, 19, 24, 25, 22, 23, 21, 20);

void setupDisplay() {
  // segmentDisplay.testDisplay();
  segmentDisplay.displayHex(15, false);
}
// ============ DISPLAY ============




// ============ ENCODER ============
// pins 31, 32. Button in 29
// Encoder enc(31, 32);
// long oldEncPos = -999;
#define ENC_A_PIN 33//31
#define ENC_B_PIN 32//32
// PicoEncoder encoder;
static const uint8_t EncoderButton = 29;
uint8_t button_history[2];
static int8_t Encoder_inc;
int8_t encoderPosition = 0;

void setupEncoder() {
  // uint8_t success = encoder.begin(31);
  // Serial.printf("Encoder init %s \n", success == 0 ? "successfully" : "with ERROR"); //0 success //1 no PIO //2 too many instances
  pinMode(EncoderButton, INPUT);
  // Encoder A and B pins
  gpio_init(ENC_A_PIN);
  gpio_set_dir(ENC_A_PIN, GPIO_IN);
  gpio_set_pulls(ENC_A_PIN, false, false);

  gpio_init(ENC_B_PIN);
  gpio_set_dir(ENC_B_PIN, GPIO_IN);
  gpio_set_pulls(ENC_B_PIN, false, false);
}

void readEncoder(){
  // encoder.update();
  // Encoder rotation
  // A
  button_history[0] = (button_history[0] << 1) | !gpio_get(ENC_A_PIN);
  // B
  button_history[1] = (button_history[1] << 1) | !gpio_get(ENC_B_PIN);

  // infer increment direction
  // reset increment first
  Encoder_inc = 0;

  // debounce
  if ((button_history[0] & 0x03) == 0x02 && (button_history[1] & 0x03) == 0x00)
  // if(button_history[ENC_A] == 1 && (button_history[ENC_B] & 0x03 == 0))
  {
    Encoder_inc = -1;
  }
  else if ((button_history[1] & 0x03) == 0x02 && (button_history[0] & 0x03) == 0x00)
  // else if(button_history[ENC_B] == 1 && (button_history[ENC_A] & 0x03 == 0))
  {
    Encoder_inc = 1;
  }

  encoderPosition = encoderPosition + Encoder_inc;
}

void printEncoder(){
  // Serial.printf("Encoder position: %u \n", encoder.step);
  // Serial.printf("Encoder sub-step: %i \n", encoder.position);
  // Serial.printf("Encoder speed: %i sub-steps per second \n", encoder.speed);
  Serial.printf("Encoder Position: %i \n", encoderPosition);
}

static inline int8_t get_Encoder_inc()
{
  return Encoder_inc;
}

// ============ ENCODER ============


// ============== LEDs =============
// leds 33
// ============== LEDs =============


// ============= GATES =============
Bounce gateIn = Bounce(12, 1); // 1 ms debounce time

void setupGates(uint8_t pullup = 1){
  // GATE1 = 12 (INPUT)
  if (pullup == 1)
  {
    pinMode(12, INPUT_PULLUP);
  } else {
    pinMode(12, INPUT_PULLDOWN);
  }
  // GATE2 = 17 (OUTPUT)
  pinMode(17, OUTPUT);
}

void readGateIn(){
  gateIn.update();
}

void writeGateOut(int status){
  digitalWrite(17, status);
}

void printGateIn()
{
  if (gateIn.fallingEdge())
  {
    Serial.println("Gate In Low");
  }
  else if (gateIn.risingEdge())
  {
    Serial.println("Gate In High");
  }
}
  // ============= GATES =============


  // ============ BUTTONS ============
  // Encoder button = 29
  // Buttons 1-4 = 13, 16, 28, 30
  Bounce button1 = Bounce(13, 5); // 5 ms debounce time
  Bounce button2 = Bounce(16, 5);
  Bounce button3 = Bounce(28, 5);
  Bounce button4 = Bounce(30, 5);
  Bounce buttonEncoder = Bounce(29, 5);

  void setupButtons(uint8_t pullup = 1)
  {
    if (pullup == 1)
    {
      pinMode(13, INPUT_PULLUP);
      pinMode(16, INPUT_PULLUP);
      pinMode(28, INPUT_PULLUP);
      pinMode(30, INPUT_PULLUP);
      pinMode(29, INPUT_PULLUP);
    } else {
    pinMode(13, INPUT_PULLDOWN);
    pinMode(16, INPUT_PULLDOWN);
    pinMode(28, INPUT_PULLDOWN);
    pinMode(30, INPUT_PULLDOWN);
    pinMode(29, INPUT_PULLDOWN);
  }
}

void readButtons(){
  button1.update();
  button3.update();
  button2.update();
  button4.update();
  buttonEncoder.update();
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
  if (button3.fallingEdge())
  {
    Serial.println("Button 3 Pressed");
  }
  else if (button3.risingEdge())
  {
    Serial.println("Button 3 Released");
  }
  if (button4.fallingEdge())
  {
    Serial.println("Button 4 Pressed");
  }
  else if (button4.risingEdge())
  {
    Serial.println("Button 4 Released");
  }
  if (buttonEncoder.fallingEdge())
  {
    Serial.println("Button Encoder Pressed");
  }
  else if (buttonEncoder.risingEdge())
  {
    Serial.println("Button Encoder Released");
  }
}
// ============ BUTTONS ============


// ============= AUDIO =============
#include "audiotest.hpp" //at the end to trick the compiler and use all hardware