#include "MissileGame.hpp"
#include "../Controls/JoystickControls.hpp"
#include "../Arcade/VectorFrame.hpp"
#include "../Audio/SoundFX.h"
#include "../HardwareConfig.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

float MissileGame::radius(const Explosion &explosion)
{
  return explosion.age < 0.7f ? 8 * explosion.age : std::max(0.0f, 5.6f - 4 * (explosion.age - 0.7f));
}

bool MissileGame::move(Missile &missile, float distance)
{
  const b2Vec2 offset = missile.target - missile.position;
  const float length = offset.Length();
  if (length <= distance)
  {
    missile.position = missile.target;
    return true;
  }
  missile.position += (distance / length) * offset;
  return false;
}

bool MissileGame::intersects(const b2Vec2 &start, const b2Vec2 &end, const b2Vec2 &center, float blast_radius)
{
  const b2Vec2 segment = end - start;
  const float length_squared = segment.LengthSquared();
  const float fraction = length_squared == 0 ? 0 :
      std::max(0.0f, std::min(1.0f, b2Dot(center - start, segment) / length_squared));
  return (center - (start + fraction * segment)).LengthSquared() <= blast_radius * blast_radius;
}

int MissileGame::random_number(int maximum)
{
  random_state = 1664525 * random_state + 1013904223;
  return static_cast<int>((random_state >> 8) % maximum);
}

int MissileGame::multiplier() const
{
  return std::min(6, 1 + (round - 1) / 2);
}

int MissileGame::survivors() const
{
  int alive = 0;
  for (const auto &city : cities)
  {
    alive += city.alive ? 1 : 0;
  }
  return alive;
}

void MissileGame::award(int points)
{
  score += points;
  while (score >= next_bonus)
  {
    ++reserve_cities;
    next_bonus += 10000;
  }
}

void MissileGame::explode(const b2Vec2 &position, bool defensive)
{
  if (sound)
  {
    sound->bang_large();
  }
  Explosion *oldest = &explosions[0];
  for (auto &explosion : explosions)
  {
    if (!explosion.active)
    {
      explosion.active = true;
      explosion.defensive = defensive;
      explosion.position = position;
      explosion.age = 0;
      return;
    }
    if (explosion.age > oldest->age)
    {
      oldest = &explosion;
    }
  }
  oldest->position = position;
  oldest->defensive = defensive;
  oldest->age = 0;
}

bool MissileGame::spawn_enemy(const b2Vec2 &origin, int target, bool split)
{
  for (auto &enemy : enemies)
  {
    if (!enemy.active)
    {
      enemy.active = true;
      enemy.split = split;
      enemy.origin = origin;
      enemy.position = origin;
      enemy.target.Set(city_x(target), 24);
      enemy.city = target;
      return true;
    }
  }
  return false;
}

void MissileGame::shoot()
{
  for (auto &defender : defenders)
  {
    if (defender.active)
    {
      continue;
    }
    for (int offset = 0; offset < 5; ++offset)
    {
      const int index = (launch_city + offset) % 5;
      if (cities[index].alive && cities[index].ammo > 0)
      {
        defender.active = true;
        defender.origin.Set(city_x(index), 22);
        defender.position = defender.origin;
        defender.target = cursor;
        --cities[index].ammo;
        launch_city = (index + 1) % 5;
        fire_time = 0.25f;
        if (sound)
        {
          sound->fire();
        }
        return;
      }
    }
    return;
  }
}

void MissileGame::finish_round()
{
  int ammunition = 0;
  for (const auto &city : cities)
  {
    if (city.alive)
    {
      ammunition += city.ammo;
    }
  }
  round_bonus = multiplier() * (100 * survivors() + 5 * ammunition);
  award(round_bonus);
  intermission = 4;
}

