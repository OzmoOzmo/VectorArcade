#include <Arduino.h>
#include "esp32-hal-cpu.h"
#include "HardwareConfig.h"
#if defined(ASTEROIDS_CALIBRATION_DIAGNOSTICS) && ASTEROIDS_CALIBRATION_DIAGNOSTICS
#include "CalibrationDiagnostics.hpp"
#endif
#if ASTEROIDS_ENABLE_AUDIO
#include <SPIFFS.h>
#include "Audio/I2SOutput.h"
#endif
#include "driver/dac.h"
#include "Audio/SoundFX.h"
#include "Controls/JoystickControls.hpp"
#include "Arcade/GameMenu.hpp"
#if ASTEROIDS_SPI_BENCHMARK
#include "Game_Missile/MissileGame.hpp"
#endif
#include "Fonts/SimpleFont.hpp"
#include "FastDAC.h"
#include "Game_Astroid/GameLoop.h"
#include "Rendering/DACRenderer.h"
#include "Rendering/RenderBuffer.hpp"
#include "Rendering/HeltecOLEDRenderer.hpp"
#include "Rendering/SPIRenderer.h"
#include "Rendering/SPIRendererDMA.h"

namespace
{
constexpr float kWorldSize = 30.0f;
GameLoop *gameLoop = nullptr;
Renderer *renderer = nullptr;

void runDacBenchmark()
{
  constexpr uint32_t sampleCount = 5000000;
  ESP_ERROR_CHECK(dac_output_enable(DAC_CHANNEL_1));
  ESP_ERROR_CHECK(dac_output_enable(DAC_CHANNEL_2));
  DACPrepare(false);

  uint32_t started = micros();
  for (uint32_t sample = 0; sample < sampleCount; ++sample)
  {
    DAC1Write(sample);
  }
  uint32_t elapsedUs = micros() - started;
  Serial.printf("DAC1 direct-register throughput: %.0f samples/s\n",
                sampleCount * 1000000.0f / elapsedUs);

  started = micros();
  for (uint32_t sample = 0; sample < sampleCount; ++sample)
  {
    DAC2Write(sample);
  }
  elapsedUs = micros() - started;
  Serial.printf("DAC2 direct-register throughput: %.0f samples/s\n",
                sampleCount * 1000000.0f / elapsedUs);
}
}

void setup()
{
  const uint32_t requestedCpuMhz = 240 * HardwareConfig::kCpuSpeedPercent / 100;
  const uint32_t supportedCpuMhz = requestedCpuMhz >= 240 ? 240 : requestedCpuMhz >= 160 ? 160 : 80;
  const bool clockConfigured = setCpuFrequencyMhz(supportedCpuMhz);
  Serial.begin(115200);
  delay(250);
  Serial.printf("CPU requested=%lu%% (%lu MHz), actual=%lu MHz, APB=%lu Hz%s\n",
                static_cast<unsigned long>(HardwareConfig::kCpuSpeedPercent),
                static_cast<unsigned long>(requestedCpuMhz),
                static_cast<unsigned long>(getCpuFrequencyMhz()),
                static_cast<unsigned long>(getApbFrequency()),
                clockConfigured ? "" : " (clock change rejected)");
  if (requestedCpuMhz != supportedCpuMhz)
  {
    Serial.println("CPU request limited to supported 80/160/240 MHz settings; maximum is 100% (240 MHz)");
  }

#if defined(ASTEROIDS_CALIBRATION_DIAGNOSTICS) && ASTEROIDS_CALIBRATION_DIAGNOSTICS
  JoystickControls *controls = new JoystickControls();
  controls->begin();
  Serial.printf("CALIBRATION pins: joystick X=GPIO%d Y=GPIO%d, fire=GPIO%d, thrust=GPIO%d, DAC X=GPIO25 DAC Y=GPIO26\n",
                HardwareConfig::kJoystickXPin,
                HardwareConfig::kJoystickYPin,
                HardwareConfig::kFirePin,
                HardwareConfig::kThrustPin);
  Serial.println("Move stick to every physical extreme; X and Y ADC min/max are accumulated. X changes square width, Y changes height; fire/thrust only report button state.");
  SimpleFont *font = new SimpleFont();
  renderer = new DACRenderer(kWorldSize, font);
  CalibrationDiagnostics *diagnostics = new CalibrationDiagnostics(*controls);
  RenderBuffer *buffer = renderer->get_render_buffer();
  buffer->needs_render = true;
  diagnostics->render_frame(*buffer);
  buffer->swapBuffers();
  renderer->start();
  gameLoop = new GameLoop(diagnostics, buffer);
  gameLoop->start();
  Serial.println("Joystick/DAC calibration diagnostics running");
  return;
#else

#if ASTEROIDS_ENABLE_AUDIO
  if (!SPIFFS.begin())
  {
    Serial.println("SPIFFS mount failed; audio assets may be unavailable");
  }
#endif

#if ASTEROIDS_DAC_BENCHMARK
  runDacBenchmark();
#endif

#if ASTEROIDS_ENABLE_AUDIO
  I2SOutput *i2sOutput = new I2SOutput();
  i2sOutput->start();
  SoundFX *soundFx = new SoundFX(i2sOutput);
#else
  SoundFX *soundFx = new SoundFX(nullptr);
#endif

  JoystickControls *controls = new JoystickControls();
  controls->begin();
#if ASTEROIDS_SPI_BENCHMARK
  MissileGame *game = new MissileGame(*controls);
#else
  GameMenu *game = new GameMenu(*controls, soundFx);
#endif
  SimpleFont *font = new SimpleFont();

#if ASTEROIDS_RENDERER_MODE == ASTEROIDS_RENDERER_DAC
  renderer = new DACRenderer(kWorldSize, font);
#elif ASTEROIDS_RENDERER_MODE == ASTEROIDS_RENDERER_SPI_DAC
  renderer = new SPIRenderer(kWorldSize, font);
#elif ASTEROIDS_RENDERER_MODE == ASTEROIDS_RENDERER_OLED
  renderer = new HeltecOLEDRenderer(kWorldSize, font);
#elif ASTEROIDS_RENDERER_MODE == ASTEROIDS_RENDERER_SPI_BUFFERED
  renderer = new SPIRendererDMA(kWorldSize, font);
#else
#error "Unsupported ASTEROIDS_RENDERER_MODE"
#endif

#if ASTEROIDS_SPI_BENCHMARK
  game->render_frame(*renderer->get_render_buffer());
#endif
  renderer->start();
  gameLoop = new GameLoop(game, renderer->get_render_buffer());
#if ASTEROIDS_SPI_BENCHMARK != 1
  gameLoop->start();
#endif
  Serial.println("Vector arcade started");
#endif
}

