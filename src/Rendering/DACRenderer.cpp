#include "driver/dac.h"
#include "DACRenderer.h"
#include "../FastDAC.h"

static DACRenderer *active_dac_renderer = nullptr;
static hw_timer_t *dac_timer = nullptr;

static inline int IRAM_ATTR scaleDacOutput(int value)
{
  constexpr int kDacCenter = 128;
  return kDacCenter +
         (value - kDacCenter) * HardwareConfig::kDacOutputScalePercent / 100;
}

void IRAM_ATTR draw_timer()
{
  if (active_dac_renderer != nullptr)
  {
#if ASTEROIDS_RENDERER_BENCHMARK
    const uint32_t started = Renderer::benchmarkCycleCount();
    active_dac_renderer->draw();
    active_dac_renderer->recordBenchmarkDraw(Renderer::benchmarkCycleCount() - started);
#else
    active_dac_renderer->draw();
#endif
  }
}

static inline void IRAM_ATTR writeDacPair(DACRenderer *renderer, int x, int y)
{
#if ASTEROIDS_RENDERER_BENCHMARK
  const uint32_t started = Renderer::benchmarkCycleCount();
#endif
  DAC1Write(scaleDacOutput(x));
  DAC2Write(scaleDacOutput(y));
#if ASTEROIDS_RENDERER_BENCHMARK
  renderer->recordBenchmarkOutput(Renderer::benchmarkCycleCount() - started);
#endif
}

void DACRenderer::draw()
{
  if (!segment_active)
  {
    if (draw_position >= render_buffer->display_frame->size())
    {
      rendered_frames = rendered_frames + 1;
      render_buffer->swapBuffers();
      draw_position = 0;
      return;
    }

    const DrawInstruction_t &instruction = render_buffer->display_frame->at(draw_position);
    target_x = instruction.x;
    target_y = instruction.y;
    if (!instruction.laser)
    {
      set_laser(false);
      current_x = target_x;
      current_y = target_y;
      writeDacPair(this, current_x, current_y);
      transactions = transactions + 1;
      draw_position = draw_position + 1;
      return;
    }

    segment_dx = abs(target_x - current_x);
    segment_dy = abs(target_y - current_y);
    segment_sx = current_x < target_x ? 1 : -1;
    segment_sy = current_y < target_y ? 1 : -1;
    segment_error = segment_dx - segment_dy;
    segment_active = true;
    set_laser(true);
  }

  writeDacPair(this, current_x, current_y);
  transactions = transactions + 1;

  if (current_x == target_x && current_y == target_y)
  {
    segment_active = false;
    draw_position = draw_position + 1;
    return;
  }

  const uint8_t pixels_per_update = render_buffer->display_is_menu
                                        ? HardwareConfig::kDacPixelsPerUpdateMenu
                                        : HardwareConfig::kDacPixelsPerUpdate;
  for (uint8_t pixel = 0; pixel < pixels_per_update; ++pixel)
  {
    if (current_x == target_x && current_y == target_y)
    {
      break;
    }

    const int16_t doubled_error = 2 * segment_error;
    if (doubled_error > -segment_dy)
    {
      segment_error -= segment_dy;
      current_x += segment_sx;
    }
    if (doubled_error < segment_dx)
    {
      segment_error += segment_dx;
      current_y += segment_sy;
    }
  }
}

void DACRenderer::start()
{
  draw_position = 0;
  ESP_ERROR_CHECK(dac_output_enable(DAC_CHANNEL_1));
  ESP_ERROR_CHECK(dac_output_enable(DAC_CHANNEL_2));
  DACPrepare(false);
  Renderer::start();

  active_dac_renderer = this;
  dac_timer = timerBegin(0, 4, true);
  if (dac_timer == nullptr)
  {
    record_error();
    Serial.println("ERROR: failed to create DAC renderer timer");
    return;
  }
  timerAttachInterrupt(dac_timer, &draw_timer, true);
  timerAlarmWrite(dac_timer, HardwareConfig::kDacTimerAlarmTicks, true);
  timerAlarmEnable(dac_timer);
}

DACRenderer::DACRenderer(float world_size, Font *font)
    : draw_position(0),
      current_x(0),
      current_y(0),
      target_x(0),
      target_y(0),
      segment_dx(0),
      segment_dy(0),
      segment_sx(0),
      segment_sy(0),
      segment_error(0),
      segment_active(false)
{
  render_buffer = new RenderBuffer(
      0, 255,
      0, 255,
      128,
      128,
      128.0f / world_size,
      font);
}
