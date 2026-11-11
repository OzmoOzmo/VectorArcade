#pragma once

#include "../Arcade/ArcadeGame.hpp"
#include "box2d/box2d.h"
#include <array>

class JoystickControls;
class SoundFX;

class LunarGame : public ArcadeGame
{
#if defined(ARCADE_TESTS)
  friend struct ArcadeTestAccess;
#endif
private:
  JoystickControls &controls;
  SoundFX *sound;
  b2World world{b2Vec2(0, 1.62f)};
  b2Body *craft = nullptr;
  b2Body *ground = nullptr;
  std::array<b2Vec2, 13> terrain;
  float fuel = 500;
  float result_time = 0;
  float thrust_sound_time = 0;
  float angle = 0;
  bool landed = false;
  bool ended = false;
  bool burning = false;
  int score = 0;
  int stage = 0;

  void new_attempt();

public:
  explicit LunarGame(JoystickControls &input, SoundFX *effects = nullptr);
  void step_world(float elapsed_time) override;
  void render_frame(RenderBuffer &buffer) override;
  bool finished() const override { return ended; }
};