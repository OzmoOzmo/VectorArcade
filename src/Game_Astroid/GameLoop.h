#ifndef _esp32_game_h
#define _esp32_game_h

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class RenderBuffer;
class ArcadeGame;

class GameLoop
{
private:
  TaskHandle_t game_task_handle;
  ArcadeGame *game;
  RenderBuffer *render_buffer;

public:
  GameLoop(ArcadeGame *game, RenderBuffer *render_buffer) : game(game), render_buffer(render_buffer), steps(0), errors(0) {}
  int steps;
  volatile uint32_t errors;
  void start();
  friend void game_task(void *param);
  friend void game_timer_alarm();
};

#endif