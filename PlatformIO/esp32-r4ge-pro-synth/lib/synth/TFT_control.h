/**
 * @file TFT_control.h
 * @author Paul Pagel (https://twobittinker.com)
 * @brief  Base class for all controls to be drawn to the screen.
 * @version 0.1
 * @date 2020-08-01
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#ifndef _TFT_CONTROL_
#define _TFT_CONTROL_

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

class TFT_control
{
  public:
    TFT_control(int16_t x, int16_t y, const char* label);
    virtual  void     draw(Adafruit_ILI9341 *tft, bool force_redraw);
    virtual  bool     handleTouch(int16_t x, int16_t y);
    virtual  bool     handleButtons(bool up_pressed, bool down_pressed);
    bool     getChanged();
    bool     getSelected();
    void     setSelected(bool selected);
    int16_t  getValue();
    void     setValue(int16_t value);
    
    
  protected:
    int16_t  _x;
    int16_t  _y;
    int16_t  _ht;
    int16_t  _wd;
    int16_t  _value;
    int16_t  _min;
    int16_t  _max;
    bool     _selected;
    bool     _changed;

    const char* _label;

};

#endif // _TFT_CONTROL_
