/***************************************************
ESP32 R4ge Pro Wave Studio application

Requires:
 - ESP32 R4ge Pro
 - SD card

 Controls:
 - Left/Right: change selected control
 - Up/Down:    change value of selected control
 - 
 - A: play selected note    
 - B: next screen    
 - X: prev screen
 - Y: start/stop sequencer playback (pending)    

Copyright (c) 2020 Paul Pagel
This is free software; see the license.txt file for more information.
There is no warranty; not even for merchantability or fitness for a particular purpose.
*****************************************************/

#include <Arduino.h>
#include "esp32_r4ge_pro.h" 
#include <XPT2046_Touchscreen.h>
#include <SD.h> 
#include <MIDI.h>
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "SYN_common.h"
#include "SYN_engine.h"
#include "SYN_midi.h"
#include "TFT_group_op12.h"
#include "TFT_group_op34.h"
#include "TFT_group_fltr.h"
#include "TFT_group_seq.h"
#include "TFT_keyboard.h"
#include "TFT_sd_grid.h"
#include "TFT_select_wave.h"
#include "TFT_slider.h"
#include "sd_icon.h"
#include "r4ge_pro_title.h"
#include "synth_title.h"

#define TOP_LINE     32
#define MID_LINE    118
#define BOTTOM_LINE 203

#define SD_TOUCH_X1 284
#define SD_TOUCH_Y1   0 
#define SD_TOUCH_X2 319
#define SD_TOUCH_Y2  30

enum app_mode_type 
{
  MODE_OP12,
  MODE_OP34,
  MODE_FLTR,
  MODE_STEP_SEQ,
  MODE_SELECT_SD,
};
enum app_mode_type app_mode, prev_app_mode;

const char* APP_FOLDER = "/SYNTH/";
char cfg_filename[]    = "/SYNTH/SYN000.CFG";      
uint16_t    mic_index  = 1;

bool btn_was_pressed[8], btn_pressed[8], btn_released[8];
bool btnSD_pressed, btnSD_released;
bool spkrLeft_on, spkrRight_on, led1_on, led2_on, led3_on;
bool sd_present = false;
int16_t touch_x, touch_y;
int16_t joy_x_left, joy_y_left, joy_x_right, joy_y_right; 

uint8_t tft_led_bright = 64;  // 0 = full on, 255 = off

File     root;
XPT2046_Touchscreen ts(TCH_CS);
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, midi_in);

// Display controls
TFT_group_op12 op12_grp = TFT_group_op12();
TFT_group_op34 op34_grp = TFT_group_op34();
TFT_group_fltr fltr_grp = TFT_group_fltr();
TFT_group_seq   seq_grp = TFT_group_seq();
TFT_keyboard keybrd = TFT_keyboard(0, 204, "OCT1");
TFT_sd_grid sd_grid = TFT_sd_grid(0, 36, "SD");

SYN_engine syn_eng = SYN_engine();
SYN_op_config_t       op_cfg;
SYN_filter_config_t   fltr_cfg;
SYN_global_config_t   global_cfg = { .route = SYN_ROUTE_1234};
SYN_sequence_config_t seq_cfg;
//SYN_midi midi = SYN_midi();

//SYN_played_note_type played_note[SYN_MAX_VOICES];
//uint8_t note_idx = 0;
uint8_t note_num = 69;  // A4, 440 Hz
uint8_t prev_note_num = 69;
float pitch_bend =  1.0; // No bend
float mod_level  =  1.0; // No mod change

bool     play_seq = false;
uint64_t seq_note_start = 0; // time last sequence note played
uint8_t  seq_idx;

