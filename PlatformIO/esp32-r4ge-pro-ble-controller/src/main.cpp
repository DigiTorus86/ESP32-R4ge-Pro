/***************************************************
ESP32 R4ge Pro BLE Controller (Peripheral role)

Allows you to use the joysticks and buttons of the ESP32 R4ge Pro
to control another ESP32/Bluetooth device operating in the central role.

Requires:
 - ESP32 R4ge Pro
 - Another ESP32 operating as the central server, i.e. the esp32-r4ge-pro-cmdr sketch

Copyright (c) 2021 Paul Pagel
This is free software; see the license.txt file for more information.
There is no warranty; not even for merchantability or fitness for a particular purpose.
*****************************************************/

#include "esp32_r4ge_pro.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "freertos/queue.h"

#include "r4ge_pro_title.h"

#define TOP_LINE      35
#define BOTTOM_LINE  180  

#define BLE_DEVICE_ID   "R4geCtrl"  // Adjust as desired, but keep short

// UUIDs for the BLE service and characteristic - must match the central
// Generate new GUIDs if you modify the data packet size/format
#define SERVICE_UUID        "788fce9d-5ba6-4e8f-9e98-5965b20ab856"
#define CHARACTERISTIC_UUID "e2cdb570-6340-4aa2-9ac3-ab32cfa371f3"

// These are all customized for a 4-byte packet used to control
// a tank robot.  Any changes here will need to be reflected in 
// the receiver/central role sketch.
#define PACKET_SIZE 4
#define DP_CMND     0
#define DP_LTRK     1
#define DP_RTRK     2
#define DP_ELEV     3

#define DP_CMND_LTRKFWD   1  
#define DP_CMND_LTRKBWD   2
#define DP_CMND_RTRKFWD   4
#define DP_CMND_RTRKBWD   8
#define DP_CMND_PICTURE  16

uint8_t data_packet[] = 
{
   0x00,  // command flags
   0x00,  // left tread/joystick (0 = fwd, 127 = stop, 255 = bkwd 
   0x00,  // right tread/joystick
   0x00,  // elevation position
};

BLECharacteristic*  pCharacteristic;
bool                device_connected = false;

bool btn_pressed[8], btn_released[8];
bool btnA_pressed, btnB_pressed, btnX_pressed, btnY_pressed;
bool btnUp_pressed, btnDown_pressed, btnLeft_pressed, btnRight_pressed;
uint8_t joy_x_left, joy_y_left, joy_x_right, joy_y_right; 
uint8_t elevate;


uint8_t tft_led_bright = 64;  // PWM setting for TFT LED.  0 = full on, 255 = off

// i2s configuration
// See https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/include/driver/driver/i2s.h

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

uint8_t getCommandFlags();
void sendControlChange();
void setTextColor(uint16_t color, bool inverted);
void updateScreen();
void checkButtonPresses();
void checkJoysticks();


class MyServerCallbacks: public BLEServerCallbacks 
{
    void onConnect(BLEServer* pServer) {
      device_connected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      device_connected = false;
    }
};

/*
 * Set up the board
 */
void setup() 
{
  Serial.begin(115200);
  Serial.println("ESP32 R4ge Pro BLE Controller"); 
  delay(100);

  // Set up the TFT backlight brightness control
  // Keep Hz out of audible range.  Lower does lead to visible flickering
  ledcSetup(TFT_LED_CHANNEL, 20000, 8);  // Hz max, 8 bit resolution.  
  ledcAttachPin(TFT_LED, TFT_LED_CHANNEL);
  ledcWrite(TFT_LED_CHANNEL, tft_led_bright);

  // Set up shift register pins
  pinMode(SR_PL, OUTPUT);
  pinMode(SR_CP, OUTPUT);   
  pinMode(SR_Q7, INPUT);

  // Set up the joysticks
  pinMode(JOYX_L, INPUT);
  pinMode(JOYY_L, INPUT);
  pinMode(JOYX_R, INPUT);
  pinMode(JOYY_R, INPUT);
  //pinMode(JBTN_L, INPUT_PULLUP);
  //pinMode(JBTN_R, INPUT_PULLUP);

  delay(100);
  
  // Set up the TFT and touch screen
  tft.begin();
  tft.setRotation(SCREEN_ROT);

  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);  
 
  tft.drawRGBBitmap(  0, 0, (uint16_t *)r4ge_pro_title, 175, 32);

  tft.setCursor(195, 8);
  tft.setTextSize(2);
  tft.print("BLE Ctrl");

  tft.drawLine(0, TOP_LINE, SCREEN_WD, TOP_LINE, ILI9341_BLUE);   // top line
  tft.drawLine(0, BOTTOM_LINE, SCREEN_WD, BOTTOM_LINE, ILI9341_BLUE); // bottom line

  // Set up BLE
  
  BLEDevice::init(BLE_DEVICE_ID);

  Serial.print("MAC Address: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());

  tft.setTextSize(2);
  tft.setCursor(0, 190);
  tft.setTextColor(ILI9341_ORANGE);
  tft.print("ADDR: ");
  tft.print(BLEDevice::getAddress().toString().c_str());
    
  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("Callbacks created.");
  
  // Create the BLE Service
  BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    BLEUUID(CHARACTERISTIC_UUID),
    BLECharacteristic::PROPERTY_READ   |
    BLECharacteristic::PROPERTY_WRITE  |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_WRITE_NR
  );
  Serial.println("BLE service characteristic created.");
  
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();
  Serial.println("BLE service started.");
  
  // Start advertising
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  pAdvertising->start();
  Serial.println("BLE advertising started.");
}

