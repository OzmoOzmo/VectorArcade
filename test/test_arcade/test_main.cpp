#include <Arduino.h>
#include <unity.h>
#include "Controls/JoystickControls.hpp"
#include "Arcade/GameMenu.hpp"
#include "Arcade/VectorFrame.hpp"
#include "Game_Lunar/LunarGame.hpp"
#include "Game_Missile/MissileGame.hpp"
#include "Fonts/SimpleFont.hpp"
#include "Rendering/RenderBuffer.hpp"
#include "Rendering/SPIRendererDMA.h"
#include "Audio/SoundFX.h"

struct ArcadeTestAccess
{
  static void buffered_spi_points()
  {
    SimpleFont font;
    SPIRendererDMA renderer(30, &font);
    renderer.pending = &renderer.blocks[0];
    renderer.pending->count = 0;
    int16_t current_x = 2048;
    int16_t current_y = 2048;
    renderer.prepareInstruction({2048, 2048, 0, false}, current_x, current_y);
    renderer.prepareInstruction({2049, 2048, 0, true}, current_x, current_y);
    renderer.prepareInstruction({2049, 2049, 0, true}, current_x, current_y);
    renderer.prepareInstruction({2050, 2049, 0, true}, current_x, current_y);
    renderer.prepareInstruction({2050, 2049, 0, false}, current_x, current_y);
    renderer.prepareInstruction({2050, 2049, 0, true}, current_x, current_y);
    renderer.prepareInstruction({2050, 2049, 0, true}, current_x, current_y);
    TEST_ASSERT_EQUAL(7, renderer.pending->count);
    const uint16_t expected_x[] = {2048, 2049, 2049, 2050};
    const uint16_t expected_y[] = {2048, 2048, 2049, 2049};
    size_t coordinate_count = 0;
    for (size_t index = 0; index < renderer.pending->count; ++index)
    {
      const auto &sample = renderer.pending->samples[index];
      TEST_ASSERT_EQUAL(0, sample.extra_dwell_us);
      TEST_ASSERT_EQUAL_HEX8(0x50, sample.commands[0] & 0xF0);
      TEST_ASSERT_EQUAL_HEX8(0xD0, sample.commands[2] & 0xF0);
      if (sample.write_coordinates)
      {
        TEST_ASSERT_LESS_THAN(4, coordinate_count);
        const uint16_t x = ((sample.commands[0] & 0x0F) << 8) | sample.commands[1];
        const uint16_t y = 4096 - (((sample.commands[2] & 0x0F) << 8) | sample.commands[3]);
        TEST_ASSERT_EQUAL(expected_x[coordinate_count], x);
        TEST_ASSERT_EQUAL(expected_y[coordinate_count], y);
        ++coordinate_count;
      }
    }
    TEST_ASSERT_EQUAL(4, coordinate_count);
    TEST_ASSERT_FALSE(renderer.pending->samples[5].laser);
    TEST_ASSERT_TRUE(renderer.pending->samples[6].laser);
    TEST_ASSERT_FALSE(renderer.pending->samples[6].write_coordinates);
    renderer.prepareInstruction({2050, 2049, 0, true, 30}, current_x, current_y);
    TEST_ASSERT_EQUAL(8, renderer.pending->count);
    TEST_ASSERT_TRUE(renderer.pending->samples[7].laser);
    TEST_ASSERT_FALSE(renderer.pending->samples[7].write_coordinates);
    TEST_ASSERT_EQUAL(30, renderer.pending->samples[7].extra_dwell_us);
    renderer.prepareInstruction({2050, 2049, 0, false, 30}, current_x, current_y);
    TEST_ASSERT_EQUAL(0, renderer.pending->samples[8].extra_dwell_us);

    auto *buffer = renderer.get_render_buffer();
    VectorFrame frame(*buffer);
    const b2Vec2 bullet_position(1, 2);
    frame.point(bullet_position, HardwareConfig::kBulletExtraDwellUs);
    TEST_ASSERT_EQUAL(2, buffer->drawing_frame->size());
    TEST_ASSERT_FALSE(buffer->drawing_frame->at(0).laser);
    TEST_ASSERT_EQUAL(0, buffer->drawing_frame->at(0).extra_dwell_us);
    TEST_ASSERT_TRUE(buffer->drawing_frame->at(1).laser);
    TEST_ASSERT_EQUAL(HardwareConfig::kBulletExtraDwellUs, buffer->drawing_frame->at(1).extra_dwell_us);
    frame.line(bullet_position, b2Vec2(2, 2));
    TEST_ASSERT_EQUAL(0, buffer->drawing_frame->back().extra_dwell_us);
    delete buffer->display_frame;
    delete buffer->drawing_frame;
    delete buffer;
  }

