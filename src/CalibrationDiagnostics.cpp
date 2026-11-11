#include "CalibrationDiagnostics.hpp"
#include <Arduino.h>
#include "Arcade/VectorFrame.hpp"
#include "Controls/JoystickControls.hpp"
#include "HardwareConfig.h"
#include "Rendering/RenderBuffer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
constexpr float kMinimumHalfExtent = 0.75f;
constexpr float kMaximumHalfExtent = 15.0f;
constexpr float kResizeSpeed = 8.0f;
constexpr float kRawDeadband = 64.0f;
constexpr float kRawTravelForFullRate = 256.0f;
constexpr float kDacCountsPerWorldUnit = 128.0f / 30.0f;
constexpr int kAdcRailMargin = 32;

bool validJoystickAdc(int raw)
{
  return raw >= kAdcRailMargin && raw <= 4095 - kAdcRailMargin;
}

int dacMinimum(float half_extent)
{
  return static_cast<int>(lroundf(128.0f - half_extent * kDacCountsPerWorldUnit));
}

int dacMaximum(float half_extent)
{
  return static_cast<int>(lroundf(128.0f + half_extent * kDacCountsPerWorldUnit));
}

float estimatedDacVoltage(int code)
{
  return code * (3.3f / 255.0f);
}
}

float CalibrationDiagnostics::adjustment(int raw, int center)
{
  const float offset = static_cast<float>(raw - center);
  if (fabsf(offset) <= kRawDeadband)
  {
    return 0;
  }
  const float travel = (fabsf(offset) - kRawDeadband) /
                       (kRawTravelForFullRate - kRawDeadband);
  return copysignf(constrain(travel, 0.0f, 1.0f), offset);
}

void CalibrationDiagnostics::step_world(float elapsed_time)
{
  controls.sample();
  const int raw_x = controls.rawX();
  const int raw_y = controls.rawY();
  const bool valid_x = validJoystickAdc(raw_x);
  const bool valid_y = validJoystickAdc(raw_y);
  if (valid_x)
  {
    min_x = std::min(min_x, raw_x);
    max_x = std::max(max_x, raw_x);
  }
  else
  {
    ++rail_x_samples;
  }
  if (valid_y)
  {
    min_y = std::min(min_y, raw_y);
    max_y = std::max(max_y, raw_y);
  }
  else
  {
    ++rail_y_samples;
  }

  const float delta = std::min(std::max(elapsed_time, 0.0f), 0.1f);
  if (valid_x)
  {
    half_width = constrain(half_width + adjustment(raw_x, controls.centerX()) * kResizeSpeed * delta,
                           kMinimumHalfExtent,
                           kMaximumHalfExtent);
  }
  if (valid_y)
  {
    half_height = constrain(half_height + adjustment(raw_y, controls.centerY()) * kResizeSpeed * delta,
                            kMinimumHalfExtent,
                            kMaximumHalfExtent);
  }

  if (millis() - last_report_ms >= 250)
  {
    report();
    last_report_ms = millis();
  }
}

void CalibrationDiagnostics::report()
{
  const int dac1_min = dacMinimum(half_width);
  const int dac1_max = dacMaximum(half_width);
  const int dac2_min = 255 - dacMaximum(half_height);
  const int dac2_max = 255 - dacMinimum(half_height);
  Serial.printf(
      "CAL raw_x=%d center_x=%d seen_x=[%d,%d] rail_x=%lu "
      "raw_y=%d center_y=%d seen_y=[%d,%d] rail_y=%lu "
      "buttons_fire=%d buttons_thrust=%d square_half_world=(%.2f,%.2f) "
      "dac1_codes=[%d,%d] approx_V=[%.2f,%.2f] "
      "dac2_codes=[%d,%d] approx_V=[%.2f,%.2f]\n",
      controls.rawX(), controls.centerX(), min_x, max_x,
      static_cast<unsigned long>(rail_x_samples),
      controls.rawY(), controls.centerY(), min_y, max_y,
      static_cast<unsigned long>(rail_y_samples),
      controls.is_firing(), controls.is_thrusting(),
      half_width, half_height,
      dac1_min, dac1_max,
      estimatedDacVoltage(dac1_min), estimatedDacVoltage(dac1_max),
      dac2_min, dac2_max,
      estimatedDacVoltage(dac2_min), estimatedDacVoltage(dac2_max));
}

void CalibrationDiagnostics::render_frame(RenderBuffer &buffer)
{
  if (!buffer.needs_render)
  {
    return;
  }

  VectorFrame frame(buffer);
  const b2Vec2 top_left(-half_width, -half_height);
  const b2Vec2 top_right(half_width, -half_height);
  const b2Vec2 bottom_right(half_width, half_height);
  const b2Vec2 bottom_left(-half_width, half_height);
  frame.line(top_left, top_right);
  frame.line(top_right, bottom_right);
  frame.line(bottom_right, bottom_left);
  frame.line(bottom_left, top_left);
  frame.finish();
}
