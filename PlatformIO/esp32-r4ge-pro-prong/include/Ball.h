#ifndef _BALL_
#define _BALL_

#include <stdint.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define BALL_RADIUS      4


enum update_result_type {
  RESULT_NORMAL,
  RESULT_BOUNCE,
  RESULT_SCORE1,
  RESULT_SCORE2
};

class Ball
{
  public:
    Ball();
    void setLimits(int16_t top_limit, int16_t bottom_limit);
    void begin(uint16_t color);
    update_result_type update();
    void draw(Adafruit_ILI9341 *tft);
    void erase(Adafruit_ILI9341 *tft);
    int16_t hitsLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
    void bounce(double x_chng, double y_chng);
      
    int16_t getX();
    int16_t getY();
    int16_t getDirX();
    int16_t getDirY();
    int16_t getRadius();
    bool    isDead();
    
  private:
    uint16_t _color;
    int16_t  _x, _y, _prev_x, _prev_y; 
    int16_t  _radius;
    int16_t  _top_limit, _bottom_limit;
    int16_t  _dir_x, _dir_y;
    bool     _is_dead;
};

#endif // _BALL_
