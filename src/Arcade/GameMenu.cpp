#include "GameMenu.hpp"
#include "VectorFrame.hpp"
#include "../Controls/JoystickControls.hpp"
#include "../Game_Astroid/Game.hpp"
#include "../Game_Lunar/LunarGame.hpp"
#include "../Game_Missile/MissileGame.hpp"
#include "../HardwareConfig.h"
#include <algorithm>
#include <cmath>

namespace
{
void drawDashedLine(VectorFrame &frame, const b2Vec2 &start, const b2Vec2 &end)
{
  constexpr int kDashCount = 3;
  constexpr float kDashFraction = 0.72f;
  const b2Vec2 direction = end - start;
  for (int dash = 0; dash < kDashCount; ++dash)
  {
    const float section_start = static_cast<float>(dash) / kDashCount;
    const float section_length = 1.0f / kDashCount;
    const float inset = section_length * (1.0f - kDashFraction) * 0.5f;
    frame.line(start + (section_start + inset) * direction,
               start + (section_start + section_length - inset) * direction);
  }
}

void drawMenuBorder(VectorFrame &frame)
{
  const b2Vec2 top_left(-25.0f, -28.0f);
  const b2Vec2 top_right(25.0f, -28.0f);
  const b2Vec2 right_top(28.0f, -25.0f);
  const b2Vec2 right_bottom(28.0f, 22.0f);
  const b2Vec2 bottom_right(25.0f, 25.0f);
  const b2Vec2 bottom_left(-25.0f, 25.0f);
  const b2Vec2 left_bottom(-28.0f, 22.0f);
  const b2Vec2 left_top(-28.0f, -25.0f);

  drawDashedLine(frame, top_left, top_right);
  frame.line(top_right, right_top);
  drawDashedLine(frame, right_top, right_bottom);
  frame.line(right_bottom, bottom_right);
  drawDashedLine(frame, bottom_right, bottom_left);
  frame.line(bottom_left, left_bottom);
  drawDashedLine(frame, left_bottom, left_top);
  frame.line(left_top, top_left);
}
}

GameMenu::GameMenu(JoystickControls &input, SoundFX *effects) : controls(input), sound(effects)
{
#if ASTEROIDS_RENDERER_BENCHMARK
  launch();
#endif

    //selection = 2; launch();
}

void GameMenu::launch()
{
  if (selection == 0)
  {
    auto *asteroids = new Game(30, &controls, sound);
    asteroids->begin_playing();
    active.reset(asteroids);
  }
  else if (selection == 1)
  {
    active.reset(new LunarGame(controls, sound));
  }
  else
  {
    active.reset(new MissileGame(controls, sound));
  }
  waiting_for_release = true;
  result_delay = 0;
}

bool GameMenu::updateScreensaver(float elapsed_time)
{
  const bool input_active = controls.x() != 0 || controls.y() != 0 ||
                            controls.is_firing() || controls.is_thrusting();
  const float elapsed = std::max(0.0f, elapsed_time);
  if (screensaver_active)
  {
    if (input_active)
    {
      screensaver_active = false;
      inactivity_seconds = 0;
      waiting_for_neutral = true;
      previous_fire = controls.is_firing();
    }
    else
    {
      screensaver_seconds += elapsed;
    }
    return true;
  }
  if (waiting_for_neutral)
  {
    previous_fire = controls.is_firing();
    inactivity_seconds = 0;
    if (!input_active)
    {
      waiting_for_neutral = false;
    }
    return true;
  }
  inactivity_seconds = input_active ? 0 : inactivity_seconds + elapsed;
  if (HardwareConfig::kScreenSaverTimeoutSeconds != 0 &&
      inactivity_seconds >= HardwareConfig::kScreenSaverTimeoutSeconds)
  {
    screensaver_active = true;
    screensaver_seconds = 0;
    return true;
  }
  return false;
}

void GameMenu::renderScreensaver(RenderBuffer &buffer)
{
  VectorFrame frame(buffer);
  const char *title = "VECTOR ARCADE";
  const b2Vec2 size = buffer.draw_text(b2Vec2(0, 0), 0, 0, title, true);
  const auto bounce = [](float time, float speed, float span)
  {
    if (span <= 0)
    {
      return 0.0f;
    }
    const float travel = fmodf(time * speed, 2 * span);
    return travel <= span ? travel : 2 * span - travel;
  };
  frame.text(-28 + bounce(screensaver_seconds, 4.3f, 56 - size.x),
             -27 + bounce(screensaver_seconds, 3.1f, 54 - size.y), title);

  const b2Vec2 center(23 * sinf(screensaver_seconds * 0.37f + 1),
                      22 * sinf(screensaver_seconds * 0.53f));
  b2Vec2 previous = center + 3.0f * b2Vec2(cosf(screensaver_seconds), sinf(screensaver_seconds));
  const b2Vec2 first = previous;
  for (int vertex = 1; vertex <= 4; ++vertex)
  {
    const float angle = screensaver_seconds + vertex * b2_pi / 2;
    const b2Vec2 next = vertex == 4 ? first : center + 3.0f * b2Vec2(cosf(angle), sinf(angle));
    frame.line(previous, next);
    previous = next;
  }
  frame.finish();
}

void GameMenu::step_world(float elapsed_time)
{
  controls.sample();
  if (updateScreensaver(elapsed_time))
  {
    return;
  }
  const float delta = std::min(elapsed_time, 0.05f);
  const bool fire = controls.is_firing();
  const bool pressed = fire && !previous_fire;
  previous_fire = fire;
  if (waiting_for_release)
  {
    if (!fire)
    {
      waiting_for_release = false;
    }
    return;
  }

  if (active)
  {
    if (active->finished())
    {
      result_delay += delta;
      if (pressed && result_delay > 1)
      {
        active.reset();
        waiting_for_release = true;
      }
    }
    else
    {
      active->step_world(delta);
    }
    return;
  }

  navigation_delay = std::max(0.0f, navigation_delay - delta);
  if (controls.y() == 0)
  {
    navigation_delay = 0;
  }
  if (navigation_delay == 0 && (controls.y() < -0.45f || controls.y() > 0.45f))
  {
    selection = (selection + (controls.y() > 0 ? 1 : 2)) % 3;
    navigation_delay = 0.3f;
  }
  if (pressed)
  {
    launch();
  }
}

void GameMenu::render_frame(RenderBuffer &buffer)
{
  if (!buffer.needs_render)
  {
    return;
  }
  buffer.drawing_is_menu = !screensaver_active && !active;
  if (screensaver_active)
  {
    renderScreensaver(buffer);
    return;
  }
  if (active)
  {
    active->render_frame(buffer);
    return;
  }

  VectorFrame frame(buffer);
  drawMenuBorder(frame);
  frame.centered(-22, "VECTOR ARCADE");
  frame.line(b2Vec2(-17.0f, -14.0f), b2Vec2(17.0f, -14.0f));
  const char *titles[] = {"ASTEROIDS", "LUNAR LANDER", "MISSILE COMMAND"};
  for (int index = 0; index < 3; ++index)
  {
    const float row = -6.0f + index * 10;
    frame.text(-21, row, titles[index]);
    if (selection == index)
    {
      frame.line(b2Vec2(-27, row), b2Vec2(-24, row + 1.5f));
      frame.line(b2Vec2(-24, row + 1.5f), b2Vec2(-27, row + 3));
    }
  }
  frame.finish();
}