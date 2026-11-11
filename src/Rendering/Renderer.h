#ifndef _renderer_h_
#define _renderer_h_

#include "esp_attr.h"
#include "driver/gpio.h"
#include <vector>
#include "../HardwareConfig.h"
#include "RenderBuffer.hpp"

class Game;

class Renderer
{
protected:
  RenderBuffer *render_buffer;

public:
  Renderer();

  virtual void start() = 0;
  void IRAM_ATTR set_laser(bool on);
  RenderBuffer *get_render_buffer() { return render_buffer; }

  volatile int rendered_frames;
  volatile int transactions;
  volatile int delays;
  volatile uint32_t error_count;

#if ASTEROIDS_RENDERER_BENCHMARK
  volatile uint32_t benchmark_draw_calls;
  volatile uint32_t benchmark_draw_cycles;
  volatile uint32_t benchmark_max_draw_cycles;
  volatile uint32_t benchmark_output_calls;
  volatile uint32_t benchmark_output_cycles;
  volatile uint32_t benchmark_max_output_cycles;
  volatile uint32_t benchmark_missed_timer_ticks;
  volatile uint32_t benchmark_over_budget_draws;

  static inline uint32_t benchmarkCycleCount()
  {
    uint32_t cycles;
    asm volatile("rsr %0, ccount" : "=a"(cycles));
    return cycles;
  }

  void recordBenchmarkDraw(uint32_t cycles)
  {
    ++benchmark_draw_calls;
    benchmark_draw_cycles += cycles;
    if (cycles > benchmark_max_draw_cycles)
    {
      benchmark_max_draw_cycles = cycles;
    }
    if (cycles > F_CPU / 20000)
    {
      ++benchmark_over_budget_draws;
    }
  }

  void recordBenchmarkOutput(uint32_t cycles)
  {
    ++benchmark_output_calls;
    benchmark_output_cycles += cycles;
    if (cycles > benchmark_max_output_cycles)
    {
      benchmark_max_output_cycles = cycles;
    }
  }
#endif

  void record_error() { ++error_count; }
};

#endif