#include <cstring>
#include "SPIRenderer.h"
#include "../HardwareConfig.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static SPIRenderer *active_spi_renderer = nullptr;
static hw_timer_t *spi_timer = nullptr;
static TaskHandle_t spi_draw_task_handle = nullptr;
constexpr int kLineStepsPerDacUpdate = 16;

void IRAM_ATTR spi_draw_timer()
{
  if (spi_draw_task_handle != nullptr)
  {
    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(spi_draw_task_handle, &higher_priority_task_woken);
    if (higher_priority_task_woken == pdTRUE)
    {
      portYIELD_FROM_ISR();
    }
  }
}

void spi_draw_task(void *parameter)
{
  (void)parameter;
  if (spi_device_acquire_bus(active_spi_renderer->spi, portMAX_DELAY) != ESP_OK)
  {
    active_spi_renderer->record_error();
    vTaskDelete(nullptr);
    return;
  }
  for (;;)
  {
    const uint32_t notification_count = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (active_spi_renderer != nullptr)
    {
#if ASTEROIDS_RENDERER_BENCHMARK
      if (notification_count > 1)
      {
        active_spi_renderer->benchmark_missed_timer_ticks += notification_count - 1;
      }
      const uint32_t started = Renderer::benchmarkCycleCount();
#endif
      active_spi_renderer->draw();
#if ASTEROIDS_RENDERER_BENCHMARK
      active_spi_renderer->recordBenchmarkDraw(Renderer::benchmarkCycleCount() - started);
#endif
    }
  }
}

void SPIRenderer::write_position(int16_t position_x, int16_t position_y)
{
  const int x = position_x;
  const int y = 4096 - position_y;

#if ASTEROIDS_RENDERER_BENCHMARK
  const uint32_t started = Renderer::benchmarkCycleCount();
#endif
  spi_transaction_t t1;
  memset(&t1, 0, sizeof(t1));
  t1.length = 16;
  t1.flags = SPI_TRANS_USE_TXDATA;
  t1.tx_data[0] = 0b01010000 | ((x >> 8) & 0xF);
  t1.tx_data[1] = x & 0xFF;
  if (spi_device_polling_transmit(spi, &t1) != ESP_OK)
  {
    record_error();
    return;
  }

  spi_transaction_t t2;
  memset(&t2, 0, sizeof(t2));
  t2.length = 16;
  t2.flags = SPI_TRANS_USE_TXDATA;
  t2.tx_data[0] = 0b11010000 | ((y >> 8) & 0xF);
  t2.tx_data[1] = y & 0xFF;
  if (spi_device_polling_transmit(spi, &t2) != ESP_OK)
  {
    record_error();
    return;
  }

  gpio_set_level(HardwareConfig::kSpiLatchPin, 0);
  gpio_set_level(HardwareConfig::kSpiLatchPin, 1);
  transactions = transactions + 1;
#if ASTEROIDS_RENDERER_BENCHMARK
  recordBenchmarkOutput(Renderer::benchmarkCycleCount() - started);
#endif
}

void SPIRenderer::draw()
{
  if (!segment_active)
  {
    if (draw_position >= render_buffer->display_frame->size())
    {
      rendered_frames = rendered_frames + 1;
      render_buffer->swapBuffers();
      draw_position = 0;
      return;
    }

    const DrawInstruction_t &instruction = render_buffer->display_frame->at(draw_position);
    target_x = instruction.x;
    target_y = instruction.y;
    if (!instruction.laser)
    {
      set_laser(false);
      current_x = target_x;
      current_y = target_y;
      write_position(current_x, current_y);
      draw_position = draw_position + 1;
      return;
    }

    segment_dx = abs(target_x - current_x);
    segment_dy = abs(target_y - current_y);
    segment_sx = current_x < target_x ? 1 : -1;
    segment_sy = current_y < target_y ? 1 : -1;
    segment_error = segment_dx - segment_dy;
    segment_active = true;
    set_laser(true);
  }

  write_position(current_x, current_y);

  if (current_x == target_x && current_y == target_y)
  {
    segment_active = false;
    draw_position = draw_position + 1;
    return;
  }

  for (int step = 0; step < kLineStepsPerDacUpdate; ++step)
  {
    if (current_x == target_x && current_y == target_y)
    {
      break;
    }

    const int16_t doubled_error = 2 * segment_error;
    if (doubled_error > -segment_dy)
    {
      segment_error -= segment_dy;
      current_x += segment_sx;
    }
    if (doubled_error < segment_dx)
    {
      segment_error += segment_dx;
      current_y += segment_sy;
    }
  }
}

SPIRenderer::SPIRenderer(float world_size, Font *font)
    : draw_position(0),
      current_x(0),
      current_y(0),
      target_x(0),
      target_y(0),
      segment_dx(0),
      segment_dy(0),
      segment_sx(0),
      segment_sy(0),
      segment_error(0),
      segment_active(false)
{
  render_buffer = new RenderBuffer(
      1024, 3072,
      1024, 3072,
      2048,
      2048,
      1024.0f / world_size,
      font);
}

void SPIRenderer::start()
{
  // Keep LDAC inactive until both channel values have been sent.
  gpio_set_direction(HardwareConfig::kSpiLatchPin, GPIO_MODE_OUTPUT);
  gpio_set_level(HardwareConfig::kSpiLatchPin, 1);

  // setup SPI output
  esp_err_t ret;
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = HardwareConfig::kSpiMosiPin;
    buscfg.miso_io_num = HardwareConfig::kSpiMisoPin;
    buscfg.sclk_io_num = HardwareConfig::kSpiClockPin;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    spi_device_interface_config_t devcfg = {};
    devcfg.mode = 0;
    devcfg.clock_speed_hz = 20000000;
    devcfg.spics_io_num = HardwareConfig::kSpiChipSelectPin;
    devcfg.flags = SPI_DEVICE_NO_DUMMY;
    devcfg.queue_size = 2;
  //Initialize the SPI bus
  ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);
  if (ret != ESP_OK)
  {
    record_error();
    Serial.printf("ERROR: SPI bus initialization failed (%d)\n", ret);
    return;
  }
  //Attach the SPI device
  ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
  if (ret != ESP_OK)
  {
    record_error();
    Serial.printf("ERROR: SPI DAC initialization failed (%d)\n", ret);
    return;
  }

  Renderer::start();

  spi_timer = timerBegin(2, 4, true);
  if (spi_timer == nullptr)
  {
    record_error();
    Serial.println("ERROR: failed to create SPI renderer timer");
    return;
  }

  active_spi_renderer = this;
  if (xTaskCreatePinnedToCore(spi_draw_task, "SPI DAC Draw", 4096, nullptr, 2, &spi_draw_task_handle, 1) != pdPASS)
  {
    record_error();
    Serial.println("ERROR: failed to create SPI renderer task");
    spi_draw_task_handle = nullptr;
    return;
  }

  timerAttachInterrupt(spi_timer, &spi_draw_timer, true);
  timerAlarmWrite(spi_timer, 800, true);
  timerAlarmEnable(spi_timer);
}