void MissileGame::step_world(float elapsed_time)
{
  if (ended)
  {
    return;
  }
  const float delta = std::min(elapsed_time, 0.05f);
  if (intermission > 0)
  {
    intermission -= delta;
    if (intermission <= 0)
    {
      for (auto &city : cities)
      {
        if (!city.alive && reserve_cities > 0)
        {
          city.alive = true;
          --reserve_cities;
        }
        city.ammo = city.alive ? 10 : 0;
      }
      ended = survivors() == 0;
      ++round;
      cities_lost = 0;
      remaining = std::min(60, 10 + round * 2);
      spawn_time = 1;
    }
    return;
  }

  cursor.x = std::max(-28.0f, std::min(28.0f, cursor.x + controls.x() * 26 * delta));
  cursor.y = std::max(-24.0f, std::min(18.0f, cursor.y + controls.y() * 26 * delta));
  fire_time = std::max(0.0f, fire_time - delta);
  if (controls.is_firing() && fire_time == 0)
  {
    shoot();
  }
  spawn_time -= delta;
  if (remaining > 0 && spawn_time <= 0)
  {
    if (spawn_enemy(b2Vec2(random_number(55) - 27, -24), random_number(5), round >= 3 && remaining % 4 == 0))
    {
      --remaining;
      spawn_time = std::max(0.2f, 1.4f - round * 0.08f);
    }
  }

  for (auto &explosion : explosions)
  {
    if (explosion.active)
    {
      explosion.age += delta;
      explosion.active = explosion.age < 2.1f;
    }
  }
  for (auto &defender : defenders)
  {
    if (defender.active && move(defender, 34 * delta))
    {
      defender.active = false;
      explode(defender.target);
    }
  }
  for (auto &enemy : enemies)
  {
    if (!enemy.active)
    {
      continue;
    }
    const b2Vec2 previous = enemy.position;
    const bool reached = move(enemy, std::min(16.0f, 3.0f + round * 0.6f) * delta);
    for (const auto &explosion : explosions)
    {
      if (explosion.active && explosion.defensive && intersects(previous, enemy.position, explosion.position, radius(explosion)))
      {
        enemy.active = false;
        award(25 * multiplier());
        explode(enemy.position);
        break;
      }
    }
    if (!enemy.active)
    {
      continue;
    }
    if (enemy.split && enemy.position.y >= -4)
    {
      enemy.split = false;
      spawn_enemy(enemy.position, (enemy.city + 2) % 5, false);
    }
    if (reached)
    {
      enemy.active = false;
      if (cities[enemy.city].alive && cities_lost < 3)
      {
        cities[enemy.city].alive = false;
        cities[enemy.city].ammo = 0;
        ++cities_lost;
        explode(enemy.target, false);
      }
    }
  }

  bool busy = remaining > 0;
  for (const auto &enemy : enemies)
  {
    busy = busy || enemy.active;
  }
  for (const auto &defender : defenders)
  {
    busy = busy || defender.active;
  }
  for (const auto &explosion : explosions)
  {
    busy = busy || explosion.active;
  }
  if (!busy)
  {
    finish_round();
  }
}

void MissileGame::render_frame(RenderBuffer &buffer)
{
  if (!buffer.needs_render)
  {
    return;
  }
  VectorFrame frame(buffer);
  char status[48];
  snprintf(status, sizeof(status), "S %d R %d", score, round);
  frame.text(-28, -29, status);
  for (int index = 0; index < 5; ++index)
  {
    if (!cities[index].alive)
    {
      continue;
    }
    const float center = city_x(index);
    frame.line(b2Vec2(center - 2.5f, 25), b2Vec2(center - 2.5f, 22));
    frame.line(b2Vec2(center - 2.5f, 22), b2Vec2(center - 0.5f, 22));
    frame.line(b2Vec2(center - 0.5f, 22), b2Vec2(center - 0.5f, 20.5f));
    frame.line(b2Vec2(center - 0.5f, 20.5f), b2Vec2(center + 1.5f, 20.5f));
    frame.line(b2Vec2(center + 1.5f, 20.5f), b2Vec2(center + 2.5f, 25));
    frame.line(b2Vec2(center + 2.5f, 25), b2Vec2(center - 2.5f, 25));
    snprintf(status, sizeof(status), "%d", cities[index].ammo);
    frame.text(center - 1.5f, 27, status);
  }
  if (ended || intermission > 0)
  {
    frame.centered(-12, ended ? "THE END" : "WAVE CLEAR");
    snprintf(status, sizeof(status), "CITIES %d", survivors());
    frame.centered(-5, status);
    snprintf(status, sizeof(status), "BONUS %d", round_bonus);
    frame.centered(2, status);
    snprintf(status, sizeof(status), "RESERVE %d", reserve_cities);
    frame.centered(9, status);
  }
  else
  {
    for (const auto &enemy : enemies)
    {
      if (enemy.active)
      {
        frame.line(enemy.origin, enemy.position);
      }
    }
    for (const auto &defender : defenders)
    {
      if (defender.active)
      {
        frame.point(defender.position, HardwareConfig::kBulletExtraDwellUs);
      }
    }
    for (const auto &explosion : explosions)
    {
      if (explosion.active)
      {
        const float blast_radius = radius(explosion);
        if (explosion.defensive)
        {
          frame.circle(explosion.position, blast_radius);
        }
        else
        {
          constexpr int vertex_count = 16;
          const b2Vec2 first = explosion.position + b2Vec2(0, -blast_radius);
          b2Vec2 previous = first;
          for (int vertex = 1; vertex <= vertex_count; ++vertex)
          {
            const float angle = -0.5f * b2_pi + vertex * (2.0f * b2_pi / vertex_count);
            const float spike_radius = blast_radius * (vertex % 2 == 0 ? 1.0f : 0.4f);
            const b2Vec2 next = vertex == vertex_count ? first :
                explosion.position + spike_radius * b2Vec2(cosf(angle), sinf(angle));
            frame.line(previous, next);
            previous = next;
          }
        }
      }
    }
    frame.line(cursor + b2Vec2(-1.2f, 0), cursor + b2Vec2(1.2f, 0));
    frame.line(cursor + b2Vec2(0, -1.2f), cursor + b2Vec2(0, 1.2f));
  }
  frame.finish();
}