#pragma once

#include "ArcadeGame.hpp"
#include <memory>

class JoystickControls;
class SoundFX;

class GameMenu : public ArcadeGame
{
#if defined(ARCADE_TESTS)
  friend struct ArcadeTestAccess;
#endif
private:
  JoystickControls &controls;
  SoundFX *sound;
  std::unique_ptr<ArcadeGame> active;
  int selection = 0;
  float navigation_delay = 0;
  float result_delay = 0;
  bool waiting_for_release = true;
  bool previous_fire = false;
  float inactivity_seconds = 0;
  float screensaver_seconds = 0;
  bool screensaver_active = false;
  bool waiting_for_neutral = false;

  void launch();
  bool updateScreensaver(float elapsed_time);
  void renderScreensaver(RenderBuffer &buffer);

public:
  GameMenu(JoystickControls &input, SoundFX *effects);
  void step_world(float elapsed_time) override;
  void render_frame(RenderBuffer &buffer) override;
};