#include "Player.h"
#include "esp32_r4ge_pro.h"

Player::Player()
{
  _color = ILI9341_WHITE;
  _ht = PLAYER_PADDLE_HT;
  _wd = PLAYER_PADDLE_WD;
  _half_wd = PLAYER_PADDLE_WD / 2;

  // TODO: adjust sensitivity (for human) and reaction speed (for computer)
  _max_up_speed = -4;
  _max_dn_speed = 4;
}

void Player::setLimits(int16_t top_limit, int16_t bottom_limit)
{
  _top_limit = top_limit;
  _bottom_limit = bottom_limit;
}

void Player::begin(uint16_t player_number, uint16_t color, bool is_human)
{
  _player_number = player_number;
  _color = color;
  _is_human = is_human;
  
  _score = 0;
  _x = (player_number == 1 ? PLAYER_PADDLE_MARGIN : SCREEN_WD - PLAYER_PADDLE_MARGIN - PLAYER_PADDLE_WD);
  _prev_x = _x;
  _y = (_bottom_limit - _top_limit) / 2 - PLAYER_PADDLE_HT / 2;
  _prev_y = _y;
}

update_result_type Player::update(int16_t controller_y, Ball *ball)
{ 
  update_result_type result = RESULT_NORMAL;  
  _prev_y = _y;

  if (_is_human)
  {
    _y -= controller_y;
  }
  else
  {
    //int16_t diff = min(max((int)_max_up_speed, _y + _half_wd - ball_y), _max_dn_speed);
    int16_t diff = _y + _half_wd - ball->getY();
    if (diff < _max_up_speed) diff = _max_up_speed;
    if (diff > _max_dn_speed) diff = _max_dn_speed;
    _y -= diff;
  }

  // Enforce playing area limits
  if (_y < _top_limit) _y = _top_limit;
  if (_y + _ht > _bottom_limit) _y = _bottom_limit - _ht;

  // check to see if ball hits the paddle
  int16_t hit;
  if (_player_number == 1)
    hit = ball->hitsLine(_x + _wd, _y, _x + _wd, _y + _ht); // left player surface
  else
    hit = ball->hitsLine(_x, _y, _x, _y + _ht);  // right player surface

  if (hit != 0)
  {
    // 1 - 10 = hit, 5 = center, 1 = top end, 10 = bottom end
    double x_chg = _paddle_angle[hit].x_change;
    double y_chg = _paddle_angle[hit].y_change;
    Serial.printf("X: %f,  Y: %f", x_chg, y_chg);
    
    ball->bounce(x_chg, y_chg);
    result = RESULT_BOUNCE;
  }

  return result;
}

void Player::draw(Adafruit_ILI9341 *tft)
{
  // Erase old paddle position 
  // TODO: erase only the part that doesn't overlap new position
  tft->fillRect(_x, _prev_y, _wd, _ht, ILI9341_BLACK);

  // Draw new paddle position
  tft->fillRect(_x, _y, _wd, _ht, _color);  
}

uint16_t Player::getScore()
{
  return _score;
}
    
uint16_t Player::changeScore(int8_t changeAmt)
{
  _score += changeAmt;
  return _score;
}

int16_t Player::hitTest(int16_t x, int16_t y, int16_t dir_x, int16_t dir_y)
{
  // TODO
}

int16_t Player::getX()
{
  return _x;
}

int16_t Player::getY()
{
  return _y;
}
