/***************************************************
ESP32 R4ge Pro test app

Requires:
 - ESP32 R4ge Pro

Copyright (c) 2020 Paul Pagel
This is free software; see the license.txt file for more information.
There is no warranty; not even for merchantability or fitness for a particular purpose.
*****************************************************/

/* TODOS:  
	- brightness bargraph
	- tada sound in progmem
*/
#include "esp32_r4ge_pro.h"
#include <XPT2046_Touchscreen.h>
#include <SD.h> 
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "icons.h"	
#include "r4ge_pro_title.h"

void drawSD(bool present);

// Touch screen coordinates for the touch button
#define TOUCH_X1    3333
#define TOUCH_Y1    1000
#define TOUCH_X2    3700
#define TOUCH_Y2    1888

#define TOP_LINE      35
#define BOTTOM_LINE  180  

bool btn_pressed[8], btn_released[8];
bool btnA_pressed, btnB_pressed, btnX_pressed, btnY_pressed;
bool btnUp_pressed, btnDown_pressed, btnLeft_pressed, btnRight_pressed;
bool spkrLeft_on, spkrRight_on;
bool btnTouch_pressed, btnTouch_released;
int16_t joy_x_left, joy_y_left, joy_x_right, joy_y_right; 


uint8_t tft_led_bright = 64;  // PWM setting for TFT LED.  0 = full on, 255 = off

// i2s configuration
// See https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/include/driver/driver/i2s.h

int i2s_port_num = 0; 
i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
  .sample_rate = 11025,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,  // (i2s_bits_per_sample_t) 8
  .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  //I2S_CHANNEL_FMT_RIGHT_LEFT,
  .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),  // | I2S_COMM_FORMAT_PCM    
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,      // high interrupt priority. See esp_intr_alloc.h for more
  .dma_buf_count = 8,
  .dma_buf_len = 64,        
  .use_apll = false,        // I2S using APLL as main I2S clock, enable it to get accurate clock
  .tx_desc_auto_clear = 0,  // helps in avoiding noise in case of data unavailability
  .fixed_mclk = 0
};

i2s_pin_config_t pin_config = {
  .bck_io_num = I2S_BCLK,   // bit clock pin - to BCK pin on I2S DAC/PCM5102
  .ws_io_num = I2S_LRCK,    // left right select - to LCK pin on I2S DAC
  .data_out_num = I2S_DOUT, // DATA output pin - to DIN pin on I2S DAC
  .data_in_num = -1         // Not used
};

#define  SAMPLE_SIZE        11025
#define  BUFFER_SIZE          512 
#define  SAMPLES_PER_BUFFER   128  // 4 bytes per sample (16bit x 2 channels for stereo)
int16_t  audio_sample[SAMPLE_SIZE];
uint8_t  audio_buffer[BUFFER_SIZE];

XPT2046_Touchscreen ts(TCH_CS);
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);


bool initSD();
bool initAudioI2S();
void playAudio(bool play_right, bool play_left);
void drawSD(bool present);
void setTextColor(uint16_t color, bool inverted);
void updateScreen();
void checkButtonPresses();
void checkScreenTouch();
void checkJoysticks();
void checkMicrophone();

/*
 * Set up the board
 */
void setup() 
{
  Serial.begin(115200);
  Serial.println("ESP32 R4ge Pro Test"); 
  delay(100);

  // Set up the TFT backlight brightness control
  //  Keep Hz out of audible range.  Lower does lead to visible flickering
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

  ts.begin();
  ts.setRotation(TCHSCRN_ROT);
  tft.fillScreen(ILI9341_BLACK);

  tft.drawRGBBitmap(  0, 0, (uint16_t *)r4ge_pro_title, 175, 32);

  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.setCursor(205, 4);
  tft.println("TEST");
  tft.setTextSize(2);

  tft.drawLine(0, TOP_LINE, SCREEN_WD, TOP_LINE, ILI9341_BLUE);   // top line
  tft.drawLine(0, BOTTOM_LINE, SCREEN_WD, BOTTOM_LINE, ILI9341_BLUE); // bottom line

  tft.drawRect(250, 50, 70, 70, ILI9341_BLUE);  // touch button outline

  tft.drawRGBBitmap(0, 196, (uint16_t *)microphone_icon, 16, 16);
  tft.drawRGBBitmap(240, 220, (uint16_t *)brightness_icon, 16, 16);
  tft.drawRGBBitmap(0, 220, (uint16_t *)touch_icon, 16, 16);


  // Fill the audio buffer with a sine waveform for generating sound 
  double two_pi_scaled = PI * 2 / 8;
  for(int i = 0; i < SAMPLE_SIZE; i++) 
      audio_sample[i] = (8000 * sin((double)i * two_pi_scaled));

  bool sd_present = initSD();
  drawSD(sd_present);
}

/*
 * Initialize the SD file system before trying using a reader or writer
 */