void  blinkLED(uint8_t count);
void  playStartupSound();
void  playNote(uint8_t note_num, uint8_t velocity, uint32_t duration_ms);
void updateWait(uint32_t duration_ms);
bool  initSD();
void  handleSelectionSD();
void  setFilename(uint16_t index);
bool  loadConfigFile(uint16_t index);
bool  saveConfigFile(uint16_t index);
bool  initAudioI2S();
void  playAudio();
void  drawSD(bool present);
void  drawCell(uint8_t cell, bool selected);
void  beginDisplayOp12();
void  beginDisplayOp34();
void  beginDisplayFltr();
void  beginDisplayStepSeq();
void  beginWavSelect();
void  updateScreen();
void  checkButtonPresses();
void  checkJoysticks();
bool  checkScreenTouch(bool debug);

/**
 * @brief MIDI callback for NoteOn messages.
 * 
 * @param channel  MIDI Channel (0-15)
 * @param pitch    MIDI Note Number 
 * @param velocity (0-127)
 */
void handleNoteOn(byte channel, byte pitch, byte velocity)
{
  if (velocity > 0)
  {
    syn_eng.noteOn(channel, pitch, velocity);
  }
  else
  {
    // NoteOn messages with 0 velocity are interpreted as NoteOffs
    syn_eng.noteOff(channel, pitch);
  }
  
  digitalWrite(ESP_LED, HIGH);
  //Serial.print("NoteOn pitch: "); Serial.println(pitch);
}

/**
 * @brief MIDI callback for NoteOff messages.
 * 
 * @param channel 
 * @param pitch 
 * @param velocity 
 */
void handleNoteOff(byte channel, byte pitch, byte velocity)
{
  syn_eng.noteOff(channel, pitch);
  digitalWrite(ESP_LED, HIGH);
  //Serial.print("NoteOff pitch: "); Serial.println(pitch);
}

/*
 * Set up the board
 */
void setup() 
{
  Serial.begin(9600);
  Serial.println("ESP32 R4ge Pro Synth"); 
  delay(100);

  pinMode(ESP_LED, OUTPUT);  // built-in blue LED on NodeMCU

  // Set up shift register pins
  pinMode(SR_PL, OUTPUT);
  pinMode(SR_CP, OUTPUT);   
  pinMode(SR_Q7, INPUT);

  // Set up the SPI CS output pins
  pinMode(TFT_CS, OUTPUT);
  pinMode(TCH_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);

  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, LOW); // full brightness
  
  // Set up the joysticks
  pinMode(JOYX_L, INPUT);
  pinMode(JOYY_L, INPUT);
  pinMode(JOYX_R, INPUT);
  pinMode(JOYY_R, INPUT);

  delay(100);
  
  // Set up the TFT and touch screen
  tft.begin();
  tft.setRotation(SCREEN_ROT);

  ts.begin();
  ts.setRotation(TCHSCRN_ROT);

  // SD card setup
  sd_present = initSD();
  sd_grid.setSdPresent(sd_present);

  if (sd_present)
  {
    if (!SD.exists(APP_FOLDER))
    {
      if (!SD.mkdir(APP_FOLDER))
      {
        Serial.println(F("Unable to create SYNTH directory."));
        Serial.println(F("Please create it manually on the SD card."));
      }
    }
  }

  beginDisplayOp12();
  playStartupSound();

  // Set up MIDI (IN only)
  midi_in.setHandleNoteOn(handleNoteOn);  
  midi_in.setHandleNoteOff(handleNoteOff);
  midi_in.begin(MIDI_CHANNEL_OMNI);
  
  blinkLED(2);
  Serial.println(F("Setup complete."));
}

void blinkLED(uint8_t count)         
{
    for (uint8_t i = 0; i < count; i++)
    {
        digitalWrite(ESP_LED, HIGH);
        delay(50);
        digitalWrite(ESP_LED, LOW);
        delay(50);
    }
}

