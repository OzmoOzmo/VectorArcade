#pragma once

#include "../Arcade/ArcadeGame.hpp"
#include "box2d/box2d.h"
#include <array>
#include <cstdint>

class JoystickControls;
class SoundFX;

class MissileGame : public ArcadeGame
{
#if defined(ARCADE_TESTS)
  friend struct ArcadeTestAccess;
#endif
private:
  struct City
  {
    bool alive = true;
    int ammo = 10;
  };
  struct Missile
  {
    bool active = false;
    bool split = false;
    b2Vec2 origin{0, 0};
    b2Vec2 position{0, 0};
    b2Vec2 target{0, 0};
    int city = 0;
  };
  struct Explosion
  {
    bool active = false;
    bool defensive = true;
    b2Vec2 position{0, 0};
    float age = 0;
  };

  JoystickControls &controls;
  SoundFX *sound;
  std::array<City, 5> cities;
  std::array<Missile, 24> enemies;
  std::array<Missile, 16> defenders;
  std::array<Explosion, 32> explosions;
  b2Vec2 cursor{0, -5};
  int round = 1;
  int score = 0;
  int reserve_cities = 0;
  int next_bonus = 10000;
  int remaining = 12;
  int launch_city = 0;
  int cities_lost = 0;
  float spawn_time = 1;
  float fire_time = 0;
  float intermission = 0;
  int round_bonus = 0;
  bool ended = false;
  uint32_t random_state = 0x12345678;

  static float city_x(int index) { return -24.0f + index * 12.0f; }
  static float radius(const Explosion &explosion);
  static bool move(Missile &missile, float distance);
  static bool intersects(const b2Vec2 &start, const b2Vec2 &end, const b2Vec2 &center, float radius);
  int random_number(int maximum);
  int multiplier() const;
  int survivors() const;
  void award(int points);
  void explode(const b2Vec2 &position, bool defensive = true);
  bool spawn_enemy(const b2Vec2 &origin, int target, bool split);
  void shoot();
  void finish_round();

public:
  explicit MissileGame(JoystickControls &input, SoundFX *effects = nullptr) : controls(input), sound(effects) {}
  void step_world(float elapsed_time) override;
  void render_frame(RenderBuffer &buffer) override;
  bool finished() const override { return ended; }
};