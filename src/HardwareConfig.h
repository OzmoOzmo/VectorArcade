#pragma once

#include <Arduino.h>

#define ASTEROIDS_RENDERER_DAC 1
#define ASTEROIDS_RENDERER_SPI_DAC 2
#define ASTEROIDS_RENDERER_OLED 3
#define ASTEROIDS_RENDERER_SPI_BUFFERED 4

#ifndef ASTEROIDS_SPI_BENCHMARK
#define ASTEROIDS_SPI_BENCHMARK 0
#endif

#ifndef ASTEROIDS_SPI_LOW_LEVEL
#define ASTEROIDS_SPI_LOW_LEVEL 1
#endif

#ifndef ASTEROIDS_DAC_BENCHMARK
#define ASTEROIDS_DAC_BENCHMARK 0
#endif

#ifndef ASTEROIDS_RENDERER_BENCHMARK
#define ASTEROIDS_RENDERER_BENCHMARK 0
#endif

#ifndef ASTEROIDS_ENABLE_AUDIO
#define ASTEROIDS_ENABLE_AUDIO 1
#endif

#ifndef ASTEROIDS_RENDERER_MODE
#define ASTEROIDS_RENDERER_MODE ASTEROIDS_RENDERER_DAC
//#define ASTEROIDS_RENDERER_MODE ASTEROIDS_RENDERER_SPI_DAC
//#define ASTEROIDS_RENDERER_MODE ASTEROIDS_RENDERER_OLED
//#define ASTEROIDS_RENDERER_MODE ASTEROIDS_RENDERER_SPI_BUFFERED
#endif

namespace HardwareConfig
{
constexpr uint32_t kCpuSpeedPercent = 200;
static_assert(kCpuSpeedPercent > 0 && kCpuSpeedPercent <= 1000, "CPU percentage must be between 1 and 1000");
constexpr gpio_num_t kDacXPin = GPIO_NUM_25;
constexpr gpio_num_t kDacYPin = GPIO_NUM_26;
constexpr uint8_t kDacOutputScalePercent = 65; //internal dac is too high for my scope (65 i sok)
constexpr uint32_t kDacTimerAlarmTicks = 500;
constexpr uint8_t kDacPixelsPerUpdate = 1;
constexpr uint8_t kDacPixelsPerUpdateMenu = 2;
static_assert(kDacOutputScalePercent > 0 && kDacOutputScalePercent <= 100, "DAC output scale must be between 1 and 100 percent");
static_assert(kDacTimerAlarmTicks > 0, "DAC timer alarm must be positive");
static_assert(kDacPixelsPerUpdate > 0, "DAC pixel stride must be positive");
static_assert(kDacPixelsPerUpdateMenu > 0, "Menu DAC pixel stride must be positive");
constexpr gpio_num_t kLaserPin = GPIO_NUM_32;   //not used

constexpr gpio_num_t kEncoderClockPin = GPIO_NUM_19; //CK
constexpr gpio_num_t kEncoderDataPin = GPIO_NUM_18;  //DT
constexpr int kJoystickXPin = 39;
constexpr int kJoystickYPin = 36;
constexpr bool kJoystickInvertX = false;
constexpr bool kJoystickInvertY = false;
constexpr gpio_num_t kFirePin = GPIO_NUM_14;
constexpr gpio_num_t kThrustPin = GPIO_NUM_16;

constexpr int kSpiMisoPin = -1;
constexpr int kSpiMosiPin = 25;
constexpr int kSpiClockPin = 26;
constexpr int kSpiChipSelectPin = 27;
constexpr gpio_num_t kSpiLatchPin = GPIO_NUM_17;
constexpr int kSpiBufferedLineStepsPerDacUpdate = 4;
static_assert(kSpiBufferedLineStepsPerDacUpdate > 0, "Buffered SPI line steps must be positive");
constexpr uint16_t kBulletExtraDwellUs = 30;
constexpr uint32_t kScreenSaverTimeoutSeconds = 60;

constexpr int kOledSdaPin = GPIO_NUM_5;
constexpr int kOledSclPin = GPIO_NUM_4;
constexpr int kOledResetPin = -1;
constexpr uint8_t kOledI2cAddress = 0x3C;

constexpr gpio_num_t kI2sBitClockPin = GPIO_NUM_4;
constexpr gpio_num_t kI2sWordSelectPin = GPIO_NUM_5;
constexpr gpio_num_t kI2sDataPin = GPIO_NUM_13;
}