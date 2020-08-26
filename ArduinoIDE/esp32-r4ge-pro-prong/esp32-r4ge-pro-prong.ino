/***************************************************
ESP32 R4ge Prong

Requires:
 - ESP32 R4ge Pro

Copyright (c) 2020 Paul Pagel
This is free software; see the license.txt file for more information.
There is no warranty; not even for merchantability or fitness for a particular purpose.
*****************************************************/

#include "esp32_r4ge_pro.h"
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "Ball.h" 
#include "Player.h"  
#include "Title.h"  
#include "Bounce_wav.h"
#include "Score_wav.h"

#define LINE_COLOR    ILI9341_GREEN
#define BALL_COLOR    ILI9341_WHITE
#define PADDLE_COLOR  ILI9341_YELLOW
#define SCORE_COLOR   ILI9341_WHITE

#define TOP_LINE      20
#define NET_LINE      160

#define SCORE_DELAY_MS 1000

enum game_state_type {
  STATE_TITLE,
  STATE_START,
  STATE_PLAY,
  STATE_SCORE,
  STATE_PAUSE,
  STATE_GAME_OVER
};

enum game_state_type game_state, prev_game_state;

bool btn_pressed[8], btn_released[8];
bool btnA_pressed, btnB_pressed, btnX_pressed, btnY_pressed;
bool btnUp_pressed, btnDown_pressed, btnLeft_pressed, btnRight_pressed;
bool spkrLeft_on, spkrRight_on;
bool btnTouch_pressed, btnTouch_released;
int16_t joy_x_left, joy_y_left, joy_x_right, joy_y_right; 
uint32_t state_start_time;

const uint8_t *audio_wav;
bool          audio_playing, audio_right, audio_left;
uint16_t      wav_length, sample_pos;

int16_t ball_x, ball_y;
double  ball_x_dir, ball_y_dir;
uint8_t num_players = 1;
Player  player[2];
Ball    ball;

// i2s configuration
// See https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/include/driver/driver/i2s.h

int i2s_port_num = 0; 
i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
  .sample_rate = 11025,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,  // (i2s_bits_per_sample_t) 8
  .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  //I2S_CHANNEL_FMT_ONLY_RIGHT, I2S_CHANNEL_FMT_RIGHT_LEFT
  .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),  // | I2S_COMM_FORMAT_PCM    
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,      // high interrupt priority. See esp_intr_alloc.h for more
  .dma_buf_count = 6,
  .dma_buf_len = 60,        
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

#define  BUFFER_SIZE          1024 
#define  SAMPLES_PER_BUFFER    512  // 2 bytes per sample (16bit x 2 channels for stereo)
uint8_t  audio_buffer[BUFFER_SIZE];

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

/*
 * Set up the board
 */
