/***************************************************
ESP32 R4ge Pro Drummer Boy

Requires:
 - ESP32 R4ge Pro

Copyright (c) 2020 Paul Pagel
This is free software; see the license.txt file for more information.
There is no warranty; not even for merchantability or fitness for a particular purpose.
*****************************************************/

#include "esp32_r4ge_pro.h"
#include <XPT2046_Touchscreen.h>
#include <SD.h> 
#include <Fonts/FreeMonoBold12pt7b.h>
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "Kit01_chat_wav.h"
#include "Kit01_ohat_wav.h"
#include "Kit01_kick_wav.h"
#include "Kit01_snare_wav.h"
#include "Title.h"
#include "Icons.h"


#define PIXELS_PER_STEP 30
#define SEQ_LEN          8

const char* APP_FOLDER = "/DRUMMER/";


enum app_state_type {
  STATE_MENU,
  STATE_FREEPLAY,
  STATE_COMPOSE,
  STATE_PLAYBACK
};

enum app_state_type app_state, prev_app_state;

bool btn_pressed[8], btn_released[8];
bool btnA_pressed, btnB_pressed, btnX_pressed, btnY_pressed;
bool btnUp_pressed, btnDown_pressed, btnLeft_pressed, btnRight_pressed;
bool spkrLeft_on, spkrRight_on;
bool btnTouch_pressed, btnTouch_released;
int16_t joy_x_left, joy_y_left, joy_x_right, joy_y_right; 
bool sd_present = false;

uint32_t state_start_time, step_start_time;
uint8_t  seq_step, compose_seq_step, compose_row;

const uint8_t *audio_wav;
bool          audio_playing;
uint16_t      wav_length, sample_pos, tempo_ms;

uint8_t  sequence[] = { 1, 0, 2, 0, 1, 3, 2, 4 };
uint16_t seq_dark[] = { ILI9341_PURPLE, ILI9341_NAVY, ILI9341_MAROON, ILI9341_DARKGREEN };
uint16_t seq_light[] = { ILI9341_MAGENTA, ILI9341_BLUE, ILI9341_RED, ILI9341_GREEN };

// i2s configuration
// See https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/include/driver/driver/i2s.h

int i2s_port_num = 0; 
i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
  .sample_rate = 22050,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,  // (i2s_bits_per_sample_t) 8
  .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  //I2S_CHANNEL_FMT_ONLY_RIGHT, I2S_CHANNEL_FMT_RIGHT_LEFT
  .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),  // | I2S_COMM_FORMAT_PCM    
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,      // high interrupt priority. See esp_intr_alloc.h for more
  .dma_buf_count = 8,
  .dma_buf_len = 64,        
  .use_apll = true,        // I2S using APLL as main I2S clock, enable it to get accurate clock
  .tx_desc_auto_clear = 0,  // helps in avoiding noise in case of data unavailability
  .fixed_mclk = 0
};

i2s_pin_config_t pin_config = {
  .bck_io_num = I2S_BCLK,   // bit clock pin - to BCK pin on I2S DAC/PCM5102
  .ws_io_num = I2S_LRCK,    // left right select - to LCK pin on I2S DAC
  .data_out_num = I2S_DOUT, // DATA output pin - to DIN pin on I2S DAC
  .data_in_num = -1         // Not used
};

#define  BUFFER_SIZE          4096 
#define  SAMPLES_PER_BUFFER   2048  // 2 bytes per sample (16bit x 2 channels for stereo)
uint8_t  audio_buffer[BUFFER_SIZE];

File     root;
XPT2046_Touchscreen ts(TCH_CS);
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

/*
 * Set up the board
 */
void setup() 
{
  Serial.begin(115200);
  Serial.println("ESP32 R4ge Pro Drummer Boy"); 
  delay(100);

  // Set up shift register pins
  pinMode(SR_PL, OUTPUT);
  pinMode(SR_CP, OUTPUT);   
  pinMode(SR_Q7, INPUT);

  // Set up the joysticks
  pinMode(JOYX_L, INPUT);
  pinMode(JOYY_L, INPUT);
  pinMode(JOYX_R, INPUT);
  pinMode(JOYY_R, INPUT);

  // Set up the TFT backlight brightness control
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, LOW);

  delay(100);
  
  // Set up the TFT and touch screen
  tft.begin();
  tft.setRotation(SCREEN_ROT);

  ts.begin();
  ts.setRotation(TCHSCRN_ROT);

  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(&FreeMonoBold12pt7b);
  tft.setTextColor(ILI9341_WHITE);  

  tempo_ms = 250;

  drawMenu();
  app_state = STATE_MENU;
}

