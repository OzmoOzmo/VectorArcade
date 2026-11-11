#include "SPIRendererDMA.h"
#include "esp_timer.h"
#if ASTEROIDS_SPI_LOW_LEVEL
#include "soc/spi_struct.h"
#include "soc/gpio_sig_map.h"
#include "esp32/rom/gpio.h"
#if !CONFIG_IDF_TARGET_ESP32
#error "The direct SPI backend supports the original ESP32 only"
#endif
#endif

namespace
{
constexpr uint32_t kSettlingUs = 5;
#if ASTEROIDS_SPI_LOW_LEVEL
constexpr uint32_t packDirectWord(uint8_t first, uint8_t second)
{
  return first | (static_cast<uint32_t>(second) << 8);
}
static_assert(packDirectWord(0x58, 0xAB) == 0xAB58, "SPI FIFO byte order must match the driver");

bool writeDirectWord(uint8_t first, uint8_t second)
{
  SPI2.data_buf[0] = packDirectWord(first, second);
  const uint32_t started = ESP.getCycleCount();
  SPI2.cmd.usr = 1;
  while (SPI2.cmd.usr)
  {
    if (ESP.getCycleCount() - started > 240000)
    {
      gpio_matrix_out(HardwareConfig::kSpiChipSelectPin, SIG_GPIO_OUT_IDX, false, false);
      gpio_set_level(static_cast<gpio_num_t>(HardwareConfig::kSpiChipSelectPin), 1);
      return false;
    }
  }
  return true;
}
#endif
}

SPIRendererDMA::SPIRendererDMA(float world_size, Font *font)
{
  render_buffer = new RenderBuffer(1024, 3072, 1024, 3072, 2048, 2048,
                                 1024.0f / world_size, font);
  render_buffer->display_frame->clear();
  render_buffer->drawing_frame->clear();
  render_buffer->needs_render = true;
}

void SPIRendererDMA::prepareEntry(void *parameter)
{
  static_cast<SPIRendererDMA *>(parameter)->prepareFrames();
}

void SPIRendererDMA::outputEntry(void *parameter)
{
  static_cast<SPIRendererDMA *>(parameter)->outputFrames();
}

void SPIRendererDMA::append(int16_t x, int16_t y, bool laser, uint16_t extra_dwell_us)
{
  const bool changed = x != previous_x || y != previous_y;
  if (!changed && laser == previous_laser && extra_dwell_us == 0)
  {
    return;
  }
  if (pending == nullptr)
  {
    xQueueReceive(free_blocks, &pending, portMAX_DELAY);
    pending->count = 0;
  }

  Sample &sample = pending->samples[pending->count++];
  const uint16_t output_y = 4096 - y;
  sample.commands[0] = 0x50 | ((x >> 8) & 0x0F);
  sample.commands[1] = x & 0xFF;
  sample.commands[2] = 0xD0 | ((output_y >> 8) & 0x0F);
  sample.commands[3] = output_y & 0xFF;
  sample.laser = laser;
  sample.write_coordinates = changed;
  sample.extra_dwell_us = laser ? extra_dwell_us : 0;
  previous_x = x;
  previous_y = y;
  previous_laser = laser;
  if (pending->count == 128)
  {
    flush(false);
  }
}

void SPIRendererDMA::flush(bool frame_end)
{
  if (pending == nullptr)
  {
    xQueueReceive(free_blocks, &pending, portMAX_DELAY);
    pending->count = 0;
  }
  pending->frame_end = frame_end;
  xQueueSend(ready_blocks, &pending, portMAX_DELAY);
  pending = nullptr;
}

void SPIRendererDMA::prepareInstruction(const DrawInstruction_t &instruction,
                                      int16_t &current_x, int16_t &current_y)
{
  if (!instruction.laser)
  {
    current_x = instruction.x;
    current_y = instruction.y;
    append(current_x, current_y, false);
    return;
  }

    append(current_x, current_y, true,
      current_x == instruction.x && current_y == instruction.y ? instruction.extra_dwell_us : 0);
  const int delta_x = abs(instruction.x - current_x);
  const int delta_y = abs(instruction.y - current_y);
  const int step_x = current_x < instruction.x ? 1 : -1;
  const int step_y = current_y < instruction.y ? 1 : -1;
  int error = delta_x - delta_y;
  while (current_x != instruction.x || current_y != instruction.y)
  {
    for (int step = 0; step < HardwareConfig::kSpiBufferedLineStepsPerDacUpdate; ++step)
    {
      if (current_x == instruction.x && current_y == instruction.y)
      {
        break;
      }
      const int doubled_error = 2 * error;
      if (doubled_error > -delta_y)
      {
        error -= delta_y;
        current_x += step_x;
      }
      if (doubled_error < delta_x)
      {
        error += delta_x;
        current_y += step_y;
      }
    }
        append(current_x, current_y, true,
          current_x == instruction.x && current_y == instruction.y ? instruction.extra_dwell_us : 0);
  }
}

