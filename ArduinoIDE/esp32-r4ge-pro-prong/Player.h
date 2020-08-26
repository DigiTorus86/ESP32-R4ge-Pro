#ifndef _PLAYER_
#define _PLAYER_

#include <stdint.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "Ball.h"

#define PLAYER_PADDLE_HT      40
#define PLAYER_PADDLE_WD       5
#define PLAYER_PADDLE_MARGIN  10

struct paddle_bounce_t
{
  double x_change;
  double y_change;
};

class Player
{
  public:
    Player();
    void setLimits(int16_t top_limit, int16_t bottom_limit);
    void begin(uint16_t player_number, uint16_t color, bool is_human);
    update_result_type update(int16_t controller_y, Ball *ball);
    void     draw(Adafruit_ILI9341 *tft);
    int16_t  hitTest(int16_t x, int16_t y, int16_t dir_x, int16_t dir_y);
    int16_t  getX();
    int16_t  getY();
    uint16_t getScore();
    uint16_t changeScore(int8_t changeAmt);
    
  private:
    uint16_t _player_number, _score, _color;
    int16_t  _x, _y, _prev_x, _prev_y; 
    int16_t  _ht, _wd, _half_wd;
    int16_t  _top_limit, _bottom_limit;
    int16_t  _max_up_speed, _max_dn_speed;
    bool     _is_human;
    paddle_bounce_t _paddle_angle[11] = {
      {-1, 1.2}, {-1, 1}, {-1, 1}, {-1, 1}, {-1.2, 1}, {-1.5, 0.8}, {-1.5, 0.8}, {-1.2, 1}, {-1, 1}, {-1, 1}, {-1, 1.2}
    };
};

#endif // _PLAYER_