  static void screensaver_timeout_motion_and_wakeup()
  {
    if (HardwareConfig::kScreenSaverTimeoutSeconds == 0)
    {
      return;
    }
    JoystickControls controls;
    SoundFX sound(nullptr);
    SimpleFont font;
    GameMenu menu(controls, &sound);
    menu.selection = 2;
    menu.launch();
    auto *active_game = menu.active.get();
    const float timeout = HardwareConfig::kScreenSaverTimeoutSeconds;
    TEST_ASSERT_FALSE(menu.updateScreensaver(timeout - 0.1f));
    controls.axis_x = 1;
    TEST_ASSERT_FALSE(menu.updateScreensaver(0.2f));
    TEST_ASSERT_EQUAL_FLOAT(0, menu.inactivity_seconds);
    controls.axis_x = 0;
    TEST_ASSERT_FALSE(menu.updateScreensaver(timeout - 0.1f));
    TEST_ASSERT_TRUE(menu.updateScreensaver(0.2f));
    TEST_ASSERT_TRUE(menu.screensaver_active);

    RenderBuffer buffer(0, 255, 0, 255, 128, 128, 128.0f / 30, &font);
    buffer.needs_render = true;
    menu.render_frame(buffer);
    TEST_ASSERT_FALSE(buffer.needs_render);
    TEST_ASSERT_FALSE(buffer.drawing_frame->empty());
    const auto first_frame = *buffer.drawing_frame;
    menu.updateScreensaver(1);
    buffer.needs_render = true;
    menu.render_frame(buffer);
    bool moved = first_frame.size() != buffer.drawing_frame->size();
    for (size_t index = 0; index < buffer.drawing_frame->size(); ++index)
    {
      const auto &instruction = buffer.drawing_frame->at(index);
      TEST_ASSERT_INT_WITHIN(128, 127, instruction.x);
      TEST_ASSERT_INT_WITHIN(128, 127, instruction.y);
      if (index < first_frame.size())
      {
        moved = moved || instruction.x != first_frame[index].x || instruction.y != first_frame[index].y;
      }
    }
    TEST_ASSERT_TRUE(moved);

    controls.fire = true;
    TEST_ASSERT_TRUE(menu.updateScreensaver(0.01f));
    TEST_ASSERT_FALSE(menu.screensaver_active);
    TEST_ASSERT_TRUE(menu.waiting_for_neutral);
    TEST_ASSERT_TRUE(menu.previous_fire);
    TEST_ASSERT_EQUAL_PTR(active_game, menu.active.get());
    TEST_ASSERT_TRUE(menu.updateScreensaver(0.01f));
    controls.fire = false;
    TEST_ASSERT_TRUE(menu.updateScreensaver(0.01f));
    TEST_ASSERT_FALSE(menu.waiting_for_neutral);
    TEST_ASSERT_FALSE(menu.previous_fire);
    TEST_ASSERT_FALSE(menu.updateScreensaver(0.01f));

    TEST_ASSERT_TRUE(menu.updateScreensaver(timeout));
    controls.button_b = true;
    TEST_ASSERT_TRUE(menu.updateScreensaver(0.01f));
    TEST_ASSERT_FALSE(menu.screensaver_active);
    controls.button_b = false;
    menu.updateScreensaver(0.01f);
    menu.updateScreensaver(timeout);
    controls.axis_y = 0.01f;
    TEST_ASSERT_TRUE(menu.updateScreensaver(0.01f));
    TEST_ASSERT_FALSE(menu.screensaver_active);
    TEST_ASSERT_TRUE(menu.waiting_for_neutral);
    delete buffer.display_frame;
    delete buffer.drawing_frame;
  }