/*
 * Returns the button states as a byte of bit flags
 */
uint8_t getCommandFlags()
{
  uint8_t cmd_flags = 0x00;

  if (joy_y_left > 155) cmd_flags |= DP_CMND_LTRKFWD | DP_CMND_RTRKFWD;
  if (joy_y_left < 50) cmd_flags |= DP_CMND_LTRKBWD | DP_CMND_RTRKBWD;
  if (joy_y_right < 50) cmd_flags |= DP_CMND_RTRKFWD;
  if (joy_y_right > 155) cmd_flags |= DP_CMND_LTRKFWD;
  
  if (btnA_pressed) cmd_flags |= DP_CMND_PICTURE;  

  return cmd_flags;
}

/*
 * Sends a "control change" message
 */
void sendControlChange()
{
  int16_t joy1 = abs(128 - joy_y_left);
  uint8_t joy1byte = (joy1 < 127) ? (joy1 & 0xFF) : 127;

  int16_t joy2 = abs(128 - joy_y_right);
  uint8_t joy2byte = (joy2 < 127) ? (joy2 & 0xFF) : 127;

  tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
  data_packet[0] = getCommandFlags();
  data_packet[1] = joy1byte;
  data_packet[2] = joy2byte;  
  data_packet[3] = elevate;
  
  if (device_connected) 
  {
    digitalWrite(ESP_LED, HIGH);
    pCharacteristic->setValue(data_packet, PACKET_SIZE); 
    pCharacteristic->notify();
    digitalWrite(ESP_LED, LOW);

    tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  }

  // display the "sent" data for debugging
  tft.setCursor(260, 60);
  tft.print("Data");
  tft.setCursor(260, 80);
  tft.print(data_packet[0]); tft.print(" ");
  tft.setCursor(260, 100);    
  tft.print(data_packet[1]); tft.print(" ");
  tft.setCursor(260, 120);   
  tft.print(data_packet[2]); tft.print(" ");
  tft.setCursor(260, 140);   
  tft.print(data_packet[3]); tft.print(" ");
}

/*
 * Use to toggle between normal and inverted text display
 */
void setTextColor(uint16_t color, bool inverted)
{
  if (inverted)
    tft.setTextColor(ILI9341_BLACK, color);
  else
    tft.setTextColor(color, ILI9341_BLACK);
}

/*
 * Updates the screen based on which buttons are being pressed
 */
void updateScreen()
{
  // Direction buttons
  tft.setCursor(45, 50);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_UP]);
  tft.print("UP");

  tft.setCursor(0, 70);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_LEFT]);
  tft.print("<-LT"); 

  tft.setCursor(60, 70);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_RIGHT]);
  tft.print("RT->");
 
  tft.setCursor(45, 90);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_DOWN]);
  tft.print("DN");

  int x = 140;

  // Action buttons
  tft.setCursor(x+30, 50);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_Y]);
  tft.print("Y"); 

  tft.setCursor(x, 70);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_X]);
  tft.print("X"); 
 
  tft.setCursor(x+60, 70);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_B]);
  tft.print("B");

  tft.setCursor(x+30, 90);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_A]);
  tft.print("A");

  // joysticks / rage cons
  tft.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  tft.setCursor(28, 125);
  tft.print("X:");
  tft.print(joy_x_left);
  tft.print("  ");

  tft.setCursor(28, 145);
  tft.print("Y:");
  tft.print(joy_y_left);
  tft.print("  ");
    
  tft.setCursor(x, 125);
  tft.print("X:");
  tft.print(joy_x_right);
  tft.print("  ");

  tft.setCursor(x, 145);
  tft.print("Y:");
  tft.print(joy_y_right);
  tft.print("  ");
}

/*
 * Check button presses connected to the shift register
 */
void checkButtonPresses()
{
  bool pressed = false;
  
  digitalWrite(SR_CP, LOW);
  digitalWrite(SR_PL, LOW);
  delay(5);
  digitalWrite(SR_PL, HIGH);

  for(uint8_t i = 0; i < 8; i++)
  {
    pressed = (digitalRead(SR_Q7) == LOW ? 1: 0);// read the state of the SO:
    btn_released[i] = !pressed && btn_pressed[i];
    btn_pressed[i] = pressed;
    // Shift the next button pin value into the serial data out
    digitalWrite(SR_CP, LOW);
    delay(1);
    digitalWrite(SR_CP, HIGH);
    delay(1);
    //Serial.print(i); Serial.print(": "); Serial.println(btn_pressed[i]);
  }
}

/*
 * Check the analog joystick values
 */
void checkJoysticks()
{
  // Only use the left 8 bits of the joystick pot readings
  // May want to use analogReadResolution() at some point to eliminate the need for shifts
  joy_x_left = analogRead(JOYX_L) >> 4;
  joy_y_left = analogRead(JOYY_L) >> 4;
  
  joy_x_right = analogRead(JOYX_R) >> 4;
  joy_y_right = analogRead(JOYY_R) >> 4;
}

/*
 * Main program loop.  Called continuously after setup.
 */
void loop(void) 
{
  checkButtonPresses();
  checkJoysticks();

  if (btn_pressed[BTN_DOWN])
  {
    if (elevate > 0) elevate -= 1;
  }

  if (btn_pressed[BTN_DOWN])
  {
    if (elevate < 255) elevate += 1;
  }

  sendControlChange();
  updateScreen();
  delay(200);
}