void SPIRendererDMA::prepareFrames()
{
  int16_t current_x = 2048;
  int16_t current_y = 2048;
  for (;;)
  {
    render_buffer->swapBuffers();
    if (render_buffer->display_frame->empty())
    {
      vTaskDelay(1);
      continue;
    }

    for (const DrawInstruction_t &instruction : *render_buffer->display_frame)
    {
      prepareInstruction(instruction, current_x, current_y);
    }
    append(current_x, current_y, false);
    flush(true);
    xSemaphoreTake(frame_complete, portMAX_DELAY);
  }
}

bool SPIRendererDMA::writeSample(const Sample &sample)
{
  if (!sample.laser)
  {
    set_laser(false);
  }
  if (sample.write_coordinates)
  {
#if ASTEROIDS_SPI_LOW_LEVEL
    if (!writeDirectWord(sample.commands[0], sample.commands[1]) ||
        !writeDirectWord(sample.commands[2], sample.commands[3]))
    {
      set_laser(false);
      record_error();
      return false;
    }
#else
    spi_transaction_t transfer = {};
    transfer.length = 16;
    transfer.flags = SPI_TRANS_USE_TXDATA;
    for (int channel = 0; channel < 2; ++channel)
    {
      transfer.tx_data[0] = sample.commands[channel * 2];
      transfer.tx_data[1] = sample.commands[channel * 2 + 1];
      if (spi_device_polling_transmit(spi, &transfer) != ESP_OK)
      {
        set_laser(false);
        record_error();
        return false;
      }
    }
#endif
    gpio_set_level(HardwareConfig::kSpiLatchPin, 0);
    gpio_set_level(HardwareConfig::kSpiLatchPin, 1);
    ++transactions;
  }
  set_laser(sample.laser);
  delayMicroseconds(kSettlingUs + sample.extra_dwell_us);
  return true;
}

void SPIRendererDMA::outputFrames()
{
#if !ASTEROIDS_SPI_LOW_LEVEL
  if (spi_device_acquire_bus(spi, portMAX_DELAY) != ESP_OK)
  {
    record_error();
    vTaskDelete(nullptr);
    return;
  }
#endif
  bool frame_ok = true;
#if ASTEROIDS_SPI_BENCHMARK
  Sample sample = {{0x58, 0x00, 0xD8, 0x00}, false, true};
  constexpr uint32_t sample_count = 4096;
  const int64_t started = esp_timer_get_time();
  uint32_t completed = 0;
  for (uint32_t index = 0; index < sample_count; ++index)
  {
    sample.commands[1] = index & 0xFF;
    sample.commands[3] = 255 - (index & 0xFF);
    if (!writeSample(sample))
    {
      break;
    }
    ++completed;
  }
  const int64_t elapsed = esp_timer_get_time() - started;
  benchmark_pair_elapsed_us = elapsed;
  benchmark_pair_count = completed;
  Serial.printf("SPI_BENCH backend=%s count=%lu total_us=%lld pair_including_5us_settle_us=%.3f errors=%lu stride=%d\n",
                ASTEROIDS_SPI_LOW_LEVEL ? "registers" : "driver",
                static_cast<unsigned long>(completed), elapsed,
                completed == 0 ? 0 : static_cast<double>(elapsed) / completed,
                static_cast<unsigned long>(error_count), HardwareConfig::kSpiBufferedLineStepsPerDacUpdate);
#endif
  for (;;)
  {
    Block *block = nullptr;
    xQueueReceive(ready_blocks, &block, portMAX_DELAY);
    for (size_t index = 0; index < block->count; ++index)
    {
      if (frame_ok)
      {
        frame_ok = writeSample(block->samples[index]);
      }
    }
    const bool frame_end = block->frame_end;
    xQueueSend(free_blocks, &block, portMAX_DELAY);
    if (frame_end)
    {
      set_laser(false);
      if (frame_ok)
      {
        ++rendered_frames;
      }
      frame_ok = true;
      xSemaphoreGive(frame_complete);
      vTaskDelay(1);
    }
  }
}

