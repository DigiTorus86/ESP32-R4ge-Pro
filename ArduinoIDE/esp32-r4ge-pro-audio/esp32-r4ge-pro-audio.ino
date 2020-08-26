/***************************************************
ESP32 R4ge Pro Audio application

Requires:
 - ESP32 R4ge Pro
 - SD card

Copyright (c) 2020 Paul Pagel
This is free software; see the license.txt file for more information.
There is no warranty; not even for merchantability or fitness for a particular purpose.
*****************************************************/

#include "esp32_r4ge_pro.h"
#include <XPT2046_Touchscreen.h>
#include <SD.h> 
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "WAV_audio.h"
#include "WAV_file.h"
#include "WAV_frame.h"
#include "DFT.h"
#include "sd_icon.h"

#define TOP_LINE     30
#define WAVFORM_CTR  80  // 130 for 8 bit?
#define WAVFORM_HT  100
#define MID_LINE    135
#define ANALYZE_Y   200 
#define ANALYZE_HT   60
#define BOTTOM_LINE 205

// Touch screen coordinates for the SD touch button
#define SD_TOUCH_X1 3300
#define SD_TOUCH_Y1    0
#define SD_TOUCH_X2 3800
#define SD_TOUCH_Y2  500

#define SEL_BTN_CNT 20
#define SEL_BTN_WD  60
#define SEL_BTN_HT  40

#define SEL_BTN_SCRN_X(btn) (((btn) - 1) % 5 * (SEL_BTN_WD + 4))
#define SEL_BTN_SCRN_Y(btn) (TOP_LINE + 4 + (((btn) - 1) / 5) * (SEL_BTN_HT + 4))

enum app_mode_type 
{
  MODE_WAV_DISPLAY,
  MODE_WAV_SELECT_LOAD,
  MODE_WAV_SELECT_SAVE,
  MODE_WAV_ANALYZE
};
enum app_mode_type app_mode, prev_app_mode;

const char* APP_FOLDER = "/AUDIO/MIC/";
const char* WAV_NAME   = "MIC000";
char wav_filename[]    = "/AUDIO/MIC/MIC000.WAV";      
uint16_t    mic_index  = 1;

bool btn_pressed[8], btn_released[8], btnSD_pressed, btnSD_released;
bool spkrLeft_on, spkrRight_on, led1_on, led2_on, led3_on;
bool sd_present = false;

uint8_t tft_led_bright = 64;  // 0 = full on, 255 = off

// i2s configuration
// See https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/include/driver/driver/i2s.h

int i2s_port_num = 0; 
i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
  .sample_rate = 11025,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,  // (i2s_bits_per_sample_t) 8
  .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,  
  .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),  // | I2S_COMM_FORMAT_PCM    
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,      // high interrupt priority. See esp_intr_alloc.h for more
  .dma_buf_count = 8,
  .dma_buf_len = 64,        
  .use_apll = false,        // I2S using APLL as main I2S clock, enable it to get accurate clock
  .tx_desc_auto_clear = 0,  // helps in avoiding noise in case of data unavailability
  .fixed_mclk = 0
};

i2s_pin_config_t pin_config = {
  .bck_io_num = I2S_BCLK, //this is BCK pin
  .ws_io_num = I2S_LRCK, // this is LCK pin
  .data_out_num = I2S_DOUT, // this is DATA output pin (DIN on PCM5102)
  .data_in_num = -1   //Not used
};

WAV_audio wav_audio;
WAV_frame wav_frame;
WAV_file  wav_file;

#define   BUFFER_SIZE          512 
#define   SAMPLES_PER_BUFFER   256   // 2 bytes per sample
uint8_t   audio_buffer[BUFFER_SIZE];

File     root;
XPT2046_Touchscreen ts(TCH_CS);
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

/*
 * Set up the board
 */
