#ifndef _heltec_hpp_
#define _heltec_hpp_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Renderer.h"

class Font;

class HeltecOLEDRenderer : public Renderer
{
private:
  TaskHandle_t _draw_task_handle;

public:
  HeltecOLEDRenderer(float world_size, Font *font);
  void start();
  friend void oled_draw_task(void *param);
};

#endif