bool initSD()
{
  // ESP32 requires 25 MHz SPI clock limit. Breakouts may require 10 MHz limit due to longer wires
  // See SDFat/src/SdCard/SdInfo.h for list of error codes
  bool sd_ok = false;
  File root;
  
  digitalWrite(TFT_CS, HIGH); // disable TFT
  digitalWrite(TCH_CS, HIGH); // disable Touchscreen - otherwise SD.begin() will fail!

  Serial.print(F("Initializing filesystem..."));
  if(SD.begin(SD_CS)) 
  { 
    root = SD.open("/");
    if (root) 
    {
      sd_ok = true;
    }
    else
    {
      tft.println(F("open root failed!"));
      Serial.println(F("open root failed!"));
    }
  }
  else
  { 
    //tft.println(F("SD begin() failed!"));
    Serial.println(F("SD begin() failed!")); 
  }
 
  return sd_ok;
}

/*
 * Initialize the I2S audio output
 */
bool initAudioI2S()
{
	esp_err_t err;
	
	err = i2s_driver_install((i2s_port_t)i2s_port_num, &i2s_config, 0, NULL);
	if (err != ESP_OK)
	{
		Serial.print("I2S driver install fail: ");
		Serial.println(err);
		return false;
	}	
	i2s_set_pin((i2s_port_t)i2s_port_num, &pin_config);
	i2s_set_clk((i2s_port_t)i2s_port_num, 11025, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
	
	return true;
}

/* 
 * Plays the audio sample through either or both speaker channels.  
 * Does not return until sample is done playing.
 */
void playAudio(bool play_right, bool play_left)
{
  bool      audio_playing;
  uint16_t  sample_pos = 0;
  int16_t   temp, temp_msb;
  size_t    bytes_out;
  
	if (!play_right && !play_left) return;  // not playing anything, so bail
	
	audio_playing = initAudioI2S();

	// Fill I2S transfer audio buffer from sample buffer
	while (audio_playing)
	{
		for (int i = 0; i < SAMPLES_PER_BUFFER; i++) 
		{
		  temp = (audio_sample[sample_pos + i]); 
		  temp_msb = temp >> 8;
		  
		  if (play_left) // put sound data into right channel (backwards?)
		  {
			audio_buffer[i * 4] = (uint8_t)temp & 0xff;
			audio_buffer[i * 4 + 1] = (uint8_t)temp_msb;
		  }
		  else
		  {
			audio_buffer[i * 4] = 0;
			audio_buffer[i * 4 + 1] = 0;
		  }

		  if (play_right) // put sound data into left channel (backwards?)
		  {
			audio_buffer[i * 4 + 2] = (uint8_t)temp & 0xff;
			audio_buffer[i * 4 + 3] = (uint8_t)temp_msb;
		  }
		  else
		  {
			audio_buffer[i * 4 + 2] = 0;  
			audio_buffer[i * 4 + 3] = 0;
		  }
		}

	  
    // Write data to I2S DMA buffer.  Blocking call, last parameter = ticks to wait or portMAX_DELAY for no timeout
    i2s_write((i2s_port_t)i2s_port_num, (const char *)&audio_buffer, sizeof(audio_buffer), &bytes_out, 100);
    if (bytes_out != sizeof(audio_buffer)) Serial.println("I2S write timeout");
  
    sample_pos += SAMPLES_PER_BUFFER;
    if (sample_pos >= SAMPLE_SIZE - 3)
    {
  	  audio_playing = false;
    }
	}
	// Stop audio playback
	i2s_driver_uninstall((i2s_port_t)i2s_port_num);
}


/*
 * Draws the SD card status icon indicating if the card is present or not.
 */
void drawSD(bool present)
{
  tft.drawRGBBitmap(300, 0, (uint16_t *)sd_icon, 18, 24);
  if (!present)
    tft.drawLine(300, 0, 318, 24, ILI9341_RED);  
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

  if (btn_pressed[BTN_UP])
  {
    tft_led_bright = (tft_led_bright > 10) ? tft_led_bright - 10 : 0;
    ledcWrite(TFT_LED_CHANNEL, tft_led_bright);
  }

  tft.setCursor(0, 70);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_LEFT]);
  tft.print("<-LT"); 

  tft.setCursor(60, 70);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_RIGHT]);
  tft.print("RT->");
 
  tft.setCursor(45, 90);
  setTextColor(ILI9341_CYAN, btn_pressed[BTN_DOWN]);
  tft.print("DN");

  if (btn_pressed[BTN_DOWN])
  {
    tft_led_bright = (tft_led_bright < 245) ? tft_led_bright + 10 : 255;
    ledcWrite(TFT_LED_CHANNEL, tft_led_bright);
  }

  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(264, 220);
  tft.print(tft_led_bright);
  tft.print(" "); 

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

  // Speakers
  tft.setCursor(180, 196);
  setTextColor(ILI9341_LIGHTGREY, btn_pressed[BTN_LEFT]);
  tft.print("((L))");

  tft.setCursor(250, 196);
  setTextColor(ILI9341_LIGHTGREY, btn_pressed[BTN_RIGHT]);
  tft.print("((R))");
  
  if (btnTouch_pressed)
  {
    tft.fillRect(251, 51, 68, 68, ILI9341_CYAN);
  }  
  else if (btnTouch_released)
  {
    tft.fillRect(251, 51, 68, 68, ILI9341_BLACK);
  }
    
  tft.setCursor(255, 70);
  setTextColor(ILI9341_CYAN, btnTouch_pressed); 
  tft.print("TOUCH");

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
 * Check for touches on the TFT screen and any corresponding on-screen buttons
 */