void SPIRendererDMA::start()
{
#if ASTEROIDS_SPI_LOW_LEVEL
  if (getApbFrequency() != 80000000)
  {
    record_error();
    Serial.println("ERROR: direct SPI requires an 80 MHz APB clock");
    return;
  }
#endif
  Renderer::start();
  set_laser(false);
  gpio_set_level(HardwareConfig::kSpiLatchPin, 1);
  gpio_set_direction(HardwareConfig::kSpiLatchPin, GPIO_MODE_OUTPUT);

  spi_bus_config_t bus = {};
  bus.mosi_io_num = HardwareConfig::kSpiMosiPin;
  bus.miso_io_num = -1;
  bus.sclk_io_num = HardwareConfig::kSpiClockPin;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  spi_device_interface_config_t device = {};
  device.clock_speed_hz = 20000000;
  device.spics_io_num = HardwareConfig::kSpiChipSelectPin;
  device.flags = SPI_DEVICE_NO_DUMMY;
  device.queue_size = 1;
  if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED) != ESP_OK)
  {
    record_error();
    return;
  }
  Serial.printf("Buffered SPI: backend=%s clock=20000000 stride=%d settling_us=%lu\n",
                ASTEROIDS_SPI_LOW_LEVEL ? "registers" : "driver",
                HardwareConfig::kSpiBufferedLineStepsPerDacUpdate,
                static_cast<unsigned long>(kSettlingUs));
#if ASTEROIDS_SPI_LOW_LEVEL
  const gpio_num_t chip_select = static_cast<gpio_num_t>(HardwareConfig::kSpiChipSelectPin);
  gpio_reset_pin(chip_select);
  gpio_set_level(chip_select, 1);
  gpio_set_direction(chip_select, GPIO_MODE_OUTPUT);
  SPI2.slave.val = 0;
  SPI2.ctrl.val = 0;
  SPI2.ctrl1.val = 0;
  SPI2.ctrl2.val = 0;
  SPI2.user.val = 0;
  SPI2.user1.val = 0;
  SPI2.user2.val = 0;
  SPI2.pin.val = 0;
  SPI2.pin.cs0_dis = 0;
  SPI2.pin.cs1_dis = 1;
  SPI2.pin.cs2_dis = 1;
  SPI2.user.usr_mosi = 1;
  SPI2.user.cs_setup = 1;
  SPI2.user.cs_hold = 1;
  SPI2.ctrl2.setup_time = 1;
  SPI2.ctrl2.hold_time = 2;
  SPI2.mosi_dlen.usr_mosi_dbitlen = 15;
  SPI2.miso_dlen.usr_miso_dbitlen = 0;
  SPI2.clock.val = 0;
  SPI2.clock.clkcnt_n = 3;
  SPI2.clock.clkcnt_h = 1;
  SPI2.clock.clkcnt_l = 3;
  gpio_matrix_out(HardwareConfig::kSpiChipSelectPin, HSPICS0_OUT_IDX, false, false);
#else
  if (spi_bus_add_device(SPI2_HOST, &device, &spi) != ESP_OK)
  {
    spi_bus_free(SPI2_HOST);
    record_error();
    return;
  }
#endif

  free_blocks = xQueueCreate(2, sizeof(Block *));
  ready_blocks = xQueueCreate(2, sizeof(Block *));
  frame_complete = xSemaphoreCreateBinary();
  if (!free_blocks || !ready_blocks || !frame_complete)
  {
    record_error();
    return;
  }
  for (auto &block : blocks)
  {
    Block *available = &block;
    xQueueSend(free_blocks, &available, portMAX_DELAY);
  }
  if (xTaskCreatePinnedToCore(outputEntry, "SPI buffered output", 4096, this, 2,
                            &output_task, 1) != pdPASS)
  {
    record_error();
    return;
  }
  if (xTaskCreatePinnedToCore(prepareEntry, "SPI sample preparation", 4096, this, 1,
                            &prepare_task, 0) != pdPASS)
  {
    vTaskDelete(output_task);
    record_error();
  }
}