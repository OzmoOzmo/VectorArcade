#pragma once

#include "Arcade/ArcadeGame.hpp"
#include <cstdint>

class JoystickControls;
class RenderBuffer;

class CalibrationDiagnostics : public ArcadeGame
{
private:
  JoystickControls &controls;
  float half_width = 1.0f;
  float half_height = 1.0f;
  int min_x = 4095;
  int max_x = 0;
  int min_y = 4095;
  int max_y = 0;
  uint32_t rail_x_samples = 0;
  uint32_t rail_y_samples = 0;
  uint32_t last_report_ms = 0;

  static float adjustment(int raw, int center);
  void report();

public:
  explicit CalibrationDiagnostics(JoystickControls &input) : controls(input) {}
  void step_world(float elapsed_time) override;
  void render_frame(RenderBuffer &buffer) override;
};
