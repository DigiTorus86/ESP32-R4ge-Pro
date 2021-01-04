/***************************************************
* A wireless internet radio player with stereo output.  
* Includes a small list of stations.  
* Displays current track information once connected.
*
* Requires:
* - ESP32 R4ge Pro 
* - ESP8266Audio library:  https://github.com/earlephilhower/ESP8266Audio 

Copyright (c) 2021 Paul Pagel
This is free software; see the license.txt file for more information.
There is no warranty; not even for merchantability or fitness for a particular purpose.
****************************************************/

#include <Arduino.h>
#include <SD.h>
#include "esp32_r4ge_pro.h"
#include <WiFi.h>
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "tft_helper.h"
#include "icons.h"
#include "r4ge_pro_title.h"
#include "webradio_title.h"

// To run, update the SSID info and upload.

// TODO - Enter your WiFi setup here:
const char *SSID = "";
const char *PASSWORD = "";

// WARNING!  Internet radio stations come and go, and may change their characteristics.
// There is no guarantee that this list will work for you.
// Please plug in your own favorite stations and update the station count.

#define STATION_CNT 6
const char* station[STATION_CNT][2] = 
{ 
  { "PureRock.US", "http://167.114.64.181:8524/stream" },
  //{ "ShoutCast - Dance", "http://78.31.65.20:8080/dance.mp3" },
  //{ "ShoutCast - Classic Rock", "http://144.217.158.59:5098/stream" },
  { "The Oasis - Classic Rock", "http://janus.cdnstream.com:5014/;?esPlayer&cb=88591.mp3/;stream/1"},
  { "Hot Hitz 80's", "http://50.97.94.44:9900/stream" },
  { "The Big 80s Station", "http://149.56.155.73:8052/stream" },
  //{ "J-Pop Sakura 17", "http://144.217.253.136:8519/stream" },
  { "Ambient Sleeping Pill", "http://163.172.169.217:80/asp-s" },
  { "PsyRadio*FM", "http://81.88.36.42:8010/progressive/" }
  
};

uint8_t cur_station = 0;

bool btn_pressed[8], btn_released[8];
uint8_t tft_led_bright = 64;  // PWM setting for TFT LED.  0 = full on, 255 = off

AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

//XPT2046_Touchscreen ts(TCH_CS);
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);


void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string);
void StatusCallback(void *cbData, int code, const char *string);
void initAudioFile();
void showStations();
void showStatus(const char* message, uint32_t color);
void setup();
void loop();

/*
 * Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
 */
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1)-1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2)-1] = 0; // zero terminator
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();

  if (s1[0] == 'S' && s1[6] == 'T') // "StreamTitle"
  {
    showStatus("Currently Playing", ILI9341_DARKGREY);
    tft.fillRect(0, 154, 320, 68, ILI9341_BLACK);
    tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
    tft.setCursor(0, 159);
    //tft.print(s2);
    TFT_drawWordWrap(s2, 4, 26, &tft);
  }
}

/*
 * Called when there's a warning or error (like a buffer underflow or decode hiccup)
 */
void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}

/*
 * Begins audio playback on the currently-selected radio station 
 */
void initAudioFile()
{
  Serial.print("Loading station: ");
  Serial.println(station[cur_station][0]);
  Serial.flush();
  
  file = new AudioFileSourceICYStream(station[cur_station][1]);
  file->RegisterMetadataCB(MDCallback, (void*)"ICY");
  buff = new AudioFileSourceBuffer(file, 2048);
  buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT); // library default is BCLK=26, LRCK=25, DOUT=22 ... but badge uses DOUT=4 
  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
  mp3->begin(buff, out);
}

/*
 * Displays the station selection list (previous, current, next)
 */
void showStations()
{
  uint8_t prev_station = (cur_station > 0) ? cur_station - 1 :  STATION_CNT - 1;
  uint8_t next_station = (cur_station < STATION_CNT - 1) ? cur_station + 1 : 0;

  tft.fillRect(0, 35, 320, 30, ILI9341_DARKGREY);   // prev box
  tft.fillTriangle(2, 56, 7, 42, 12, 56, ILI9341_BLACK);  // up arrow
  tft.setTextColor(ILI9341_WHITE, ILI9341_DARKGREY);  
  tft.setTextSize(2);
  tft.setCursor(18, 42);
  tft.print(station[prev_station][0]);

  tft.fillRect(0, 65, 320, 30, ILI9341_BLUE);   // cur box
  tft.fillTriangle(2, 70, 2, 86, 10, 78, ILI9341_YELLOW);  // play arrow
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);  
  tft.setTextSize(2);
  tft.setCursor(18, 72);
  tft.print(station[cur_station][0]);

  tft.fillRect(0, 95, 320, 30, ILI9341_DARKGREY);   // next box
  tft.fillTriangle(2, 102, 7, 116, 12, 102, ILI9341_BLACK);  // down arrow
  tft.setTextColor(ILI9341_WHITE, ILI9341_DARKGREY);  
  tft.setTextSize(2);
  tft.setCursor(18, 102);
  tft.print(station[next_station][0]);

  showStatus("Getting Stream...", ILI9341_BLUE);
  tft.fillRect(0, 154, 320, 68, ILI9341_BLACK);  // erase now playing title area
}

