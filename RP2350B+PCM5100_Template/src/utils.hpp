// Hardware definition for Pony Env Beta 3 (RP2350B + PCM5100)
// Pin map traced from B3Kicad/Pony_env_B3.kicad_sch (RP2350B = X3)
#pragma once
#include "../variants/FantasiaRP/pins_arduino.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <pico-audio.h>
#include <Bounce2.h>

// =========================================================================
//  PIN MAP
// =========================================================================

// ---- I2S -> PCM5100 (IC8) ----
// PCM5100 left  (OUTL) -> IC14B -> ENV output jack (also drives the VCA CV)
// PCM5100 right (OUTR) -> IC14A -> ENV_ATTE output jack
#define PIN_I2S_BCK   0   // X3.77 -> IC8 BCK
#define PIN_I2S_LRCK  1   // X3.78 -> IC8 LRCK
#define PIN_I2S_DATA  2   // X3.79 -> IC8 DIN
#define DAC_CH_ENV       0  // left
#define DAC_CH_ENV_ATTE  1  // right

// Output scaling (both channels): PCM5100 full scale = 2.1 Vrms = 2.97 Vpk,
// then inverting stage gain = -39k / (2.2k + 10k) = -3.2  ->  -9.49 V per 1.0 FS.
// Trim this constant after measuring a real unit.
#define OUT_VOLTS_PER_FS (-9.49f)

// Convert a desired voltage at the output jack to a DAC sample value (-1..1)
inline float jackVoltsToDac(float volts) { return volts / OUT_VOLTS_PER_FS; }

// ---- Analog (slider/pot + CV jack summed by an inverting MCP6002 stage) ----
// Vadc = -(Vpot + 0.33 * Vcv), pot spans 0..-10V, clipped to 0..3.3V:
// moving the slider/pot towards -10V raises the reading, positive CV lowers it.
#define PIN_ATTACK   A7   // GPIO47 / ADC7 : ATTACK slider (U$8) + ATTACK CV jack
#define PIN_DECAY    A6   // GPIO46 / ADC6 : DECAY slider (U$13) + DECAY CV jack
#define PIN_LEVEL    A5   // GPIO45 / ADC5 : SPEED pot (SPEED0) + LVL CV jack

// ---- Gates ----
#define PIN_TRIG_IN   12  // GPIO12 : TRIGG jack via Q7, INVERTED (LOW = trigger high), ext. 10k pull-up R65
#define PIN_EOC_OUT   18  // GPIO18 : EOC jack via Q1, INVERTED (GPIO LOW = jack high ~+11V)

// ---- Trigger button with NeoPixel (TRIG_BUT0) ----
#define PIN_TRIG_BUTTON 44 // GPIO44 : shorts to GND when pressed -> INPUT_PULLUP
#define PIN_NEOPIXEL    43 // GPIO43 : WS2812 DIN via R22 (560R)
#define NUMPIXELS       1  // only the LED inside the button (DOUT not connected)

// ---- 4-position slide switches (common = 3.3V, active HIGH, one-hot) ----
static const uint8_t MODE_PINS[4]  = {42, 41, 40, 33}; // MODE1..MODE4   (MODE0)
static const uint8_t SHAPE_PINS[4] = {35, 34, 31, 37}; // SHAPE1..SHAPE4 (ENV_SHAPE0)
static const uint8_t RANGE_PINS[4] = {19, 39, 38, 36}; // RANGE1..RANGE4 (ENV_RANGE0)

// Not available on this board: SWD is unconnected, no 7-segment display,
// no encoder. Audio IN L/R go straight to the analog VCA (AS3360), not to the MCU.

// =========================================================================
//  NEOPIXEL
// =========================================================================
struct color
{
  uint8_t red = 255;
  uint8_t green = 0;
  uint8_t blue = 0;
};

color ledsColors[NUMPIXELS];
#define LED_BRIGHTNESS 50
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

void setupNeopixel()
{
  pixels.begin();
  pixels.setBrightness(LED_BRIGHTNESS);
  pixels.show();
}

