
#include <math.h>
#include "I2SOutput.h"
#include "WAVFile.h"
#include "../HardwareConfig.h"

// number of frames to try and send at once (a frame is a left and right sample)
#define NUM_FRAMES_TO_SEND 64

typedef struct
{
    int16_t left;
    int16_t right;
} Frame_t;

void i2sWriterTask(void *param)
{
    I2SOutput *output = (I2SOutput *)param;
    Frame_t frames[NUM_FRAMES_TO_SEND];
    while (true)
    {
        for (int i = 0; i < NUM_FRAMES_TO_SEND; i++)
        {
            float sample = 0;
            for (auto &voice : output->voices)
            {
                if (voice.play_position < voice.src->get_number_samples())
                {
                    sample += voice.volume * (voice.src->get_sample(voice.play_position) - 128.0f) / 128.0f;
                    voice.play_position = voice.play_position + 1;
                }
            }
            sample = tanhf(sample);
            frames[i].left = sample * 16383;
            frames[i].right = sample * 16383;
        }
        size_t bytesWritten = 0;
        while (bytesWritten < sizeof(frames))
        {
            size_t written = 0;
            ESP_ERROR_CHECK(i2s_write(output->tx_port, (uint8_t *)frames + bytesWritten,
                                      sizeof(frames) - bytesWritten, &written, portMAX_DELAY));
            bytesWritten += written;
        }
    }
}

void I2SOutput::start()
{
    tx_port = I2S_NUM_1;
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = 11025;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = 0;
    config.dma_buf_count = 4;
    config.dma_buf_len = NUM_FRAMES_TO_SEND;
    config.use_apll = false;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;
    ESP_ERROR_CHECK(i2s_driver_install(tx_port, &config, 0, nullptr));

    i2s_pin_config_t pinConfig = {};
    pinConfig.mck_io_num = I2S_PIN_NO_CHANGE;
    pinConfig.bck_io_num = HardwareConfig::kI2sBitClockPin;
    pinConfig.ws_io_num = HardwareConfig::kI2sWordSelectPin;
    pinConfig.data_out_num = HardwareConfig::kI2sDataPin;
    pinConfig.data_in_num = I2S_PIN_NO_CHANGE;
    ESP_ERROR_CHECK(i2s_set_pin(tx_port, &pinConfig));
    // start a task to write samples to the i2s peripheral
    TaskHandle_t writerTaskHandle;
    xTaskCreate(i2sWriterTask, "i2s Writer Task", 4096, this, 2, &writerTaskHandle);
}

void I2SOutput::add_wav_file(WAVFile *wav_file, float volume)
{
    // try and find a free slot in existing voices
    for (auto &voice : voices)
    {
        if (voice.play_position == voice.src->get_number_samples())
        {
            voice.src = wav_file;
            voice.play_position = 0;
            voice.volume = volume;
            return;
        }
    }
    voices.push_back({.src = wav_file,
                      .play_position = 0,
                      .volume = volume});
}
