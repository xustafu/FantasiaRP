#pragma once

// ─── Chip identity ────────────────────────────────────────────────────────────
#define PICO_RP2350A        0
#define USBD_MAX_POWER_MA   250

// ─── USB identity ─────────────────────────────────────────────────────────────
#define USBD_MANUFACTURER   "Befaco"
#define USBD_PRODUCT        "PonyPlay"

// ─── PSRAM ────────────────────────────────────────────────────────────────────
#define PICO_PSRAM_CS_PIN   4

// ─── Built-in LED ─────────────────────────────────────────────────────────────
#define PIN_LED             (25u)
#define LED_BUILTIN         PIN_LED

// ─── Channel active LEDs ────────────────────────────
#define PIN_LED_A           (45u)
#define PIN_LED_B           (42u)

// ─── NeoPixel ─────────────────────────────────────────────────────────────────
#define PIN_NEOPIXEL        (39u)

// ─── Serial1 (UART0) ──────────────────────────────────────────────────────────
#define PIN_SERIAL1_TX      (0u)
#define PIN_SERIAL1_RX      (1u)

// ─── Serial2 (UART1) — Picoprobe ────────────────────────────────────────
#define PIN_SERIAL2_TX      (20u)
#define PIN_SERIAL2_RX      (21u)

// ─── SPI0 — SD card ───────────────────────────────────────────────────────────
#define PIN_SPI0_SCK        (34u)
#define PIN_SPI0_MOSI       (35u)
#define PIN_SPI0_MISO       (36u)
#define PIN_SPI0_SS         (37u)
#define PIN_SD_SCK          PIN_SPI0_SCK
#define PIN_SD_MOSI         PIN_SPI0_MOSI
#define PIN_SD_MISO         PIN_SPI0_MISO
#define PIN_SD_CS           PIN_SPI0_SS

// ─── SPI1 — free for digital I/O ──────────────────────────────────────────────
#define PIN_SPI1_SCK        (10u)
#define PIN_SPI1_MOSI       (11u)
#define PIN_SPI1_MISO       ( 8u)
#define PIN_SPI1_SS         ( 9u)

// ─── I2C0 ─────────────────────────────────────────────────────────────────────
#define PIN_WIRE0_SDA       (28u)
#define PIN_WIRE0_SCL       (29u)

// ─── I2C1 ─────────────────────────────────────────────────────────────────────
#define PIN_WIRE1_SDA       (26u)
#define PIN_WIRE1_SCL       (27u)

// ─── GPIO / ADC counts (RP2350B 80-pin QFN) ───────────────────────────────────
#define SERIAL_HOWMANY          (3u)
#define SPI_HOWMANY             (2u)
#define WIRE_HOWMANY            (2u)
#define WIRE_INTERFACES_COUNT   WIRE_HOWMANY
#define PINS_COUNT              (48u)
#define NUM_DIGITAL_PINS        (48u)
#define NUM_ANALOG_INPUTS       (8u)
#define NUM_ANALOG_OUTPUTS      (0u)
#define ADC_RESOLUTION          (12u)

// ─── Digital pin aliases D0-D47 ───────────────────────────────────────────────
#include <stdint.h>
static const uint8_t D0  =  0u; static const uint8_t D1  =  1u;
static const uint8_t D2  =  2u; static const uint8_t D3  =  3u;
static const uint8_t D4  =  4u; static const uint8_t D5  =  5u;
static const uint8_t D6  =  6u; static const uint8_t D7  =  7u;
static const uint8_t D8  =  8u; static const uint8_t D9  =  9u;
static const uint8_t D10 = 10u; static const uint8_t D11 = 11u;
static const uint8_t D12 = 12u; static const uint8_t D13 = 13u;
static const uint8_t D14 = 14u; static const uint8_t D15 = 15u;
static const uint8_t D16 = 16u; static const uint8_t D17 = 17u;
static const uint8_t D18 = 18u; static const uint8_t D19 = 19u;
static const uint8_t D20 = 20u; static const uint8_t D21 = 21u;
static const uint8_t D22 = 22u; static const uint8_t D23 = 23u;
static const uint8_t D24 = 24u; static const uint8_t D25 = 25u;
static const uint8_t D26 = 26u; static const uint8_t D27 = 27u;
static const uint8_t D28 = 28u; static const uint8_t D29 = 29u;
static const uint8_t D30 = 30u; static const uint8_t D31 = 31u;
static const uint8_t D32 = 32u; static const uint8_t D33 = 33u;
static const uint8_t D34 = 34u; static const uint8_t D35 = 35u;
static const uint8_t D36 = 36u; static const uint8_t D37 = 37u;
static const uint8_t D38 = 38u; static const uint8_t D39 = 39u;
static const uint8_t D40 = 40u; static const uint8_t D41 = 41u;
static const uint8_t D42 = 42u; static const uint8_t D43 = 43u;
static const uint8_t D44 = 44u; static const uint8_t D45 = 45u;
static const uint8_t D46 = 46u; static const uint8_t D47 = 47u;

// ─── ADC aliases A0-A7 → GPIO 40-47 ──────────────────────────────────────────
static const uint8_t A0 = 40u; static const uint8_t A1 = 41u;
static const uint8_t A2 = 42u; static const uint8_t A3 = 43u;
static const uint8_t A4 = 44u; static const uint8_t A5 = 45u;
static const uint8_t A6 = 46u; static const uint8_t A7 = 47u;

// ─── Encoder ──────────────────────────────────────────────────────────────────
#define PIN_ENC_A           (17u)
#define PIN_ENC_B           (18u)
#define PIN_ENC_SW          (41u)   // ADC A1

// ─── Trigger buttons / CV play inputs (SAME PIN per channel) ─────────────────
#define PIN_BTN_A           (19u)   // Ch A: button + CV play input
#define PIN_BTN_B           (38u)   // Ch B: button + CV play input

// ─── I2S audio output ─────────────────────────────────────────────────────────
#define PIN_I2S_BCLK        ( 0u)
#define PIN_I2S_DOUT        ( 2u)

// ─── 7-segment display ────────────────────────────────────────────────────────
#define PIN_SEG_DIG         (22u)
#define PIN_SEG_A           (16u)
#define PIN_SEG_B           ( 7u)
#define PIN_SEG_C           ( 6u)
#define PIN_SEG_D           (13u)
#define PIN_SEG_E           (12u)
#define PIN_SEG_F           (15u)
#define PIN_SEG_G           (14u)
#define PIN_SEG_DP_PIN      ( 5u)

// ─── Default peripheral aliases ───────────────────────────────────────────────
static const uint8_t SS   = PIN_SPI0_SS;
static const uint8_t MOSI = PIN_SPI0_MOSI;
static const uint8_t MISO = PIN_SPI0_MISO;
static const uint8_t SCK  = PIN_SPI0_SCK;
static const uint8_t SDA  = PIN_WIRE0_SDA;
static const uint8_t SCL  = PIN_WIRE0_SCL;