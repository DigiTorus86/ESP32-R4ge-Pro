#include "Ball.h"
#include "esp32_r4ge_pro.h"


Ball::Ball()
{
  _radius = BALL_RADIUS;
  _color = ILI9341_WHITE;
  _is_dead = true;
}

void Ball::setLimits(int16_t top_limit, int16_t bottom_limit)
{
  _top_limit = top_limit;
  _bottom_limit = bottom_limit;
}

void Ball::begin(uint16_t color)
{
  _color = color;
  
  _x = SCREEN_WD >> 1;
  _y = 120;
  _prev_x = _x;
  _prev_y = _y;
  _is_dead = false;
  
  _dir_x = random(4) + 4;
  if (random(2)) _dir_x = -_dir_x;

  _dir_y = random(4) + 2;
  if (random(2)) _dir_y = -_dir_y;
}
    
update_result_type Ball::update()
{
  update_result_type result = RESULT_NORMAL;
  int16_t next_x = _x + _dir_x;
  int16_t next_y = _y + _dir_y;

  _prev_x = _x;
  _prev_y = _y;
  
  if ((next_y >= _bottom_limit - _radius) || (next_y <= _top_limit + _radius))
  {
    _dir_y = -_dir_y;
    result = RESULT_BOUNCE;
  }
  
  if (next_x >= SCREEN_WD - _radius)
  {
    result = RESULT_SCORE1;
    _is_dead = true;
  }

  if (next_x <= _radius)
  {
    result = RESULT_SCORE2;
    _is_dead = true;
  }
  
  _x += _dir_x;
  _y += _dir_y;

  return result;
}

void Ball::draw(Adafruit_ILI9341 *tft)
{
  // Erase old ball position 
  tft->fillCircle(_prev_x, _prev_y, _radius, ILI9341_BLACK);

  // Draw new ball position
  tft->fillCircle(_x, _y, _radius, _color);  
}

void Ball::erase(Adafruit_ILI9341 *tft)
{
  // Erase old ball position 
  tft->fillCircle(_prev_x, _prev_y, _radius, ILI9341_BLACK);

  // Erase new ball position
  tft->fillCircle(_x, _y, _radius, ILI9341_BLACK);  
}

// Return values:  0 = no hit, 1-9 indicates a hit
//                 1 = top, 5 = center. 9 = bottom
int16_t Ball::hitsLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
  int16_t hit = 0;
  int16_t ctr_y = (y2 - y1) / 2;
  
  if (y1 > _y || y2 < _y) return 0;  // no hit
  
  if (abs(x1 + _radius - _x) < 5) // TODO: fine-tune
  {
    
    hit = map(y2 - _y, 0, y2 - y1, 10, 1);  // TODO: angle
  }

  return hit;
}

void Ball::bounce(double x_chng, double y_chng)
{
  _dir_x = (int16_t)((double)_dir_x * x_chng);
  _dir_y = (int16_t)((double)_dir_y * y_chng);
}

int16_t Ball::getX()
{
  return _x;
}

int16_t Ball::getY()
{
  return _y;
}

int16_t Ball::getDirX()
{
  return _dir_x;
}

int16_t Ball::getDirY()
{
  return _y;
}

int16_t Ball::getRadius()
{
  return _radius;
}

bool Ball::isDead()
{
  return _is_dead;
}
