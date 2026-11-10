# FantasiaRP
Fantasia para RP2350B


## Installation Steps:  

  * Install free [VSCode](https://code.visualstudio.com/download).  
  * Add the free [PlatformIO extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide). 
  * Download this repo
  * Open it up with VSCode
  * Click the platformio compile + upload button
  * That's it!


Change pin numbers for Serial(UART), SPI, Wire(i2c...) and LED(pixels)  in your pins_arduino.h file.  
In utils.hpp, initialize objects, some of them need pins numbers like 7Segment or buttons.  
Enjoy :)


## Pony Env (Beta 3) pin map

Traced from `RP2350B+PCM5100_Template/B3Kicad/Pony_env_B3.kicad_sch`. All definitions live in `src/utils.hpp`.

| Control | GPIO | Notes |
|---|---|---|
| ATTACK slider + CV | 47 (A7) | analog, inverting summing stage |
| DECAY slider + CV | 46 (A6) | analog |
| SPEED pot + LVL CV | 45 (A5) | analog |
| TRIGG in | 12 | inverted by Q7, external pull-up |
| EOC out | 18 | inverted by Q1 (GPIO LOW = jack high) |
| Trigger button | 44 | to GND when pressed (`INPUT_PULLUP`) |
| Button NeoPixel | 43 | 1 pixel |
| MODE 1-4 | 42, 41, 40, 33 | switch common = 3.3V, active HIGH |
| SHAPE 1-4 | 35, 34, 31, 37 | active HIGH |
| RANGE 1-4 | 19, 39, 38, 36 | active HIGH |
| I2S BCK / LRCK / DIN | 0 / 1 / 2 | PCM5100: L = ENV out, R = ENV_ATTE out |

Output scaling: about -9.49 V at the jack per 1.0 DAC full scale (inverted). Use `jackVoltsToDac()` to set values in jack volts.