void playStartupSound()
{
  op12_grp.setSelected(1);  // Hack to make sure op1 start-up
  op12_grp.setSelected(0);  // properties are loaded for play
  op12_grp.updateEnvelopes();
  op12_grp.getOpConfig(1, &op_cfg);
  syn_eng.setOpConfig(1, &op_cfg);
  op12_grp.getOpConfig(2, &op_cfg);
  syn_eng.setOpConfig(2, &op_cfg);

  playNote(60, 127, 120);  // C4
  playNote(64, 127, 120);  // E4
  playNote(67, 127, 120);  // G4
  playNote(72, 127, 120);  // C5
  playNote(72,   0, 500);
  updateWait(500);

  syn_eng.allOff();
}

void playNote(uint8_t note_num, uint8_t velocity, uint32_t duration_ms)
{
  if (velocity > 0)
  {
    syn_eng.noteOn(0, note_num, 127); 
  }
  else
  {
    syn_eng.noteOff(0, note_num); 
  }

  updateWait(duration_ms);
}

void updateWait(uint32_t duration_ms)
{
    ulong start_time = millis();

    while (millis() < start_time + duration_ms)
    {
      syn_eng.update();
      delay(10);
    }
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

void handleSelectionSD()
{
  TFT_sd_grid_btn_t action = sd_grid.getButton();
  int16_t slot_idx = sd_grid.getValue();
  bool success = true;

  if (action == TFT_SD_GRID_BTN_LOAD)
  {
    success = loadConfigFile(slot_idx);
  }
  else if (action == TFT_SD_GRID_BTN_SAVE)
  {
    success = saveConfigFile(slot_idx);
  }
  else 
  {
    Serial.println(F("Disk operation canceled."));
  }

  if (!success)
  {
    // TODO: some sort of visual feedback on failure
    Serial.println(F("SD operation FAILED."));
  }

  beginDisplayOp12();  // for now...
}

void setFilename(uint16_t index)
{
	cfg_filename[10] = char(48 + (index % 1000) / 100);
	cfg_filename[11] = char(48 + (index % 100) / 10);
	cfg_filename[12] = char(48 + (index % 10));
}

bool loadConfigFile(uint16_t index)
{
  Serial.println(F("Loading synth config file: "));
	setFilename(index);
  Serial.println(cfg_filename);

  int  bytes_read;
  File file;
  
  file = SD.open(cfg_filename, FILE_READ);
  if (!file)
  {
    Serial.println(F("Error opening config file!"));
    return false; // failure
  }
  
  SYN_header_config_t hdr_cfg;
  bytes_read = file.read((uint8_t *)&hdr_cfg, sizeof(SYN_header_config_t));  
  
  if (bytes_read < sizeof(SYN_header_config_t))
  {
    Serial.println(F("Error reading from synth config file!"));
    return false;  // failure
  }

  // TODO: handle header verification and versions

  // Op1: load and process config
  bytes_read = file.read((uint8_t *)&op_cfg, sizeof(SYN_op_config_t));  
  syn_eng.setOpConfig(1, &op_cfg);
  op12_grp.setOpConfig(1, &op_cfg);

  // Op2: load and process config
  bytes_read = file.read((uint8_t *)&op_cfg, sizeof(SYN_op_config_t));  
  syn_eng.setOpConfig(2, &op_cfg);
  op12_grp.setOpConfig(2, &op_cfg);

  // Op3: load and process config
  bytes_read = file.read((uint8_t *)&op_cfg, sizeof(SYN_op_config_t));  
  syn_eng.setOpConfig(3, &op_cfg);
  op34_grp.setOpConfig(3, &op_cfg);

  // Op4: load and process config
  bytes_read = file.read((uint8_t *)&op_cfg, sizeof(SYN_op_config_t));  
  syn_eng.setOpConfig(4, &op_cfg);
  op34_grp.setOpConfig(4, &op_cfg);

  // Filter
  bytes_read = file.read((uint8_t *)&fltr_cfg, sizeof(SYN_filter_config_t));  
  syn_eng.setFilterConfig(0, &fltr_cfg);
  fltr_grp.setFilterConfig(1, &fltr_cfg);

  // Global
  bytes_read = file.read((uint8_t *)&global_cfg, sizeof(SYN_global_config_t));  
  syn_eng.setGlobalConfig(&global_cfg);
  fltr_grp.setGlobalConfig(&global_cfg);

  // Sequencer
  bytes_read = file.read((uint8_t *)&seq_cfg, sizeof(SYN_sequence_config_t));  
  seq_grp.setSeqConfig(&seq_cfg);

  file.close();

  // See if the final read succceeded
  if (bytes_read < sizeof(SYN_sequence_config_t))
  {
    Serial.println(F("Error reading sequence from synth config file!"));
    return false;  // failure
  }

	return true; 
}

bool saveConfigFile(uint16_t index)
{
  Serial.println(F("Saving synth config file: "));
	setFilename(index);
  Serial.println(cfg_filename);
  
  File file;
  file = SD.open(cfg_filename, FILE_WRITE);
  if (!file)
  {
    Serial.println(F("Error creating synth config file!"));
    return false; // failure
  }
  
  // Save the config file header
  SYN_header_config_t hdr_cfg = {
    .header_id = SYN_CFG_HDR_SYN1,
    .version = 1,
    .reserved = 0
  };
  size_t   bytes_written;
  bytes_written = file.write((uint8_t *)&hdr_cfg, sizeof(SYN_header_config_t));
  
  if (bytes_written < sizeof(SYN_header_config_t))
  {
    Serial.println(F("Error writing to synth config file!"));
    return false;  // failure
  }
  
  // Save the Op configurations
  op12_grp.getOpConfig(1, &op_cfg);
  bytes_written = file.write((uint8_t *)&op_cfg, sizeof(SYN_op_config_t));
  op12_grp.getOpConfig(2, &op_cfg);
  bytes_written = file.write((uint8_t *)&op_cfg, sizeof(SYN_op_config_t));
  op34_grp.getOpConfig(3, &op_cfg);
  bytes_written = file.write((uint8_t *)&op_cfg, sizeof(SYN_op_config_t));
  op34_grp.getOpConfig(4, &op_cfg);    
  bytes_written = file.write((uint8_t *)&op_cfg, sizeof(SYN_op_config_t));
  
  // Save the filter configuration
  fltr_grp.getFilterConfig(1, &fltr_cfg);
  bytes_written = file.write((uint8_t *)&fltr_cfg, sizeof(SYN_filter_config_t));

  // Save the global synth engine configuration
  fltr_grp.getGlobalConfig(&global_cfg);
  bytes_written = file.write((uint8_t *)&global_cfg, sizeof(SYN_global_config_t));

  // Save the sequencer configuration
  seq_grp.getSeqConfig(&seq_cfg);
  bytes_written = file.write((uint8_t *)&seq_cfg, sizeof(SYN_sequence_config_t));

  file.close();

  // See if the final write succceeded
  if (bytes_written < sizeof(SYN_sequence_config_t))
  {
    Serial.println(F("Error writing sequence to synth config file!"));
    return false;  // failure
  }

  Serial.print(cfg_filename);
  Serial.println(F(" file successfully created."));
  return true; // success!
}

/*
 * Draws the SD card status icon indicating if the card is present or not.
 */
void drawSD(bool present)
{
  uint16_t color = (present ? ILI9341_WHITE : ILI9341_DARKGREY);
  tft.drawRGBBitmap(SD_TOUCH_X1 + 10, SD_TOUCH_Y1 + 2, (uint16_t *)sd_icon, 18, 24);
  if (!present)
    tft.drawLine(SD_TOUCH_X1 + 10, SD_TOUCH_Y1 + 2, SD_TOUCH_X1 + 30, SD_TOUCH_Y1 + 26, ILI9341_RED);  

  tft.drawRect(SD_TOUCH_X1, SD_TOUCH_Y1, SD_TOUCH_X2 - SD_TOUCH_X1, SD_TOUCH_Y2 - SD_TOUCH_Y1, color); // button outline
}

void beginScreenDisplay()
{
  tft.fillScreen(ILI9341_BLACK);

  tft.drawRGBBitmap(  0, 0, (uint16_t *)r4ge_pro_title, 175, 32);
  tft.drawRGBBitmap(  173, 4, (uint16_t *)synth_title, 80, 26);

  tft.drawLine(0, TOP_LINE, 319, TOP_LINE, ILI9341_BLUE);   
  tft.drawLine(0, BOTTOM_LINE, 319, BOTTOM_LINE, ILI9341_BLUE);  

  drawSD(sd_present);
  keybrd.setValue(1);
  keybrd.draw(&tft, true);
}

/*
 * Intial drawing and setup of the WAV display screen
 */
void beginDisplayOp12()
{
  beginScreenDisplay();

  tft.drawLine(0, MID_LINE, 319, MID_LINE, ILI9341_BLUE);   
  op12_grp.draw(&tft, true);

  app_mode = MODE_OP12;
}

void beginDisplayOp34()
{
  beginScreenDisplay();

  tft.drawLine(0, MID_LINE, 319, MID_LINE, ILI9341_BLUE);   
  op34_grp.draw(&tft, true);

  app_mode = MODE_OP34;
}

void beginDisplayFltr()
{
  beginScreenDisplay();
  
  fltr_grp.draw(&tft, true);

  app_mode = MODE_FLTR;
}

void beginDisplayStepSeq()
{
  beginScreenDisplay();
  
  seq_grp.draw(&tft, true);

  app_mode = MODE_STEP_SEQ;
}

/*
 * Inital drawing and setup of the select screen
 */
void beginSelect()
{
  sd_grid.reset();
  sd_grid.draw(&tft, true);
  app_mode = MODE_SELECT_SD;
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
      case MODE_OP12:
	      beginDisplayOp12();
		    break;
      case MODE_OP34:
	      beginDisplayOp34();
		    break;
      case MODE_FLTR:
	      beginDisplayFltr();
		    break;
      case MODE_STEP_SEQ:
        beginDisplayStepSeq();
        break;
      case MODE_SELECT_SD:
		    beginSelect();
		    break;
      
      default:
		    break;
    }
  }
  prev_app_mode = app_mode;
  
  int16_t prev_key_idx, key_idx = 0;
  prev_key_idx = keybrd.getValue() + 1 + (keybrd.getOctave() * 12); 
  keybrd.handleButtons(btn_pressed[BTN_UP], btn_pressed[BTN_DOWN], btn_released[BTN_LEFT], btn_released[BTN_RIGHT]);
  if (keybrd.handleTouch(touch_x, touch_y))
  {
    key_idx = keybrd.getValue() + 1 + (keybrd.getOctave() * 12); 
    
    syn_eng.noteOff(0, prev_key_idx);
    syn_eng.noteOn(0, key_idx, 127); 
  }
  else
  {
    // TODO: only turn off the prev note once!
    syn_eng.noteOff(0, prev_key_idx);
  }
  
  keybrd.draw(&tft, false);
  

  // Update the screen based on the app mode
  switch(app_mode)
  {
    case MODE_OP12:
      op12_grp.handleButtons(btn_pressed[BTN_UP], btn_pressed[BTN_DOWN], btn_released[BTN_LEFT], btn_released[BTN_RIGHT]);
      op12_grp.handleTouch(touch_x, touch_y);
      op12_grp.updateEnvelopes();

      if (op12_grp.getChanged())
      {
        op12_grp.getOpConfig(1, &op_cfg);
        syn_eng.setOpConfig(1, &op_cfg);
        op12_grp.getOpConfig(2, &op_cfg);
        syn_eng.setOpConfig(2, &op_cfg);
      }
      op12_grp.draw(&tft, false);
      break;

    case MODE_OP34:
      op34_grp.handleButtons(btn_pressed[BTN_UP], btn_pressed[BTN_DOWN], btn_released[BTN_LEFT], btn_released[BTN_RIGHT]);
      op34_grp.handleTouch(touch_x, touch_y);
      op34_grp.updateEnvelopes();

      if (op34_grp.getChanged())
      {
        op34_grp.getOpConfig(3, &op_cfg);
        syn_eng.setOpConfig(3, &op_cfg);
        op34_grp.getOpConfig(4, &op_cfg);
        syn_eng.setOpConfig(4, &op_cfg);
      }
      op34_grp.draw(&tft, false);
      break;

    case MODE_FLTR:
      fltr_grp.handleButtons(btn_pressed[BTN_UP], btn_pressed[BTN_DOWN], btn_released[BTN_LEFT], btn_released[BTN_RIGHT]);
      fltr_grp.handleTouch(touch_x, touch_y);
      
      if (fltr_grp.getChanged())
      {
        fltr_grp.getFilterConfig(1, &fltr_cfg);
        syn_eng.setFilterConfig(1, &fltr_cfg);

        fltr_grp.getGlobalConfig(&global_cfg);
        syn_eng.setGlobalConfig(&global_cfg);        
      }
      fltr_grp.draw(&tft, false);
      break;

    case MODE_STEP_SEQ:
      seq_grp.handleButtons(btn_pressed[BTN_UP], btn_pressed[BTN_DOWN], btn_released[BTN_LEFT], btn_released[BTN_RIGHT]);
      seq_grp.handleTouch(touch_x, touch_y);
      
      if (seq_grp.getChanged())
      {
        seq_grp.getSeqConfig(&seq_cfg);
      }
      seq_grp.draw(&tft, false);
      break;

    case MODE_SELECT_SD:
      sd_grid.handleButtons(btn_pressed[BTN_UP], btn_pressed[BTN_DOWN], btn_released[BTN_LEFT], btn_released[BTN_RIGHT]);
      if (sd_grid.handleTouch(touch_x, touch_y))
      {
        // TODO
      }
      sd_grid.draw(&tft, false);
      if (sd_grid.getSelectionDone())
      {
        handleSelectionSD();
      }
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
    btn_was_pressed[i] = btn_pressed[i];  // track previous state
    pressed = (digitalRead(SR_Q7) == LOW ? 1: 0);// read the state of the SO:
    btn_released[i] = !pressed && btn_pressed[i];
    btn_pressed[i] = pressed;
    // Shift the next button pin value into the serial data out
    digitalWrite(SR_CP, LOW);
    delay(1);
    digitalWrite(SR_CP, HIGH);
    delay(1);
    //Serial.print(i); Serial.print(": "); Serial.print(btn_pressed[i]); Serial.print(" - "); Serial.println(btn_released[i]);
  }
}

