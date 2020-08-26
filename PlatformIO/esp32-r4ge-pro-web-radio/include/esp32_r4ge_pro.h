#ifndef _ESP32_R4GE_PRO_
#define _ESP32_R4GE_PRO_

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Pin assignments for v1.2 board
// Currently unused pin = IO2 (built-in LED on nodeMCU-s)

// GPIO pin assignments for the ILI9341 TFT display
#define TFT_DC    27  // Data (high)/Command (low) pin
#define TFT_CS     5  // SPI Chip Select pin for screen (active low)
#define TFT_RST   -1  // Reset pin not used, connected to 3V3 (always high)
#define TFT_MISO  19  // SPI Master In Slave Out       
#define TFT_MOSI  23  // SPI Master Out Slave In         
#define TFT_CLK   18  // SPI Clock
#define TFT_LED   32  // TFT LED brightness control
#define TCH_CS    13  // SPI Touch controller Chip Select (active low)
#define TCH_IRQ   -1  // Touch IRQ interrupt - Not Connected
#define SD_CS     15  // SPI SD card reader Chip Select pin (active low)

// Shift register GPIO assignments, tie SR pin 15 (CE) to GND
#define SR_PL      0  // Parallel Load,   SR pin 1
#define SR_CP     33  // Clock Pulse,     SR pin 2  
#define SR_Q7     14  // Serial data out, SR pin 9

#define JOYX_L    36  // Left analog joystick X axis
#define JOYY_L    39  // Left analog joystick Y axis
#define JBTN_L    -1  // Left joystick button - Not Connected
#define JOYX_R    34  // Right analog joystick X axis
#define JOYY_R    35  // Right analog joystick Y axis
#define JBTN_R    -1  // Right joystick button - Not Connected

#define MIC       12  // Microphone analog signal, input only
#define I2S_LRCK  25  // I2S left right select, also known as WSEL/word select (DAC1 pin)
#define I2S_BCLK  26  // I2S bit clock (DAC2 pin)
#define I2S_DOUT   4  // Sound data out to DIN pin on PCM5102a  (was 22 on rev1.0, 4 on rev1.1)

#define ESP_LED    2  // blue built-in LED on ESP32 dev board

// ESP32 DevKit I2C Pins (broken out on left of display)
//      SDA       21
//      SCL       22

// Serial 2 pins (broken out on right of display)
//      RX2       16
//      TX2       17

// Shift register parallel data D# pin assignments for the badge buttons (not GPIO pins!)
#define BTN_Y     0 
#define BTN_X     1  
#define BTN_B     2 
#define BTN_A     3 
#define BTN_RIGHT 4 
#define BTN_LEFT  5 
#define BTN_DOWN  6 
#define BTN_UP    7 

#define TFT_LED_CHANNEL 5 // screen brightness PWM control (lower channels used by I2S)

// Calibration constants
// These may need to be adjusted due to variations in boards and joystick pots
#define MIC_OFFSET 1800
#define JOYX_L_CTR  462
#define JOYY_L_CTR  456
#define JOYX_R_CTR  473
#define JOYY_R_CTR  468
#define JOY_5BIT_CTR 14

// Screen properties and orientation
#define SCREEN_ROT    3  // horizontal, w/SD pins on the right
#define SCREEN_WD   320
#define SCREEN_HT   240
#define TCHSCRN_ROT   1  // horizontal

//  Keep Freq Hz out of audible range.  Lower does lead to visible flickering
#define TFT_LED_FREQ 20000

#endif // _ESP32_R4GE_PRO_