/*
 * Checks the SD card, initializes it if not already, and draws the status icon.
 */
bool checkSD()
{
  if (!sd_present)
  {
    sd_present = initSD();
    if (sd_present)
    {
      if (!SD.exists(APP_FOLDER))
      {
        SD.mkdir(APP_FOLDER);
      }
    }
  }
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
      //tft.println(F("open root failed!"));
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
void playAudio(const uint8_t *wav, uint16_t length)
{ 
  audio_playing = initAudioI2S();
  audio_wav = wav;
  wav_length = length;
  sample_pos = 44;  // skip RIFF header, could detect & skip(?)

  updateAudio();
}

void updateAudio()
{
  int16_t   buff_pos;
  uint8_t   temp, temp_msb;
  size_t    bytes_out;
  
  if (!audio_playing) return;

  // Fill I2S transfer audio buffer from sample buffer
  for (int i = 0; i < SAMPLES_PER_BUFFER - 1; i+= 2)
  {    
    if (sample_pos + i < wav_length - 1)
    {
      temp = audio_wav[sample_pos + i];  
      temp_msb = audio_wav[sample_pos + i + 1];  
    }
    else
    {
      temp = 0;
      temp_msb = 0;
    }

    buff_pos = i * 2; // right + left channel samples

    audio_buffer[buff_pos] = temp;
    audio_buffer[buff_pos + 1] = temp_msb;

    audio_buffer[buff_pos + 2] = temp;
    audio_buffer[buff_pos + 3] = temp_msb;
  }
  
  // Write data to I2S DMA buffer.  Blocking call, last parameter = ticks to wait or portMAX_DELAY for no timeout
  i2s_write((i2s_port_t)i2s_port_num, (const char *)&audio_buffer, sizeof(audio_buffer), &bytes_out, 100);
  if (bytes_out != sizeof(audio_buffer)) Serial.println("I2S write timeout");

  sample_pos += SAMPLES_PER_BUFFER;
  if (sample_pos >= wav_length - 1)
  {
    // Stop audio playback
    i2s_driver_uninstall((i2s_port_t)i2s_port_num);
    audio_playing = false;
  }
}

/*
 * Draws the app title/splash screen
 */
void drawMenu()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.drawRGBBitmap(0, 0, (uint16_t *)drummer_title, 214, 26);
  checkSD();
   
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_DARKGREY);
  tft.setCursor(80, 180);
  tft.print("(X) Free Play");
  tft.setCursor(80, 200);
  tft.print("(Y) Compose");
}

/*
 * Draws the app title/splash screen
 */
void drawCompose()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.drawRGBBitmap(0, 0, (uint16_t *)drummer_title, 214, 26);
  checkSD();
  
  tft.drawRGBBitmap(20,  64, (uint16_t *)ohat_icon, 32, 28);
  tft.drawRGBBitmap(20,  96, (uint16_t *)chat_icon, 32, 28);
  tft.drawRGBBitmap(20, 128, (uint16_t *)snare_icon, 32, 30);
  tft.drawRGBBitmap(20, 160, (uint16_t *)kick_icon, 32, 32);

  drawSequencer();
  drawTempo();

  tft.fillRect(70 + compose_seq_step * PIXELS_PER_STEP, 92 + compose_row * 32, 20, 4, ILI9341_YELLOW); 
}

/*
 * Draw the step sequencer grid
 */
void drawSequencer()
{
  for (int i = 0; i < 4; i++)   // each instrument track
  {
    // Draw free play instrument ON indicator
    tft.drawRect(0, 65 + i * 32, 8, 30, seq_light[i]); 
    
    for (int c = 0; c < 8; c++) // each sequencer step column
    {
      tft.drawCircle(80 + c * 30, 80 + i * 32, 8, seq_dark[i]);

      if (sequence[c] == 4 - i) 
      {
        // Draw active note
        tft.fillCircle(80 + c * PIXELS_PER_STEP, 80 + i * 32, 7, seq_light[i]);   
      }
      else
      {
        // Erase note area
        tft.fillCircle(80 + c * PIXELS_PER_STEP, 80 + i * 32, 7, ILI9341_BLACK);  
      }
    }
  }
}

