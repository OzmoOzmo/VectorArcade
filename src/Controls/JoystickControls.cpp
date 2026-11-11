#include "JoystickControls.hpp"
#include "../HardwareConfig.h"
#include <cmath>

float JoystickControls::normalize(int value, int center)
{
  const float offset = value - center;
  const float range = offset < 0 ? center : 4095 - center;
  const float normalized = constrain(offset / range, -1.0f, 1.0f);
  constexpr float dead_zone = 0.15f;
  if (fabsf(normalized) <= dead_zone)
  {
    return 0;
  }
  return copysignf((fabsf(normalized) - dead_zone) / (1 - dead_zone), normalized);
}

int JoystickControls::read_axis(int pin)
{
  constexpr int kSampleCount = 8;
  analogRead(pin);
  int total = 0;
  for (int sample_index = 0; sample_index < kSampleCount; ++sample_index)
  {
    total += analogRead(pin);
  }
  return total / kSampleCount;
}

void JoystickControls::begin()
{
  analogReadResolution(12);
  analogSetPinAttenuation(HardwareConfig::kJoystickXPin, ADC_11db);
  analogSetPinAttenuation(HardwareConfig::kJoystickYPin, ADC_11db);
  pinMode(HardwareConfig::kFirePin, INPUT_PULLUP);
  pinMode(HardwareConfig::kThrustPin, INPUT_PULLUP);
  int total_x = 0;
  int total_y = 0;
  for (int sample_index = 0; sample_index < 64; ++sample_index)
  {
    total_x += read_axis(HardwareConfig::kJoystickXPin);
    total_y += read_axis(HardwareConfig::kJoystickYPin);
    delay(2);
  }
  center_x = constrain(total_x / 64, 512, 3583);
  center_y = constrain(total_y / 64, 512, 3583);
  raw_x = center_x;
  raw_y = center_y;
}

void JoystickControls::sample()
{
  raw_x = read_axis(HardwareConfig::kJoystickXPin);
  raw_y = read_axis(HardwareConfig::kJoystickYPin);
  axis_x = normalize(raw_x, center_x) * (HardwareConfig::kJoystickInvertX ? -1 : 1);
  axis_y = normalize(raw_y, center_y) * (HardwareConfig::kJoystickInvertY ? -1 : 1);
  if (axis_x != 0 || axis_y != 0)
  {
    direction = atan2f(axis_x, -axis_y);
  }
  const unsigned long now = millis();
  const bool raw_fire = digitalRead(HardwareConfig::kFirePin) == LOW;
  const bool raw_b = digitalRead(HardwareConfig::kThrustPin) == LOW;
  if (raw_fire != fire_candidate)
  {
    fire_candidate = raw_fire;
    fire_changed = now;
  }
  if (raw_b != button_b_candidate)
  {
    button_b_candidate = raw_b;
    button_b_changed = now;
  }
  if (now - fire_changed >= 20)
  {
    fire = fire_candidate;
  }
  if (now - button_b_changed >= 20)
  {
    button_b = button_b_candidate;
  }
}