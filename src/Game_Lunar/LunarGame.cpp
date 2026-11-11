#include "LunarGame.hpp"
#include "../Controls/JoystickControls.hpp"
#include "../Arcade/VectorFrame.hpp"
#include "../Audio/SoundFX.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

LunarGame::LunarGame(JoystickControls &input, SoundFX *effects) : controls(input), sound(effects)
{
  new_attempt();
}

void LunarGame::new_attempt()
{
  if (craft)
  {
    world.DestroyBody(craft);
    world.DestroyBody(ground);
  }
  const float offset = stage % 2 == 0 ? 0 : -2;
  terrain = {{b2Vec2(-30, 19), b2Vec2(-24, 16), b2Vec2(-20, 21),
              b2Vec2(-10, 21), b2Vec2(-5, 17), b2Vec2(0, 23),
              b2Vec2(6, 19), b2Vec2(10, 24), b2Vec2(17, 24),
              b2Vec2(21, 18), b2Vec2(25, 22), b2Vec2(28, 20), b2Vec2(30, 21)}};
  b2BodyDef ground_definition;
  ground = world.CreateBody(&ground_definition);
  for (auto &vertex : terrain)
  {
    vertex.y += offset;
  }
  for (size_t index = 1; index < terrain.size(); ++index)
  {
    b2EdgeShape edge;
    edge.SetTwoSided(terrain[index - 1], terrain[index]);
    ground->CreateFixture(&edge, 0);
  }
  b2BodyDef definition;
  definition.type = b2_dynamicBody;
  definition.position.Set(-21, -16);
  definition.linearVelocity.Set(2, 0);
  definition.fixedRotation = true;
  definition.bullet = true;
  craft = world.CreateBody(&definition);
  b2PolygonShape hull;
  hull.SetAsBox(1.1f, 1.7f);
  craft->CreateFixture(&hull, 1);
  angle = 0;
  burning = false;
  result_time = 0;
}

void LunarGame::step_world(float elapsed_time)
{
  if (ended)
  {
    return;
  }
  const float delta = std::min(elapsed_time, 0.05f);
  if (result_time > 0)
  {
    thrust_sound_time = 0;
    result_time -= delta;
    if (result_time <= 0)
    {
      if (fuel <= 0)
      {
        ended = true;
      }
      else
      {
        ++stage;
        new_attempt();
      }
    }
    return;
  }

  if (controls.x() != 0 || controls.y() != 0)
  {
    angle = controls.get_direction();
  }
  craft->SetTransform(craft->GetPosition(), angle);
  burning = controls.is_firing() && fuel > 0;
  if (burning)
  {
    if (thrust_sound_time <= 0)
    {
      if (sound)
      {
        sound->thrust();
      }
      thrust_sound_time = 0.3f;
    }
    thrust_sound_time = std::max(0.0f, thrust_sound_time - delta);
    const float powered_delta = std::min(delta, fuel / 12.0f);
    craft->ApplyLinearImpulseToCenter(
        craft->GetMass() * 4.6f * powered_delta * b2Vec2(sinf(angle), -cosf(angle)), true);
    fuel = std::max(0.0f, fuel - 12 * powered_delta);
  }
  else
  {
    thrust_sound_time = 0;
  }
  const b2Vec2 impact_velocity = craft->GetLinearVelocity();
  world.Step(delta, 6, 2);
  const b2Vec2 position = craft->GetPosition();
  bool contact = position.x < -29 || position.x > 29 || position.y > 29;
  for (b2Contact *collision = world.GetContactList(); collision; collision = collision->GetNext())
  {
    contact = contact || collision->IsTouching();
  }
  if (position.y < -24)
  {
    craft->SetTransform(b2Vec2(position.x, -24), angle);
    craft->SetLinearVelocity(b2Vec2(impact_velocity.x, std::max(0.0f, impact_velocity.y)));
  }
  if (!contact)
  {
    return;
  }

  landed = false;
  for (size_t index = 1; index < terrain.size(); ++index)
  {
    const b2Vec2 &left = terrain[index - 1];
    const b2Vec2 &right = terrain[index];
    if (left.y == right.y && position.x - 1.5f >= left.x && position.x + 1.5f <= right.x &&
        fabsf(position.y + 1.7f - left.y) < 1.0f && fabsf(angle) <= 0.22f &&
        fabsf(impact_velocity.x) <= 1.2f && fabsf(impact_velocity.y) <= 2.4f)
    {
      landed = true;
      score += (right.x - left.x < 8 ? 500 : 200);
      fuel = std::min(600.0f, fuel + 75);
      break;
    }
  }
  if (!landed)
  {
    fuel = std::max(0.0f, fuel - 100);
  }
  else if (sound)
  {
    sound->success();
  }
  if (!landed && sound)
  {
    sound->bang_large();
  }
  burning = false;
  thrust_sound_time = 0;
  result_time = 3;
}

void LunarGame::render_frame(RenderBuffer &buffer)
{
  if (!buffer.needs_render)
  {
    return;
  }
  VectorFrame frame(buffer);
  char status[48];
  snprintf(status, sizeof(status), "S %d F %d", score, static_cast<int>(fuel));
  frame.text(-28, -29, status);
  const b2Vec2 velocity = craft->GetLinearVelocity();
  snprintf(status, sizeof(status), "VX %d VY %d", static_cast<int>(velocity.x * 10), static_cast<int>(velocity.y * 10));
  frame.text(-28, -24, status);
  for (size_t index = 1; index < terrain.size(); ++index)
  {
    frame.line(terrain[index - 1], terrain[index]);
    if (terrain[index - 1].y == terrain[index].y)
    {
      frame.text(terrain[index - 1].x + 1, terrain[index].y + 1,
                 terrain[index].x - terrain[index - 1].x < 8 ? "5X" : "2X");
    }
  }
  const b2Vec2 position = craft->GetPosition();
  const b2Rot rotation(angle);
  const b2Vec2 outline[] = {{-1.1f, 0.5f}, {-1, -0.9f}, {0, -1.6f},
                            {1, -0.9f}, {1.1f, 0.5f}, {-1.1f, 0.5f}};
  for (size_t index = 1; index < sizeof(outline) / sizeof(outline[0]); ++index)
  {
    frame.line(position + b2Mul(rotation, outline[index - 1]), position + b2Mul(rotation, outline[index]));
  }
  frame.line(position + b2Mul(rotation, b2Vec2(-0.8f, 0.5f)), position + b2Mul(rotation, b2Vec2(-1.1f, 1.7f)));
  frame.line(position + b2Mul(rotation, b2Vec2(0.8f, 0.5f)), position + b2Mul(rotation, b2Vec2(1.1f, 1.7f)));
  if (burning)
  {
    frame.line(position + b2Mul(rotation, b2Vec2(-0.5f, 0.5f)), position + b2Mul(rotation, b2Vec2(0, 3)));
    frame.line(position + b2Mul(rotation, b2Vec2(0, 3)), position + b2Mul(rotation, b2Vec2(0.5f, 0.5f)));
  }
  if (ended)
  {
    frame.centered(-5, "OUT OF FUEL");
  }
  else if (result_time > 0)
  {
    frame.centered(-5, landed ? "LANDED" : "CRASH");
  }
  frame.finish();
}