void loop()
{
#if defined(ASTEROIDS_CALIBRATION_DIAGNOSTICS) && ASTEROIDS_CALIBRATION_DIAGNOSTICS
  delay(100);
  return;
#endif
  static uint32_t lastReportMs = 0;
  static int previousFrames = 0;
  const uint32_t now = millis();
  if (now - lastReportMs >= 5000)
  {
    const int frames = renderer->rendered_frames;
    const float elapsedSeconds = (now - lastReportMs) / 1000.0f;
    const float fps = (frames - previousFrames) / elapsedSeconds;
    const uint32_t errors = renderer->error_count + gameLoop->errors;
    Serial.printf("DIAG uptime=%lus fps=%.1f steps=%d transactions=%d errors=%lu free_heap=%u\n",
                  now / 1000,
                  fps,
                  gameLoop->steps,
                  renderer->transactions,
                  static_cast<unsigned long>(errors),
                  ESP.getFreeHeap());
          #if ASTEROIDS_SPI_BENCHMARK
            const auto *buffered = static_cast<SPIRendererDMA *>(renderer);
            const uint32_t pairs = buffered->benchmark_pair_count;
            Serial.printf("SPI_BENCH backend=%s pairs=%lu pair_with_settle_us=%.3f stride=%d frozen_missile_scene=%d\n",
                    ASTEROIDS_SPI_LOW_LEVEL ? "registers" : "driver",
                    static_cast<unsigned long>(pairs),
                    pairs == 0 ? 0 : static_cast<double>(buffered->benchmark_pair_elapsed_us) / pairs,
                    HardwareConfig::kSpiBufferedLineStepsPerDacUpdate,
                    ASTEROIDS_SPI_BENCHMARK == 1);
          #endif
    previousFrames = frames;
    lastReportMs = now;
  }
  delay(1);
}


/*
calibration code for joystick and DAC testing; uncomment to run calibration diagnostics
#include <Arduino.h>
#include "HardwareConfig.h"

void setup()
{
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(HardwareConfig::kJoystickXPin, ADC_11db);
  analogSetPinAttenuation(HardwareConfig::kJoystickYPin, ADC_11db);
}

void loop()
{
  const int x = analogRead(HardwareConfig::kJoystickXPin);
  const int y = analogRead(HardwareConfig::kJoystickYPin);
  Serial.printf("X (GPIO%d): %d\tY (GPIO%d): %d\n",
                HardwareConfig::kJoystickXPin, x,
                HardwareConfig::kJoystickYPin, y);
  delay(1000);
}
*/