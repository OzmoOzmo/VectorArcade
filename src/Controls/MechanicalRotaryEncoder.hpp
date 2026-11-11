#ifndef _rotary_encoder_h_
#define _rotary_encoder_h_

#include <math.h>
#include <driver/gpio.h>
#include "RotaryEncoder.hpp"

class MechanicalRotaryEncoder : public RotaryEncoder
{
private:
  gpio_num_t _a_pin;
  gpio_num_t _b_pin;
  volatile int32_t _count;
  volatile int8_t _quarter_steps;
  volatile uint8_t _previous_state;

public:
  MechanicalRotaryEncoder(gpio_num_t a_pin, gpio_num_t b_pin);
  ~MechanicalRotaryEncoder();
  float get_angle()
  {
    return -3.0 * (float(_count) / 24.0f) * M_PI;
  }
  friend void _encoder_interrupt_handler(void *param);
};

#endif