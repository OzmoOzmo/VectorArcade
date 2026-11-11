#include "GameLoop.h"
#include "../Arcade/ArcadeGame.hpp"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../Rendering/RenderBuffer.hpp"
#include "../Controls/Controls.hpp"

static GameLoop *active_game_loop = nullptr;
static hw_timer_t *game_timer = nullptr;

void IRAM_ATTR game_timer_alarm()
{
  GameLoop *game = active_game_loop;
  BaseType_t task_woken = pdFALSE;
  xTaskNotifyFromISR(game->game_task_handle, 1, eIncrement, &task_woken);
  if (task_woken == pdTRUE)
  {
    portYIELD_FROM_ISR();
  }
}

void game_task(void *param)
{
  GameLoop *game_loop = static_cast<GameLoop *>(param);

  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
  float prev_time = esp_timer_get_time() / 1E6f;
  while (true)
  {
    uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    if (ulNotificationValue > 0)
    {
      // work out the elapsed time - it's good to keep this accurate so the physics simulation and any timers are correct
      float cur_time = esp_timer_get_time() / 1E6f;
      float elapsed_time = cur_time - prev_time;
      game_loop->game->step_world(elapsed_time);
      prev_time = cur_time;
      // re-render if we need to
      game_loop->game->render_frame(*game_loop->render_buffer);
      game_loop->steps++;
    }
  }
}

void GameLoop::start()
{
  active_game_loop = this;
  if (xTaskCreatePinnedToCore(game_task, "Game Loop", 4096, this, 1, &game_task_handle, 0) != pdPASS)
  {
    ++errors;
    Serial.println("ERROR: failed to create game loop task");
    return;
  }
  game_timer = timerBegin(1, 4, true);
  if (game_timer == nullptr)
  {
    ++errors;
    Serial.println("ERROR: failed to create game loop timer");
    return;
  }
  timerAttachInterrupt(game_timer, &game_timer_alarm, true);
  timerAlarmWrite(game_timer, 333333, true);
  timerAlarmEnable(game_timer);
}
