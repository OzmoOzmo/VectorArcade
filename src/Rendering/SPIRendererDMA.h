#pragma once

#include "Renderer.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class SPIRendererDMA : public Renderer
{
#if defined(ARCADE_TESTS)
  friend struct ArcadeTestAccess;
#endif
private:
  struct Sample
  {
    uint8_t commands[4];
    bool laser;
    bool write_coordinates;
    uint16_t extra_dwell_us;
  };

  struct Block
  {
    Sample samples[128];
    size_t count;
    bool frame_end;
  };

  Block blocks[2];
  QueueHandle_t free_blocks = nullptr;
  QueueHandle_t ready_blocks = nullptr;
  SemaphoreHandle_t frame_complete = nullptr;
  spi_device_handle_t spi = nullptr;
  TaskHandle_t output_task = nullptr;
  TaskHandle_t prepare_task = nullptr;
  Block *pending = nullptr;
  int16_t previous_x = -1;
  int16_t previous_y = -1;
  bool previous_laser = false;

  static void prepareEntry(void *parameter);
  static void outputEntry(void *parameter);
  void prepareFrames();
  void prepareInstruction(const DrawInstruction_t &instruction, int16_t &current_x, int16_t &current_y);
  void outputFrames();
  void append(int16_t x, int16_t y, bool laser, uint16_t extra_dwell_us = 0);
  void flush(bool frame_end);
  bool writeSample(const Sample &sample);

public:
#if ASTEROIDS_SPI_BENCHMARK
  volatile uint32_t benchmark_pair_count = 0;
  volatile uint32_t benchmark_pair_elapsed_us = 0;
#endif
  SPIRendererDMA(float world_size, Font *font);
  void start() override;
};