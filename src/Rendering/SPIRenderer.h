#pragma once

#include <Arduino.h>
#include <esp_attr.h>
#include "Renderer.h"

class RenderBuffer;
class Font;

typedef struct spi_device_t *spi_device_handle_t; ///< Handle for a device on a SPI bus

class SPIRenderer : public Renderer
{
private:
  spi_device_handle_t spi;
  void draw();
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
  void write_position(int16_t x, int16_t y);

public:
  SPIRenderer(float world_size, Font *font);
  void start();
  friend void spi_draw_timer();
  friend void spi_draw_task(void *parameter);
};
