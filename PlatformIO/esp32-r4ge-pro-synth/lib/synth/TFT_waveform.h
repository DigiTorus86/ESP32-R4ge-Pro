/**
 * @file TFT_waveform.h
 * @author Paul Pagel (https://twobittinker.com)
 * @brief  Displays a waveform to the screen.
 * @version 0.1
 * @date 2020-07-28
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#ifndef _TFT_WAVEFORM_
#define _TFT_WAVEFORM_

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "TFT_control.h"
#include "SYN_common.h"

#define TFT_WAV_DEFAULT_COLOR      ILI9341_GREEN
#define TFT_WAV_DEFAULT_BGD_COLOR  ILI9341_BLACK


class TFT_waveform
{
  public:
    TFT_waveform(int16_t x, int16_t y, int16_t wd, int16_t ht);
    void     draw(Adafruit_ILI9341 *tft, SYN_wave_type wave_type);
    void     drawName(Adafruit_ILI9341 *tft, SYN_wave_type wave_type);
    void     setColor(uint16_t wave_color);
    void     setColor(uint16_t wave_color, uint16_t bgd_color);
    
  private:
    int16_t  _x, _y, _ht, _wd;
    int16_t  _zero_y, _ctr_y;
    int16_t  _half_wd;
    float    _scale_factor;
    uint16_t _wave_color, _bgd_color;

    void drawSilence(Adafruit_ILI9341 *tft);
    void drawSine(Adafruit_ILI9341 *tft);
    void drawSquare(Adafruit_ILI9341 *tft);
    void drawTriangle(Adafruit_ILI9341 *tft);
    void drawSaw(Adafruit_ILI9341 *tft);
    void drawRamp(Adafruit_ILI9341 *tft);
    void drawMajor(Adafruit_ILI9341 *tft);
    void drawMinor(Adafruit_ILI9341 *tft);
    void drawOct3(Adafruit_ILI9341 *tft);
    void drawCustom(Adafruit_ILI9341 *tft);

};

#endif // _TFT_WAVEFORM_