void setup() 
{
  Serial.begin(115200);
  Serial.println("ESP32 R4ge Prong"); 
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
  
  // Set up the TFT
  tft.begin();
  tft.setRotation(SCREEN_ROT);

  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);  

  drawTitle();
  game_state = STATE_TITLE;
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
void playAudio(const uint8_t *wav, uint16_t length, bool play_right, bool play_left)
{ 
  if (!play_right && !play_left) return;  // not playing anything, so bail
  
  audio_playing = initAudioI2S();
  audio_wav = wav;
  wav_length = length;
  audio_right = play_right;
  audio_left = play_left;
  sample_pos = 40;  // skip RIFF header, could detect & skip(?)

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

    // If using I2S_CHANNEL_FMT_ONLY_RIGHT
    //audio_buffer[buff_pos] = temp;
    //audio_buffer[buff_pos + 1] = (uint8_t)temp_msb;

    if (audio_left) // put sound data into right channel
    {
      audio_buffer[buff_pos] = temp;
      audio_buffer[buff_pos + 1] = (uint8_t)temp_msb;
    }
    else
    {
      audio_buffer[buff_pos] = 0;
      audio_buffer[buff_pos + 1] = 0;
    }

    if (audio_right) // put sound data into left channel
    {
      audio_buffer[buff_pos + 2] = (uint8_t)temp & 0xff;
      audio_buffer[buff_pos + 3] = (uint8_t)temp_msb;
    }
    else
    {
      audio_buffer[buff_pos + 2] = 0;  
      audio_buffer[buff_pos + 3] = 0;
    }
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

void playBounceWall()
{
  playAudio(bounce3_wav, BOUNCE_LENGTH, true, true);
}

void playBouncePaddle(bool play_right, bool play_left)
{
  playAudio(bounce1_wav, BOUNCE_LENGTH, play_right, play_left);
}

void playScore(bool play_right, bool play_left)
{
  playAudio(score_wav, BOUNCE_LENGTH, play_right, play_left);
}

/*
 * Draws the game title/splash screen
 */
void drawTitle()
{
  tft.fillScreen(ILI9341_BLACK);

  tft.drawRGBBitmap(40, 60, (uint16_t *)prong_title, 240, 78);
  
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_DARKGREY);
  tft.setCursor(80, 180);
  tft.print("[X] 1 Player");
  tft.setCursor(80, 200);
  tft.print("[Y] 2 Player");
}

/*
 * Initializes the game variables and draws the main gameplay screen
 */
void startGame(int players)
{
  tft.fillScreen(ILI9341_BLACK);
  tft.drawLine(0, TOP_LINE, SCREEN_WD, TOP_LINE, LINE_COLOR);
  
  for (int i = TOP_LINE; i < SCREEN_HT; i+= 8)
  {
    tft.drawLine(NET_LINE, i, NET_LINE, i+3, LINE_COLOR);
  }

  ball.setLimits(TOP_LINE + 1, SCREEN_HT - 1);
  player[0].setLimits(TOP_LINE + 1, SCREEN_HT - 1);
  player[1].setLimits(TOP_LINE + 1, SCREEN_HT - 1);

  player[0].begin(1, PADDLE_COLOR, (bool)(players > 0));
  player[1].begin(2, PADDLE_COLOR, (bool)(players > 1));

  drawScore();
}

/*
 * Draw the score for both players
 */
void drawScore()
{
  uint16_t score = player[0].getScore();
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(60, 0);
  tft.print(score);
  
  score = player[1].getScore();
  tft.setCursor(230, 0);
  tft.print(score);
}

/*
 * Draws a Paused message on the screen
 */
void drawPause()
{
  //tft.fillRect(124, 0, 112, 20, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_RED);
  tft.setCursor(124, 0);
  tft.print("PAUSED");
  
  delay(200);
  game_state = STATE_PAUSE;
}

/*
 * Erases the Paused message
 */
void erasePause()
{
  tft.fillRect(124, 0, 112, 20, ILI9341_BLACK);
  delay(200);
  game_state = STATE_PLAY;
}

/*
 * Draws the Game Over screen
 */
void drawGameOver()
{
  tft.fillRect (80, 70, 160, 55, ILI9341_BLACK);  // clear msg area
  tft.drawRect (80, 70, 160, 55, ILI9341_YELLOW); // border
  
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(105, 80);
  tft.print("GAME OVER");

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(105, 110);
  tft.print("PRESS [Y] TO START");
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
  //joy_x_left = (analogRead(JOYX_L) >> 7) - JOY_5BIT_CTR;

  if (btn_pressed[BTN_UP])
  {
    joy_y_left = 6;
  }
  else if (btn_pressed[BTN_DOWN])
  {
    joy_y_left = -6;
  }
  else
  {
    joy_y_left = (analogRead(JOYY_L) >> 7) - JOY_5BIT_CTR;
  }

  if (btn_pressed[BTN_Y])
  {
    joy_y_right = 6;
  }
  else if (btn_pressed[BTN_A])
  {
    joy_y_right = -6;
  }
  else
  {
    joy_y_right = (analogRead(JOYY_R) >> 7) - JOY_5BIT_CTR;
  }
}