/*
 * Displays the status line and message:  ---[ message ]---
 */ 
void showStatus(const char* message, uint32_t color)
{
  tft.drawLine(0, 142, 319, 142, ILI9341_DARKGREY);
  tft.setTextColor(ILI9341_DARKGREY, ILI9341_BLACK);  
  tft.setTextSize(2);
  tft.setCursor(30, 136);
  tft.print("[                   ]");
  tft.setTextColor(color, ILI9341_BLACK);  
  tft.setCursor(50, 136);
  tft.print(message);
}

/*
 * App setup and initialization.  Called once on startup.
 */
void setup()
{
  Serial.begin(115200);
  delay(500);

  // Set up shift register pins
  pinMode(SR_PL, OUTPUT);
  pinMode(SR_CP, OUTPUT);   
  pinMode(SR_Q7, INPUT);

  pinMode(TFT_CS, OUTPUT);
  pinMode(TCH_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);

  // Set up the TFT backlight brightness control
  ledcSetup(TFT_LED_CHANNEL, TFT_LED_FREQ, 8);  // 8 bit resolution
  ledcAttachPin(TFT_LED, TFT_LED_CHANNEL);
  ledcWrite(TFT_LED_CHANNEL, tft_led_bright);

  // Set up the TFT and touch screen
  tft.begin();
  tft.setRotation(SCREEN_ROT);

  //ts.begin();
  //ts.setRotation(TCHSCRN_ROT);

  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  //tft.println("ESP32 R4ge Pro Web Radio");

  tft.drawRGBBitmap(  0, 0, (uint16_t *)r4ge_pro_title, 175, 32);
  tft.drawRGBBitmap(196, 4, (uint16_t *)webradio_title,  94, 21);
  
  Serial.println("Connecting to WiFi");                          
  showStatus("Connecting...", ILI9341_YELLOW);

  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  
  WiFi.begin(SSID, PASSWORD);

  // Try forever
  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.println("...Connecting to WiFi");
    tft.fillRect(302, 0, 16, 11, ILI9341_BLACK);
    delay(500);
    tft.drawRGBBitmap(302, 0, (uint16_t *)wifi_ico, 16, 11);
    delay(500);
  }
  Serial.println("Connected");
  showStations();

  audioLogger = &Serial;
  initAudioFile();
}

/*
 * Main app loop.  Called repeatedly after setup.
 */
void loop()
{
  static int lastms = 0;

  if (mp3->isRunning()) 
  {
    if (millis() - lastms >= 1000) 
    {
      lastms = millis();
      //Serial.printf("Running for %d ms...\n", lastms);
      //Serial.flush();
    }
    if (!mp3->loop()) 
    {
      mp3->stop();
      //digitalWrite(LED_3, LOW); // stopped 
    }
  } 
    
  // Check button presses connected to the shift register
  digitalWrite(SR_CP, LOW);
  digitalWrite(SR_PL, LOW);
  delayMicroseconds(500);
  digitalWrite(SR_PL, HIGH);
  bool pressed = false;

  for(uint8_t i = 0; i < 8; i++)
  {
    pressed = (digitalRead(SR_Q7) == LOW ? 1: 0); // read the state of the serial data out
    btn_released[i] = !pressed && btn_pressed[i];
    btn_pressed[i] = pressed;
    // Shift the next button pin value into the serial data out
    digitalWrite(SR_CP, LOW); 
    delayMicroseconds(10);
    digitalWrite(SR_CP, HIGH);
    delayMicroseconds(10);
  }
  
  if (btn_released[BTN_DOWN]) // change station down
  {
    cur_station = (cur_station + 1) % STATION_CNT;
    showStations();
    mp3->stop();
    initAudioFile();
  }

  if (btn_released[BTN_UP]) // change station up
  {
    cur_station = (cur_station > 0) ? cur_station - 1 : STATION_CNT - 1;
    showStations();
    mp3->stop();
    initAudioFile();
  }

  if (btn_released[BTN_X]) // (re)start
  {
    mp3->stop();
    initAudioFile();
  }

  if (btn_released[BTN_Y]) // stop
  {
    mp3->stop();
  }

}