/*
 * Draws the SD card status icon indicating if the card is present or not.
 */
void drawSD(bool present)
{
  uint16_t color = (present ? ILI9341_WHITE : ILI9341_DARKGREY);
  tft.drawRGBBitmap(294, 2, (uint16_t *)sd_icon, 18, 24);
  if (!present)
    tft.drawLine(294, 0, 314, 26, ILI9341_RED);  

  tft.drawRect(284, 0, 35, 28, color); // button outline
}

/*
 * Display the tempo value
 */
void drawTempo()
{
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  tft.setCursor(0, 226);
  tft.print("Tempo: ");
  tft.fillRect(100, 210, 40, 20, ILI9341_BLACK);
  tft.print(tempo_ms);
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
  // Joystick result value will be between -15 and +15
  joy_x_left = (analogRead(JOYX_L) >> 7) - JOY_5BIT_CTR;
  joy_y_left = (analogRead(JOYY_L) >> 7) - JOY_5BIT_CTR;

  joy_x_right = (analogRead(JOYX_R) >> 7) - JOY_5BIT_CTR;
  joy_y_right = (analogRead(JOYY_R) >> 7) - JOY_5BIT_CTR;
}

/*
 * Checks for any tempo adjustments and updates the display
 */
void checkTempo()
{
  if (joy_y_left > 3)
  {
    tempo_ms += 1;
    drawTempo();
  }
  if (joy_y_left < -3)
  {
    tempo_ms -= 1;
    drawTempo();
  }
}

/*
 * Main program loop.  Called continuously after setup.
 */
void loop(void) 
{
  checkButtonPresses();
  checkJoysticks();
    
  if (prev_app_state != app_state)
  {
    state_start_time = millis();  // track when the state change started
  
    // Do initial screen drawing for new game state
    switch(app_state)
    {
      case STATE_MENU:
        Serial.println("New state: MENU");
        drawMenu();
        break;
      case STATE_FREEPLAY:
        Serial.println("New state: FREEPLAY");
        drawCompose();
        break;
      case STATE_COMPOSE:
        Serial.println("New state: COMPOSE");
        drawCompose();
        break;
      case STATE_PLAYBACK:
        Serial.println("New state: PLAYBACK");
        step_start_time = 0;
        break;
    }
  }
  prev_app_state = app_state;
  
  // Update the screen based on the app state
  switch(app_state)
  {
    case STATE_MENU:
      handleMenu();
      break;
    case STATE_FREEPLAY:
      handleFreePlay();
      break;
    case STATE_COMPOSE:
      handleCompose();
      break;
    case STATE_PLAYBACK:
      handlePlayback(); 
      break;
  }

  updateAudio();
  //delay(1);
}

/*
 * Handles the STATE_TITLE game state logic 
 */
void handleTitle()
{
  if (btn_released[BTN_X])
  {
    app_state = STATE_FREEPLAY;
    return;
  }

  if (btn_released[BTN_Y])
  {
    app_state = STATE_MENU;
    return;
  }
}

/*
 * Handles the STATE_MENU game state logic
 */
void handleMenu()
{
  if (btn_released[BTN_X])
  {
    app_state = STATE_FREEPLAY;
    return;
  }

  if (btn_released[BTN_Y])
  {
    app_state = STATE_COMPOSE;
    return;
  }
  // TODO:
}

/*
 * Handles the STATE_PLAY free sample play state logic
 */
void handleFreePlay()
{
  if (btn_released[BTN_Y])
  {
    app_state = STATE_MENU;
    return;
  }

  if (btn_pressed[BTN_LEFT])
  {
    tft.fillRect(1, 162, 6, 28, seq_light[3]);
    playAudio(Kit01_kick_wav, 12056);
    tft.fillRect(1, 162, 6, 28, ILI9341_BLACK);
    return;
  }

  if (btn_pressed[BTN_RIGHT])
  {
    tft.fillRect(1, 130, 6, 28, seq_light[2]);
    playAudio(Kit01_snare_wav, 11316);
    tft.fillRect(1, 130, 6, 28, ILI9341_BLACK);
    return;
  }

  if (btn_pressed[BTN_DOWN])
  {
    tft.fillRect(1, 98, 6, 28, seq_light[1]);
    playAudio(Kit01_chat_wav, 11026);
    tft.fillRect(1, 98, 6, 28, ILI9341_BLACK);
    return;
  }

  if (btn_pressed[BTN_UP])
  {
    tft.fillRect(1, 66, 6, 28, seq_light[0]);
    playAudio(Kit01_ohat_wav, 33474);
    tft.fillRect(1, 66, 6, 28, ILI9341_BLACK);
    return;
  }

}