void updateNeopixel()
{
  for (uint8_t i = 0; i < NUMPIXELS; i++)
  {
    pixels.setPixelColor(i, ledsColors[i].red, ledsColors[i].green, ledsColors[i].blue);
  }
  pixels.show();
}

// =========================================================================
//  ANALOG CONTROLS (0..4095)
// =========================================================================
int attackCV = 0;
int decayCV = 0;
int levelCV = 0;

void setupCVs()
{
  analogReadResolution(12);
}

void readCVs()
{
  attackCV = analogRead(PIN_ATTACK);
  decayCV  = analogRead(PIN_DECAY);
  levelCV  = analogRead(PIN_LEVEL);
}

// =========================================================================
//  SWITCHES (position 0..3, -1 while between positions)
// =========================================================================
int8_t modeSwitch = -1;
int8_t shapeSwitch = -1;
int8_t rangeSwitch = -1;

void setupSwitches()
{
  for (uint8_t i = 0; i < 4; i++)
  {
    pinMode(MODE_PINS[i], INPUT_PULLDOWN);
    pinMode(SHAPE_PINS[i], INPUT_PULLDOWN);
    pinMode(RANGE_PINS[i], INPUT_PULLDOWN);
  }
}

int8_t readSwitch(const uint8_t pins[4])
{
  for (uint8_t i = 0; i < 4; i++)
  {
    if (digitalRead(pins[i]) == HIGH)
      return i;
  }
  return -1;
}

void readSwitches()
{
  modeSwitch  = readSwitch(MODE_PINS);
  shapeSwitch = readSwitch(SHAPE_PINS);
  rangeSwitch = readSwitch(RANGE_PINS);
}

// =========================================================================
//  GATES & BUTTON
// =========================================================================
Bounce trigButton = Bounce();
Bounce trigIn = Bounce();

void setupGates()
{
  trigButton.attach(PIN_TRIG_BUTTON, INPUT_PULLUP);
  trigButton.interval(5);
  trigIn.attach(PIN_TRIG_IN, INPUT); // external pull-up R65
  trigIn.interval(1);
  pinMode(PIN_EOC_OUT, OUTPUT);
  digitalWrite(PIN_EOC_OUT, HIGH);   // HIGH = EOC jack low
}

void readGates()
{
  trigButton.update();
  trigIn.update();
}

// logical helpers (inversions handled here)
bool trigButtonPressed() { return trigButton.read() == LOW; }
bool trigButtonPushed()  { return trigButton.fell(); }
bool trigInHigh()        { return trigIn.read() == LOW; }
bool trigInRising()      { return trigIn.fell(); }
void setEOC(bool high)   { digitalWrite(PIN_EOC_OUT, high ? LOW : HIGH); }

// =========================================================================
//  SETUP / READ ALL
// =========================================================================
void setupControls()
{
  setupCVs();
  setupSwitches();
  setupGates();
  setupNeopixel();
}

void readControls()
{
  readCVs();
  readSwitches();
  readGates();
}

// =========================================================================
//  PRINT-TESTS
// =========================================================================
// switch position as printable char: '1'..'4', '-' while between positions
char switchChar(int8_t pos) { return pos < 0 ? '-' : char('1' + pos); }

void printControls()
{
  Serial.printf("ATTACK %4d  DECAY %4d  SPEED/LVL %4d | BOTON %s | MODE %c  SHAPE %c  RANGE %c\n",
                attackCV, decayCV, levelCV,
                trigButtonPressed() ? "ON " : "OFF",
                switchChar(modeSwitch), switchChar(shapeSwitch), switchChar(rangeSwitch));
}

void printNeoPixel()
{
  pixels.clear();
  for (int i = 0; i < NUMPIXELS; i++)
  {
    pixels.setPixelColor(i, pixels.Color(0, 150, 0));
    pixels.show();
    delay(500);
    Serial.printf("Pixel %d: %lu\n", i, (unsigned long)pixels.getPixelColor(i));
  }
}