/*
 * Check the analog joystick values
 */
void checkJoysticks()
{
  // Only use the left 10 bits of the joystick pot readings
  // Center normally ends up around 460
  // May want to use analogReadResolution() at some point to eliminate the need for shifts
  joy_x_left = analogRead(JOYX_L) >> 2;
  joy_y_left = analogRead(JOYY_L) >> 2;
  
  joy_x_right = analogRead(JOYX_R) >> 2;
  joy_y_right = analogRead(JOYY_R) >> 2;
}

float normalizeJoy(int16_t joy_val)
{
  float norm_val = 1;

  // Use center "deadzone" to avoid fluctuations on an inactive stick
  if (joy_val < 450 || joy_val > 475) 
  {
    norm_val = (float)joy_val / 460.0; 
  }
  return norm_val;
}

/*
 * Checks to see if the screen is being touched on any active buttons or cells.
 * Returns true if the screen is being touched, otherwise false.
 */
bool checkScreenTouch(bool debug)
{
  btnSD_released = false;
  
  bool is_touched = ts.touched();
  touch_x = -1;
  touch_y = -1;

  if (is_touched) 
  {
    TS_Point p = ts.getPoint();
    touch_x = SCREEN_WD * (((float)p.x - 340) / 3500); 
    touch_y = SCREEN_HT * (((float)p.y - 350) / 3500);

    if (debug)
    {
      //tft.drawPixel(x, y, ILI9341_RED); // TESTING: destructive display of x,y pos
      // Display the x,y touch coordinate for debugging/calibration
      tft.setCursor(0, 220);
      tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
      tft.print(p.x);
      tft.print(", ");
      tft.print(p.y);
      tft.print("   ");
    }
    
    // Check if the touch is within the bounds of the "touch" button
    // btnSD_pressed = ((p.x > SD_TOUCH_X1) && (p.x < SD_TOUCH_X2) && (p.y > SD_TOUCH_Y1) && (p.y < SD_TOUCH_Y2)); 
btnSD_pressed = ((touch_x > SD_TOUCH_X1) && (touch_x < SD_TOUCH_X2) && 
                 (touch_y > SD_TOUCH_Y1) && (touch_y < SD_TOUCH_Y2)); 


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
  
  return is_touched;
}

/*
 * Main program loop.  Called continuously after setup.
 */
void loop(void) 
{ 
  midi_in.read();
  checkButtonPresses();

  midi_in.read();
  checkScreenTouch(false);
  
  midi_in.read();
  checkJoysticks();

  pitch_bend = normalizeJoy(joy_y_left);
  syn_eng.pitchBend(pitch_bend);

  mod_level = normalizeJoy(joy_x_left);
  syn_eng.modLevel(mod_level);

  if (btn_pressed[BTN_A] && !btn_was_pressed[BTN_A])  // Play selected note
  {
    syn_eng.noteOn(0, note_num, 127);  
  }
  else if (note_num != prev_note_num)
  {
    syn_eng.noteOff(0, prev_note_num); 
    syn_eng.noteOn(0, note_num, 127);  
  }
  prev_note_num = note_num;

  if (btn_released[BTN_A])  // stop playing selected note
  {
    syn_eng.noteOff(0, note_num);  
  }

  if (btnSD_released)
  {
    app_mode = (app_mode != MODE_SELECT_SD ? MODE_SELECT_SD : MODE_OP12);
  }

  if (btn_released[BTN_X])
  {
    // change to previous app mode
    app_mode = (app_mode_type)(app_mode == MODE_OP12 ? MODE_SELECT_SD : app_mode - 1);
  }

  if (btn_released[BTN_B])
  {
    // change to next app mode
    app_mode = (app_mode_type)(app_mode == MODE_SELECT_SD ? MODE_OP12: app_mode + 1);
  }

  if (btn_released[BTN_Y])
  {
    // Toggle sequence playback on and off
    play_seq = !play_seq;

    if (!play_seq) 
    {
      syn_eng.allOff();
    }
    
  }

  midi_in.read();
  updateScreen();

  midi_in.read();
  if (play_seq && (millis() - seq_note_start > seq_cfg.tempo))
  {
    uint8_t idx = seq_cfg.note_idx[seq_idx];
    note_num = midi_note[idx].note_num;
    
    syn_eng.noteOff(0, prev_note_num);
    if (note_num > 0)
    {
      syn_eng.noteOn(0, note_num, 127);
    }
    
    prev_note_num = note_num;
    seq_note_start = millis();

    seq_grp.setSelected(seq_idx);
    seq_idx = (seq_idx < SYN_SEQ_NOTE_COUNT - 1) ? seq_idx + 1: 0;
  }

  midi_in.read();
  
  syn_eng.update(); 

  digitalWrite(ESP_LED, LOW);
  //delay(10);
}
