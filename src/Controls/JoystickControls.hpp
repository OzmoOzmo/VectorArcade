#pragma once

#include "Controls.hpp"

class JoystickControls : public Controls
{
#if defined(ARCADE_TESTS)
  friend struct ArcadeTestAccess;
#endif
private:
  int center_x = 2048;
  int center_y = 2048;
  int raw_x = 2048;
  int raw_y = 2048;
  float axis_x = 0;
  float axis_y = 0;
  float direction = 0;
  bool fire = false;
  bool button_b = false;
  bool fire_candidate = false;
  bool button_b_candidate = false;
  unsigned long fire_changed = 0;
  unsigned long button_b_changed = 0;

  static float normalize(int value, int center);
  static int read_axis(int pin);

public:
  void begin();
  void sample();
  float x() const { return axis_x; }
  float y() const { return axis_y; }
  int rawX() const { return raw_x; }
  int rawY() const { return raw_y; }
  int centerX() const { return center_x; }
  int centerY() const { return center_y; }
  bool is_firing() override { return fire; }
  bool is_thrusting() override { return button_b; }
  float get_direction() override { return direction; }
};