void checkScreenTouch()
{
  btnTouch_released = false;
  boolean is_touched = ts.touched();
  if (is_touched) 
  {
    TS_Point p = ts.getPoint();
    // Display the x,y touch coordinate for debugging/calibration
    tft.setCursor(24, 220);
    tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
    tft.print(p.x);
    tft.print(", ");
    tft.print(p.y);
    tft.print("   ");

    // Check if the touch is within the bounds of the "touch" button
    btnTouch_pressed = ((p.x > TOUCH_X1) && (p.x < TOUCH_X2) && (p.y > TOUCH_Y1) && (p.y < TOUCH_Y2)); 
  }
  else if (btnTouch_pressed)
  {
    btnTouch_released = true;
    btnTouch_pressed = false;
  }
  else
  {
    btnTouch_pressed = false;
  }
}

/*
 * Check the analog joystick values
 */
void checkJoysticks()
{
  // Only use the left 10 bits of the joystick pot readings
  // May want to use analogReadResolution() at some point to eliminate the need for shifts
  joy_x_left = analogRead(JOYX_L) >> 2;
  joy_y_left = analogRead(JOYY_L) >> 2;
  //jbtnL_pressed = 0; //(digitalRead(JBTN_L) == LOW ? 1: 0);
  
  joy_x_right = analogRead(JOYX_R) >> 2;
  joy_y_right = analogRead(JOYY_R) >> 2;
  //jbtnR_pressed = 0; //(digitalRead(JBTN_R) == LOW ? 1: 0);
}

/*
 * Reads from the microphone and displays the max amplitude
 * both as a bar graph and numeric value
 */
void checkMicrophone()
{
  static uint16_t max_amp = 0;
  static uint16_t mic_sample[4] = {0, 0, 0, 0};
  static uint16_t mic_idx = 0;
  uint16_t mic_color = ILI9341_GREEN;
  double percent;
  
  mic_sample[mic_idx] = abs(analogRead(MIC) - MIC_OFFSET);  // 12-bit (0 - 4095)
  percent = (double)mic_sample[mic_idx] / 2048; 
  
  if (mic_sample[mic_idx] > max_amp)
  {
    max_amp = mic_sample[mic_idx];
  }
  else
  {
    max_amp = (max_amp + mic_sample[0] + mic_sample[1] + mic_sample[2] + mic_sample[3]) / 5;
  }
  
  if (percent > 0.6)
  {
    mic_color = ILI9341_YELLOW;
  }
  if (percent > 0.8)
  {
    mic_color = ILI9341_RED;
  }
  tft.drawLine(0, BOTTOM_LINE, SCREEN_WD * percent, BOTTOM_LINE, mic_color);
  tft.drawLine(SCREEN_WD * percent, BOTTOM_LINE, SCREEN_WD - 1, BOTTOM_LINE, ILI9341_BLUE);

  tft.setCursor(24, 196);
  tft.setTextColor(mic_color, ILI9341_BLACK);
  //tft.print(mic_sample);
  tft.print(percent);
  tft.print("  ");
  
  mic_idx = (mic_idx + 1) % 4;
}

/*
 * Main program loop.  Called continuously after setup.
 */
void loop(void) 
{
  if (btnTouch_released)
  {
    // Testing - capture audio sample
    digitalWrite(ESP_LED, HIGH);
    audio_sample[0] = 0;
    
    for (uint16_t i = 1; i < SAMPLE_SIZE; i++)
    {
      audio_sample[i] = analogRead(MIC) - MIC_OFFSET;
      delayMicroseconds(80); // delay to approximate 11025 sample rate
    }
    audio_sample[SAMPLE_SIZE - 1] = 0;
    digitalWrite(ESP_LED, LOW);

    for (uint16_t i = 0; i < 300; i++)
       Serial.println(audio_sample[i]);
  }

  playAudio(btn_pressed[BTN_RIGHT], btn_pressed[BTN_LEFT]);

  checkButtonPresses();
  checkScreenTouch();
  checkJoysticks();
  checkMicrophone();
  //checkBattery();
  
  updateScreen();
  delay(2);
}