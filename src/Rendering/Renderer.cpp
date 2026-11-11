#include "Renderer.h"
#include "../Game_Astroid/Game.hpp"

Renderer::Renderer() : render_buffer(NULL)
{
  rendered_frames = 0;
  transactions = 0;
  delays = 0;
  error_count = 0;
#if ASTEROIDS_RENDERER_BENCHMARK
  benchmark_draw_calls = 0;
  benchmark_draw_cycles = 0;
  benchmark_max_draw_cycles = 0;
  benchmark_output_calls = 0;
  benchmark_output_cycles = 0;
  benchmark_max_output_cycles = 0;
  benchmark_missed_timer_ticks = 0;
  benchmark_over_budget_draws = 0;
#endif
}

void Renderer::start()
{
  // setup the laser output
  gpio_set_direction(HardwareConfig::kLaserPin, GPIO_MODE_OUTPUT);
}

void Renderer::set_laser(bool on)
{
  gpio_set_level(HardwareConfig::kLaserPin, on ? 1 : 0);
}
