#ifndef _dac_renderer_h_
#define _dac_renderer_h_

#include <Arduino.h>
#include "Renderer.h"

class RenderBuffer;
class Font;

class DACRenderer : public Renderer
{
private:
  volatile int draw_position;
  int16_t current_x;
  int16_t current_y;
  int16_t target_x;
  int16_t target_y;
  int16_t segment_dx;
  int16_t segment_dy;
  int16_t segment_sx;
  int16_t segment_sy;
  int16_t segment_error;
  bool segment_active;
  void IRAM_ATTR draw();

public:
  DACRenderer(float world_size, Font *font);
  void start();
  friend void draw_timer();
};

#endif