void setup() 
{
  Serial.begin(115200);
  Serial.println("ESP32 Badge Pro Audio I2S"); 
  delay(100);
  
  // Set up shift register pins
  pinMode(SR_PL, OUTPUT);
  pinMode(SR_CP, OUTPUT);   
  pinMode(SR_Q7, INPUT);

  // Set up the SPI CS output pins
  pinMode(TFT_CS, OUTPUT);
  pinMode(TCH_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);

  // Set up the TFT backlight brightness control
  //ledcSetup(TFT_LED_CHANNEL, 100, 8);  // 100 Hz max, 8 bit resolution.  Higher = tone on speaker
  //ledcAttachPin(TFT_LED, TFT_LED_CHANNEL);
  //ledcWrite(TFT_LED_CHANNEL, tft_led_bright);
  digitalWrite(TFT_LED, HIGH); // using PWM causes noise with microphone
  
  delay(100);
  
  // Set up the TFT and touch screen
  tft.begin();
  tft.setRotation(SCREEN_ROT);

  ts.begin();
  ts.setRotation(TCHSCRN_ROT);

  sd_present = initSD();
  if (sd_present)
  {
    if (!SD.exists(APP_FOLDER))
    {
      SD.mkdir(APP_FOLDER);
    }
  }

  // Set up the initial test waveform
  //wav_audio.fillWaveform(440, 0);  // 440Hz = A4

  double two_pi_scaled = 2 * PI / 16;
  int16_t sample;
  for(int i = 0; i < WAV_MAX_SAMPLES; i++) 
  {
    sample = (int16_t)(30000 * sin((double)i * two_pi_scaled));
    wav_audio.setSample(i, 0, sample);
    //if (i < 400) Serial.println(sample);
  }
  
  beginWavDisplay();
  drawSD(sd_present);
  
  Serial.println(F("Setup complete."));
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
      tft.println(F("open root failed!"));
      Serial.println(F("open root failed!"));
    }
  }
  else
  { 
    tft.println(F("SD begin() failed!"));
    Serial.println(F("SD begin() failed!")); 
  }
 
  return sd_ok;
}

void setFilename(uint16_t index)
{
	wav_filename[14] = char(48 + (index % 1000) / 100);
	wav_filename[15] = char(48 + (index % 100) / 10);
	wav_filename[16] = char(48 + (index % 10));
}

bool loadWavFile(uint16_t index)
{
	setFilename(index);
	return wav_file.loadFile(wav_filename, &wav_audio);
}