/*
 * Handles the STATE_COMPOSE app state logic
 */
void handleCompose()
{
  checkTempo();
  
  if (btn_released[BTN_X])
  {
    tft.fillRect(70 + compose_seq_step * PIXELS_PER_STEP, 92 + compose_row * 32, 20, 4, ILI9341_BLACK); 
    app_state = STATE_PLAYBACK;
    return;
  }
  
  if (btn_released[BTN_Y])
  {
   app_state = STATE_MENU;
   return;
  }

  if (btn_released[BTN_LEFT])
  {
    tft.fillRect(70 + compose_seq_step * PIXELS_PER_STEP, 92 + compose_row * 32, 20, 4, ILI9341_BLACK); 
    compose_seq_step = (compose_seq_step > 0 ? compose_seq_step - 1 : SEQ_LEN - 1);
    tft.fillRect(70 + compose_seq_step * PIXELS_PER_STEP, 92 + compose_row * 32, 20, 4, ILI9341_YELLOW); 
    return;
  }

  if (btn_released[BTN_RIGHT])
  {
    tft.fillRect(70 + compose_seq_step * PIXELS_PER_STEP, 92 + compose_row * 32, 20, 4, ILI9341_BLACK); 
    compose_seq_step = (compose_seq_step < SEQ_LEN - 1 ? compose_seq_step + 1 : 0);
    tft.fillRect(70 + compose_seq_step * PIXELS_PER_STEP, 92 + compose_row * 32, 20, 4, ILI9341_YELLOW); 
    return;
  }

  if (btn_released[BTN_UP])
  {
    tft.fillRect(70 + compose_seq_step * PIXELS_PER_STEP, 92 + compose_row * 32, 20, 4, ILI9341_BLACK); 
    compose_row = (compose_row > 0 ? compose_row - 1 : 3);
    tft.fillRect(70 + compose_seq_step * PIXELS_PER_STEP, 92 + compose_row * 32, 20, 4, ILI9341_YELLOW); 
    return;
  }

  if (btn_released[BTN_DOWN])
  {
    tft.fillRect(70 + compose_seq_step * PIXELS_PER_STEP, 92 + compose_row * 32, 20, 4, ILI9341_BLACK); 
    compose_row = (compose_row < 3 ? compose_row + 1 : 0);
    tft.fillRect(70 + compose_seq_step * PIXELS_PER_STEP, 92 + compose_row * 32, 20, 4, ILI9341_YELLOW); 
    return;
  }

  if (btn_released[BTN_A])
  {
    sequence[compose_seq_step] = 4 - compose_row;
    drawSequencer();
    return;
  }
  
  if (btn_released[BTN_B])
  {
    sequence[compose_seq_step] = 0;
    drawSequencer();
    return;
  }
}

/*
 * Handles the STATE_PLAYBACK app state logic
 */
void handlePlayback()
{
  if (btn_released[BTN_Y])
  {
    app_state = STATE_COMPOSE;
    return;
  }

  checkTempo();

  tft.fillRect(70 + seq_step * PIXELS_PER_STEP, 192, 20, 4, ILI9341_BLACK); 
  
  bool play = false;

  if (step_start_time == 0)
  {
    seq_step = 0;
    step_start_time = millis();
    play = true;
  }

  if (millis() - step_start_time > tempo_ms)
  {
    seq_step = (seq_step == 7 ? 0 : seq_step + 1);
    step_start_time = millis();
    play = true;
  }

  if (play)
  {
    tft.fillRect(70 + seq_step * PIXELS_PER_STEP, 192, 20, 4, ILI9341_YELLOW); 
    
    switch(sequence[seq_step])
    {
      case 1:  // kick
        playAudio(Kit01_kick_wav, 12056);
        break;
      case 2:  // snare
        playAudio(Kit01_snare_wav, 11316);
        break;
      case 3:  // closed hat
        playAudio(Kit01_chat_wav, 11026);
        break;
      case 4:  // open hat
        playAudio(Kit01_ohat_wav, 33474);
        break;
      default:
        break; 
    }
  }
  
}