/*
 * Main program loop.  Called continuously after setup.
 */
void loop(void) 
{
  checkButtonPresses();
  checkJoysticks();
    
  if (prev_game_state != game_state)
  {
    state_start_time = millis();  // track when the state change started
    
    // Do initial screen drawing for new game state
    switch(game_state)
    {
      case STATE_TITLE:
        drawTitle();
        break;
      case STATE_START:
        startGame(num_players);
        break;
      case STATE_PLAY:
        if (ball.isDead()) ball.begin(BALL_COLOR);
        break;
      case STATE_SCORE:
        drawScore();
        ball.erase(&tft);
        break;
      case STATE_PAUSE:
        drawPause();
        break;
      case STATE_GAME_OVER:
        drawGameOver();
        break;
    }
  }
  prev_game_state = game_state;
  
  // Update the screen based on the game state
  switch(game_state)
  {
    case STATE_TITLE:
      handleTitle(); 
      break;
    case STATE_START:
      handleStart();
      break;
    case STATE_PLAY:
      handlePlay();
      break;
    case STATE_SCORE:
      handleScore();
      break;
    case STATE_PAUSE:
      handlePause(); 
      break;
    case STATE_GAME_OVER:
      handleGameOver(); 
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
    num_players = 1;
    game_state = STATE_START;
    return;
  }

  if (btn_released[BTN_Y])
  {
    num_players = 2;
    game_state = STATE_START;
    return;
  }
}

/*
 * Handles the STATE_START game state logic
 */
void handleStart()
{
  game_state = STATE_PLAY;
}

/*
 * Handles the STATE_PLAYING game state logic
 */
void handlePlay()
{
  // player controls logic
  if (btn_released[BTN_X])
  {
    drawPause();
    return;
  }

  // Do update logic and checks
  update_result_type result;
  result = ball.update();

  switch(result)
  {
    case RESULT_BOUNCE:
      playBounceWall();
      break;
    case RESULT_SCORE1:
      player[0].changeScore(1);
      playScore(false, true);
      game_state = STATE_SCORE;
      break;
    case RESULT_SCORE2:
      player[1].changeScore(1);
      playScore(true, false);
      game_state = STATE_SCORE;
      break;
    default:
      break;
  }

  if (game_state != STATE_PLAY) return;

  // Redraw net - TODO: still gaps too long
  int16_t ball_y = ball.getY() - 4;
  for (int i = ball_y; i < ball_y + 4; i++)
  {
    if (i & 0x0004) tft.drawPixel(NET_LINE, i, LINE_COLOR);
  }
  
  result = player[0].update(joy_y_left, &ball);
  if (result == RESULT_BOUNCE) playBouncePaddle(true, false);
  
  result = player[1].update(joy_y_right, &ball);
  if (result == RESULT_BOUNCE) playBouncePaddle(false, true);
  
  // Draw results to the screen
  ball.draw(&tft);
  player[0].draw(&tft);
  player[1].draw(&tft);
}

/*
 * Handles the STATE_PAUSED game state logic
 */
void handlePause()
{
  if (btn_released[BTN_X])
  {
   erasePause();
  }
}

/*
 * Handles the STATE_GAME_OVER game state logic
 */
void handleGameOver()
{
  if (btn_released[BTN_Y])
  {
    drawTitle();
  }
}

/*
 * Handles the STATE_SCORE game state logic
 */
void handleScore()
{

  if (millis() -  state_start_time >= SCORE_DELAY_MS)
  {
    game_state = STATE_PLAY;
    return;
  }

  player[0].update(joy_y_left, &ball);
  player[1].update(joy_y_right, &ball);

  player[0].draw(&tft);
  player[1].draw(&tft);
  
}