bool saveWavFile(uint16_t index)
{
	setFilename(index);
	return wav_file.saveFile(wav_filename, &wav_audio);
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
	i2s_set_clk((i2s_port_t)i2s_port_num, 11025, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
	
	return true;
}

/* 
 * Plays the audio sample.  
 * Does not return until sample is done playing.
 */
void playAudio()
{
  bool     audio_playing = false;
  uint16_t sample_pos = 0;
  int16_t  temp;
  size_t   bytes_out;
    
  audio_playing = initAudioI2S();
  
  // Fill I2S transfer audio buffer from sample buffer
  while (audio_playing)
  {
    for (int i = 0; i < SAMPLES_PER_BUFFER - 1; i++)
    {
      temp = wav_audio.getSample(sample_pos + i, 0);
      audio_buffer[i * 2] = (uint8_t)temp & 0xff;
      temp = temp >> 8;
      audio_buffer[i * 2 + 1] = (uint8_t)temp;
    }
    
    // Write data to I2S DMA buffer.  Blocking call, last parameter = ticks to wait or portMAX_DELAY for no timeout
    i2s_write((i2s_port_t)i2s_port_num, (const char *)&audio_buffer, sizeof(audio_buffer), &bytes_out, 100);
    if (bytes_out != sizeof(audio_buffer)) Serial.println("I2S write timeout");

    sample_pos += SAMPLES_PER_BUFFER;
    if (sample_pos >= WAV_MAX_SAMPLES - 1) audio_playing = false;
  }
  // Stop audio playback
  i2s_driver_uninstall((i2s_port_t)i2s_port_num);
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
 * Draw the WAV time domain amplitude waveform to the screen.
 */
void drawWaveform()
{
    double scale_ht = (double)WAVFORM_HT / (wav_audio.header.bits_per_sample == 8 ? 256.0 : 65536.0);
    uint16_t skip = 1;
    
    tft.fillRect(0, TOP_LINE + 1, SCREEN_WD, WAVFORM_HT, ILI9341_BLACK);
    
    for (uint16_t i = 0; i < SCREEN_WD - 1; i ++)
    {
      tft.drawLine(i, WAVFORM_CTR - wav_audio.getNormalizedSample(i * skip, 0) * scale_ht, i + 1, WAVFORM_CTR - wav_audio.getNormalizedSample((i + 1) * skip, 0) * scale_ht, ILI9341_GREEN); 

      if (wav_audio.header.num_channels > 1)
      {
        tft.drawLine(i, WAVFORM_CTR - wav_audio.getNormalizedSample(i * 32, 1) * scale_ht, i + 1, WAVFORM_CTR - wav_audio.getNormalizedSample((i + 1) * 32, 1) * scale_ht, ILI9341_BLUE); 
      }
    }
}

/*
 * Draw the DFT frequency domain amplitude waveform to the screen.
 */
void drawDFT()
{
    double   scale_ht = (double)ANALYZE_HT / (wav_frame.findMaxPowerOutput() + 1);
    double   deltaF   = wav_audio.header.sample_rate / WAV_FRAME_SIZE;
    uint16_t max_idx  = 2;
    
    tft.fillRect(0, ANALYZE_Y - ANALYZE_HT, SCREEN_WD, ANALYZE_HT, ILI9341_BLACK);

    // Skip first bin (DC offset)
    for (uint16_t i = 2; i < WAV_FRAME_SIZE / 2; i ++)
    {
      //tft.drawLine(i, ANALYZE_Y - (int)wav_frame.dft_output[i].real() * scale_ht, i + 1, ANALYZE_Y - (int)wav_frame.dft_output[i + 1].real(), ILI9341_BLUE); 
      tft.drawLine(i, ANALYZE_Y - (int)wav_frame.power_spectrum[i]  * scale_ht, i + 1, ANALYZE_Y - (int)wav_frame.power_spectrum[i + 1] * scale_ht, ILI9341_YELLOW); 

      Serial.println(wav_frame.power_spectrum[i]);
      
      if (wav_frame.power_spectrum[i] > wav_frame.power_spectrum[max_idx])
        max_idx = i;
    }

    tft.setTextSize(2);
    tft.setCursor(0, 220);
    tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
    tft.print("Primary Freq: ");
    tft.print((int)(max_idx * deltaF));
    tft.print(" Hz  ");
}

/*
 * Draws the specified cell, either as selected/highlighted, or normal.
 */
void drawCell(uint8_t cell, bool selected)
{
  if (cell == 0 || cell > 20)
    return;

  uint16_t color = selected ? ILI9341_WHITE : ILI9341_ORANGE;
  uint16_t x = SEL_BTN_SCRN_X(cell);
  uint16_t y = SEL_BTN_SCRN_Y(cell);
  
  tft.fillRect(x, y, SEL_BTN_WD, SEL_BTN_HT, color);

  tft.setCursor(x + 14, y + 10);
  tft.setTextColor(ILI9341_BLACK, color);
  tft.print(cell);
}

/*
 * Intial drawing and setup of the WAV display screen
 */
void beginWavDisplay()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(2);

  tft.setCursor(0, 4);
  tft.println("ESP32 Badge Pro Audio");

  tft.drawLine(0, TOP_LINE, 319, TOP_LINE, ILI9341_BLUE);   
  tft.drawLine(0, MID_LINE, 319, MID_LINE, ILI9341_BLUE);  
  tft.drawLine(0, BOTTOM_LINE, 319, BOTTOM_LINE, ILI9341_BLUE);  
  
  drawSD(sd_present);
  drawWaveform();
}

/*
 * Inital drawing and setup of the WAV select screen
 */
void beginWavSelect()
{
  tft.fillRect(0, TOP_LINE + 1, 320, 240 - TOP_LINE, ILI9341_BLACK);

  for(int btn = 1; btn <= SEL_BTN_CNT; btn++)
  {
    drawCell(btn, false);
  }
}

/*
 * Main screen update logic.  Call on each loop.
 */
void updateScreen()
{
  if (prev_app_mode != app_mode)
  {
    // Do initial screen drawing for new mode
    switch(app_mode)
    {
      case MODE_WAV_DISPLAY:
	    beginWavDisplay();
		break;
      case MODE_WAV_SELECT_LOAD:
		beginWavSelect();
		break;
	  case MODE_WAV_SELECT_SAVE:
		beginWavSelect();
		break;
      case MODE_WAV_ANALYZE:
		break;
      default:
		break;
    }
  }
  prev_app_mode = app_mode;
  
  // Update the screen based on the app mode
  switch(app_mode)
  {
    case MODE_WAV_DISPLAY:
    break;
    case MODE_WAV_SELECT_LOAD:
    break;
	case MODE_WAV_SELECT_SAVE:
    break;
    case MODE_WAV_ANALYZE:
    break;
    default:
    break;
  }
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
    btn_pressed[i] = (digitalRead(SR_Q7) == LOW ? 1: 0);// read the state of the SO:
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
 * Checks to see which if any WAV select cells are being touched.
 * Returns cell index (1-20) of cell being touched.  
 * Returns zero if no cell is being touched.
 */
uint8_t checkCellTouch(int16_t tx, int16_t ty)
{
  uint8_t cell = 0;

  int16_t row = 1;
  int16_t col = 0;
  
  for (uint8_t btn = 0; btn < 20; btn++)
  {
    if (tx > 400 + col * 700 && tx < 400 + (col + 1) * 700 && ty > row * 700 && ty < (row + 1) * 700)
    {
      cell = btn + 1;
      break;
    }

    col += 1;
    if (col >= 5)
    {
      row += 1;
      col = 0;
    }
  }

  return cell;
}

/*
 * Checks to see if the screen is being touched on any active buttons or cells.
 * Returns true if the screen is being touched, otherwise false.
 */
bool checkScreenTouch(bool debug)
{
  // Check for screen touches
  static uint8_t prev_touched_cell = 0;
  uint8_t touched_cell = 0;  
  
  btnSD_released = false;
  
  bool is_touched = ts.touched();
  if (is_touched) 
  {
    TS_Point p = ts.getPoint();
    if (debug)
    {
      // Display the x,y touch coordinate for debugging/calibration
      tft.setCursor(0, 220);
      tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
      tft.print(p.x);
      tft.print(", ");
      tft.print(p.y);
      tft.print("   ");
    }
    
    // Check if the touch is within the bounds of the "touch" button
    btnSD_pressed = ((p.x > SD_TOUCH_X1) && (p.x < SD_TOUCH_X2) && (p.y > SD_TOUCH_Y1) && (p.y < SD_TOUCH_Y2)); 

    if (app_mode == MODE_WAV_SELECT_LOAD || app_mode == MODE_WAV_SELECT_SAVE)
    {
      touched_cell = checkCellTouch(p.x, p.y);
      drawCell(touched_cell, true);
      if (touched_cell != prev_touched_cell)
        drawCell(prev_touched_cell, false);
      prev_touched_cell = touched_cell;
    }
  }
  else if (btnSD_pressed)
  {
    btnSD_released = true;
    btnSD_pressed = false;
  }
  else
  {
    btnSD_pressed = false;
  }
  
  if (touched_cell > 0 && app_mode == MODE_WAV_SELECT_LOAD)
  {
	  loadWavFile(touched_cell);
	  app_mode = MODE_WAV_DISPLAY;
  }
  
  if (touched_cell > 0 && app_mode == MODE_WAV_SELECT_SAVE)
  {
	  saveWavFile(touched_cell);
	  app_mode = MODE_WAV_DISPLAY;
  }
  
  return is_touched;
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
  
  mic_sample[mic_idx] = abs(analogRead(MIC) - 1800);  // 12-bit (0 - 4095)
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
  tft.drawLine(0, TOP_LINE, SCREEN_WD * percent, TOP_LINE, mic_color);
  tft.drawLine(SCREEN_WD * percent, TOP_LINE, SCREEN_WD - 1, TOP_LINE, ILI9341_BLUE);

  tft.setCursor(256, 220);
  tft.setTextColor(mic_color, ILI9341_BLACK);
  //tft.print(mic_sample);
  //tft.print("  ");
  tft.print(percent);
  
  
  mic_idx = (mic_idx + 1) % 4;
}

/*
 * Main program loop.  Called continuously after setup.
 */
void loop(void) 
{
  if (btn_released[BTN_A])  // SAVE temp wav
  {
    if (!saveWavFile(0))
    {
      // unable to save WAV file
      //digitalWrite(LED_1, HIGH);
    }
  }

  if (btn_released[BTN_X])  // LOAD temp wav
  {
    if (loadWavFile(0))
    {
      drawWaveform();
    }
    else
    {
      // unable to load WAV file
    }
  }
  
  if (btn_released[BTN_B])  // RECORD
  {
    delay(100);
    wav_audio.setSample(0, 0, wav_audio.silence()); 
    for (uint16_t i = 1; i < WAV_MAX_SAMPLES; i++)
    {
      int16_t sample = analogRead(MIC) - MIC_OFFSET;
      wav_audio.setSample(i, WAV_CHNL_LEFT, sample);
      delayMicroseconds(80); // delay to approximate 11025 sample rate
    }
    wav_audio.setSample(WAV_MAX_SAMPLES - 1, WAV_CHNL_LEFT, wav_audio.silence());  
    wav_audio.header.data_bytes = WAV_MAX_SAMPLES;
    wav_audio.header.wav_size = WAV_MAX_SAMPLES + sizeof(wav_audio.header) - 8;
    
    drawWaveform();

    delay(200);
  }

  if (btn_released[BTN_LEFT]) // ANALYZE
  {
    
    tft.setTextSize(2);
    tft.setCursor(32, ANALYZE_Y - 40);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.print("Analyzing...");
    
    wav_frame.calcDFT(&wav_audio);
    drawDFT();
    
  }
  
  if (btn_released[BTN_RIGHT])  // Play WAV
  {
    playAudio();
  }
  
  checkButtonPresses();
  checkScreenTouch(true);
  checkMicrophone();

  if (btnSD_released)
  {
    app_mode = (app_mode != MODE_WAV_SELECT_LOAD ? MODE_WAV_SELECT_LOAD : MODE_WAV_DISPLAY);
  }

  updateScreen();  
  delay(10);
}
