#include <u8g2.h>
#include <Wire.h>
#include "../HardwareConfig.h"
#include "HeltecOLEDRenderer.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "RenderBuffer.hpp"

u8g2_t u8g2;
static uint32_t oledI2cErrors = 0;
static HeltecOLEDRenderer *activeOledRenderer = nullptr;

static uint8_t u8g2_arduino_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  uint8_t success = 1;
  switch (msg)
  {
  case U8X8_MSG_BYTE_INIT:
    success = Wire.begin(HardwareConfig::kOledSdaPin, HardwareConfig::kOledSclPin) ? 1 : 0;
    Wire.setClock(400000);
    if (!success && activeOledRenderer != nullptr)
    {
      ++oledI2cErrors;
      activeOledRenderer->record_error();
    }
    break;
  case U8X8_MSG_BYTE_START_TRANSFER:
    Wire.beginTransmission(u8x8_GetI2CAddress(u8x8) >> 1);
    break;
  case U8X8_MSG_BYTE_SEND:
    success = Wire.write(static_cast<uint8_t *>(arg_ptr), arg_int) == arg_int ? 1 : 0;
    if (!success && activeOledRenderer != nullptr)
    {
      ++oledI2cErrors;
      activeOledRenderer->record_error();
    }
    break;
  case U8X8_MSG_BYTE_END_TRANSFER:
    success = Wire.endTransmission() == 0 ? 1 : 0;
    if (!success)
    {
      ++oledI2cErrors;
      if (activeOledRenderer != nullptr)
      {
        activeOledRenderer->record_error();
      }
    }
    break;
  }
  return success;
}

static uint8_t u8g2_arduino_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  switch (msg)
  {
  case U8X8_MSG_GPIO_AND_DELAY_INIT:
    if (HardwareConfig::kOledResetPin >= 0)
    {
      pinMode(HardwareConfig::kOledResetPin, OUTPUT);
    }
    break;
  case U8X8_MSG_GPIO_RESET:
    if (HardwareConfig::kOledResetPin >= 0)
    {
      digitalWrite(HardwareConfig::kOledResetPin, arg_int);
    }
    break;
  case U8X8_MSG_DELAY_MILLI:
    delay(arg_int);
    break;
  case U8X8_MSG_DELAY_10MICRO:
    delayMicroseconds(10 * arg_int);
    break;
  case U8X8_MSG_DELAY_100NANO:
    break;
  }
  return 1;
}

void IRAM_ATTR oled_draw_task(void *param)
{
  HeltecOLEDRenderer *renderer = static_cast<HeltecOLEDRenderer *>(param);

  while (true)
  {
    u8g2_ClearBuffer(&u8g2);
    std::vector<DrawInstruction_t> *instructions = renderer->render_buffer->display_frame;
    for (int i = 0; i < instructions->size() - 1; i++)
    {
      if ((*instructions)[i + 1].laser)
      {
        u8g2_DrawLine(&u8g2, 32 + (*instructions)[i].x, (*instructions)[i].y, 32 + (*instructions)[i + 1].x, (*instructions)[i + 1].y);
      }
      renderer->transactions = renderer->transactions + 1;
    }
    vTaskDelay(pdMS_TO_TICKS(33));
    u8g2_SendBuffer(&u8g2);
    // trigger a re-render
    renderer->render_buffer->swapBuffers();
    renderer->rendered_frames = renderer->rendered_frames + 1;
  }
}

HeltecOLEDRenderer::HeltecOLEDRenderer(float world_size, Font *font)
{
  render_buffer = new RenderBuffer(
      0, 64,
      0, 64,
      32,
      32,
      32.0f / world_size,
      font);
}

void HeltecOLEDRenderer::start()
{
  activeOledRenderer = this;
  // Init Driver
  u8g2_Setup_ssd1306_i2c_128x64_noname_f(
      &u8g2,
      U8G2_R0,
      u8g2_arduino_i2c_byte_cb,
      u8g2_arduino_gpio_and_delay_cb);
    u8x8_SetI2CAddress(&u8g2.u8x8, HardwareConfig::kOledI2cAddress << 1);

  u8g2_InitDisplay(&u8g2);
  u8g2_SetPowerSave(&u8g2, 0);
  u8g2_ClearBuffer(&u8g2);
  u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
  u8g2_DrawStr(&u8g2, 0, 14, "OLED STARTUP");
  u8g2_DrawStr(&u8g2, 0, 30, "128x64 READY");
  u8g2_DrawStr(&u8g2, 0, 46, "I2C 0x3C");
  u8g2_SendBuffer(&u8g2);
  Serial.printf("OLED startup frame sent; I2C errors=%lu SDA=%d SCL=%d\n",
                static_cast<unsigned long>(oledI2cErrors),
                HardwareConfig::kOledSdaPin,
                HardwareConfig::kOledSclPin);
  delay(1200);

  xTaskCreatePinnedToCore(oled_draw_task, "OLED Draw Task", 4096, this, 10, &_draw_task_handle, 1);
}