  static void joystick_dead_zone()
  {
    TEST_ASSERT_EQUAL_FLOAT(0, JoystickControls::normalize(2048, 2048));
    TEST_ASSERT_EQUAL_FLOAT(0, JoystickControls::normalize(2200, 2048));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1, JoystickControls::normalize(0, 2048));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1, JoystickControls::normalize(4095, 2048));
  }

  static void menu_and_game_frames()
  {
    JoystickControls controls;
    SoundFX sound(nullptr);
    SimpleFont font;
    RenderBuffer buffer(0, 255, 0, 255, 128, 128, 128.0f / 30, &font);
    GameMenu menu(controls, &sound);
    buffer.needs_render = true;
    menu.render_frame(buffer);
    TEST_ASSERT_FALSE(buffer.needs_render);
    TEST_ASSERT_GREATER_THAN(20, buffer.drawing_frame->size());
    for (int selection = 0; selection < 3; ++selection)
    {
      menu.selection = selection;
      menu.launch();
      TEST_ASSERT_NOT_NULL(menu.active.get());
      TEST_ASSERT_TRUE(menu.waiting_for_release);
      buffer.needs_render = true;
      menu.render_frame(buffer);
      TEST_ASSERT_FALSE(buffer.needs_render);
      TEST_ASSERT_FALSE(buffer.drawing_frame->empty());
      for (const auto &instruction : *buffer.drawing_frame)
      {
        TEST_ASSERT_INT_WITHIN(128, 127, instruction.x);
        TEST_ASSERT_INT_WITHIN(128, 127, instruction.y);
      }
    }
    delete buffer.display_frame;
    delete buffer.drawing_frame;
  }

  static void lander_thrust_and_direction()
  {
    JoystickControls controls;
    controls.axis_x = -1;
    controls.axis_y = -1;
    controls.direction = -b2_pi / 4;
    controls.fire = true;
    LunarGame game(controls);
    game.craft->SetLinearVelocity(b2Vec2(0, 0));
    game.step_world(1.0f / 60);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -b2_pi / 4, game.angle);
    TEST_ASSERT_TRUE(game.craft->GetLinearVelocity().x < 0);
    TEST_ASSERT_TRUE(game.craft->GetLinearVelocity().y < 0);
    TEST_ASSERT_TRUE(game.fuel < 500);
    controls.fire = false;
    controls.button_b = true;
    const float fuel = game.fuel;
    game.step_world(1.0f / 60);
    TEST_ASSERT_EQUAL_FLOAT(fuel, game.fuel);
    TEST_ASSERT_FALSE(game.burning);
  }

  static void lander_safe_and_unsafe_touchdown()
  {
    JoystickControls controls;
    LunarGame safe(controls);
    safe.craft->SetTransform(b2Vec2(-15, 19.4f), 0);
    safe.craft->SetLinearVelocity(b2Vec2(0, 0.1f));
    safe.step_world(1.0f / 60);
    TEST_ASSERT_TRUE(safe.landed);
    TEST_ASSERT_EQUAL(200, safe.score);
    TEST_ASSERT_TRUE(safe.result_time > 0);

    LunarGame unsafe(controls);
    unsafe.craft->SetTransform(b2Vec2(-15, 19.4f), 0);
    unsafe.craft->SetLinearVelocity(b2Vec2(0, 5));
    unsafe.step_world(1.0f / 60);
    TEST_ASSERT_FALSE(unsafe.landed);
    TEST_ASSERT_EQUAL_FLOAT(400, unsafe.fuel);
  }

  static void missile_launch_and_chain_reaction()
  {
    JoystickControls controls;
    MissileGame game(controls);
    game.cities[0].alive = false;
    game.shoot();
    TEST_ASSERT_TRUE(game.defenders[0].active);
    TEST_ASSERT_EQUAL_FLOAT(-12, game.defenders[0].origin.x);
    TEST_ASSERT_EQUAL(9, game.cities[1].ammo);
    TEST_ASSERT_EQUAL_FLOAT(-5, game.defenders[0].target.y);
    game.spawn_enemy(b2Vec2(0, -10), 2, false);
    game.explode(b2Vec2(0, -10));
    game.explosions[0].age = 0.5f;
    game.step_world(1.0f / 60);
    TEST_ASSERT_FALSE(game.enemies[0].active);
    TEST_ASSERT_EQUAL(25, game.score);
    TEST_ASSERT_TRUE(game.explosions[1].active);
  }

  static void missile_city_destruction_explosion()
  {
    JoystickControls controls;
    MissileGame game(controls);
    game.spawn_enemy(b2Vec2(-24, 24), 0, false);
    game.step_world(1.0f / 60);
    TEST_ASSERT_FALSE(game.cities[0].alive);
    TEST_ASSERT_EQUAL(0, game.cities[0].ammo);
    TEST_ASSERT_TRUE(game.explosions[0].active);
    TEST_ASSERT_FALSE(game.explosions[0].defensive);
    TEST_ASSERT_EQUAL_FLOAT(-24, game.explosions[0].position.x);
    TEST_ASSERT_EQUAL_FLOAT(24, game.explosions[0].position.y);

    game.explosions[0].age = 0.5f;
    game.spawn_enemy(b2Vec2(-24, 24), 1, false);
    game.step_world(1.0f / 60);
    TEST_ASSERT_TRUE(game.enemies[0].active);
    TEST_ASSERT_EQUAL(0, game.score);
  }

  static void missile_survivors_and_bonus_cities()
  {
    JoystickControls controls;
    MissileGame game(controls);
    game.cities[0].alive = false;
    game.cities[0].ammo = 0;
    game.finish_round();
    TEST_ASSERT_EQUAL(600, game.round_bonus);
    game.intermission = 0.001f;
    game.step_world(1.0f / 60);
    TEST_ASSERT_FALSE(game.cities[0].alive);
    TEST_ASSERT_EQUAL(2, game.round);

    game.award(20000);
    TEST_ASSERT_EQUAL(2, game.reserve_cities);
    game.finish_round();
    game.intermission = 0.001f;
    game.step_world(1.0f / 60);
    TEST_ASSERT_TRUE(game.cities[0].alive);
    TEST_ASSERT_EQUAL(1, game.reserve_cities);
    TEST_ASSERT_EQUAL(10, game.cities[0].ammo);
  }

  static void missile_game_over()
  {
    JoystickControls controls;
    MissileGame game(controls);
    for (auto &city : game.cities)
    {
      city.alive = false;
      city.ammo = 0;
    }
    game.finish_round();
    game.intermission = 0.001f;
    game.step_world(1.0f / 60);
    TEST_ASSERT_TRUE(game.finished());
  }
};

void setUp() {}
void tearDown() {}

void setup()
{
  delay(1000);
  UNITY_BEGIN();
  RUN_TEST(ArcadeTestAccess::buffered_spi_points);
  RUN_TEST(ArcadeTestAccess::joystick_dead_zone);
  RUN_TEST(ArcadeTestAccess::screensaver_timeout_motion_and_wakeup);
  RUN_TEST(ArcadeTestAccess::menu_and_game_frames);
  RUN_TEST(ArcadeTestAccess::lander_thrust_and_direction);
  RUN_TEST(ArcadeTestAccess::lander_safe_and_unsafe_touchdown);
  RUN_TEST(ArcadeTestAccess::missile_launch_and_chain_reaction);
  RUN_TEST(ArcadeTestAccess::missile_city_destruction_explosion);
  RUN_TEST(ArcadeTestAccess::missile_survivors_and_bonus_cities);
  RUN_TEST(ArcadeTestAccess::missile_game_over);
  UNITY_END();
}

void loop() {}