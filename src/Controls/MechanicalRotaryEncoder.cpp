#include "MechanicalRotaryEncoder.hpp"

void IRAM_ATTR _encoder_interrupt_handler(void *param)
{
  auto *rotary_encoder = static_cast<MechanicalRotaryEncoder *>(param);
  const uint8_t state = (gpio_get_level(rotary_encoder->_a_pin) << 1) |
                        gpio_get_level(rotary_encoder->_b_pin);
  const uint8_t transition = (rotary_encoder->_previous_state << 2) | state;
  int8_t direction = 0;

  switch (transition)
  {
  case 0b1101:
  case 0b0100:
  case 0b0010:
  case 0b1011:
    direction = -1;
    break;
  case 0b0111:
  case 0b1110:
  case 0b1000:
  case 0b0001:
    direction = 1;
    break;
  default:
    break;
  }

  rotary_encoder->_previous_state = state;
  rotary_encoder->_quarter_steps += direction;
  if (rotary_encoder->_quarter_steps >= 4)
  {
    ++rotary_encoder->_count;
    rotary_encoder->_quarter_steps = 0;
  }
  else if (rotary_encoder->_quarter_steps <= -4)
  {
    --rotary_encoder->_count;
    rotary_encoder->_quarter_steps = 0;
  }
}

MechanicalRotaryEncoder::MechanicalRotaryEncoder(gpio_num_t clck_pin, gpio_num_t di_pin)
{
  _count = 0;
  _quarter_steps = 0;
  _a_pin = clck_pin;
  _b_pin = di_pin;
  gpio_set_direction(clck_pin, GPIO_MODE_INPUT);
  gpio_set_direction(di_pin, GPIO_MODE_INPUT);
  gpio_set_pull_mode(clck_pin, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(di_pin, GPIO_PULLUP_ONLY);

  _previous_state = (gpio_get_level(clck_pin) << 1) | gpio_get_level(di_pin);
  const esp_err_t isr_service_status = gpio_install_isr_service(0);
  if (isr_service_status != ESP_OK && isr_service_status != ESP_ERR_INVALID_STATE)
  {
    return;
  }

  gpio_set_intr_type(clck_pin, GPIO_INTR_ANYEDGE);
  gpio_set_intr_type(di_pin, GPIO_INTR_ANYEDGE);
  gpio_isr_handler_add(clck_pin, _encoder_interrupt_handler, this);
  gpio_isr_handler_add(di_pin, _encoder_interrupt_handler, this);
  gpio_intr_enable(clck_pin);
  gpio_intr_enable(di_pin);
}

MechanicalRotaryEncoder::~MechanicalRotaryEncoder()
{
  gpio_intr_disable(_a_pin);
  gpio_intr_disable(_b_pin);
  gpio_isr_handler_remove(_a_pin);
  gpio_isr_handler_remove(_b_pin);
}
