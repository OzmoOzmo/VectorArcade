#include "SoundFX.h"
#include "WAVFile.h"
#include "I2SOutput.h"

SoundFX::SoundFX(I2SOutput *output)
    : bang_large_wav(nullptr),
      bang_medium_wav(nullptr),
      bang_small_wav(nullptr),
      fire_wav(nullptr),
      thrust_wav(nullptr),
      success_wav(nullptr),
      output(output)
{
  if (output == nullptr)
  {
    return;
  }

  bang_large_wav = new WAVFile("/spiffs/bang_large.wav");
  bang_medium_wav = new WAVFile("/spiffs/bang_medium.wav");
  bang_small_wav = new WAVFile("/spiffs/bang_small.wav");
  fire_wav = new WAVFile("/spiffs/fire.wav");
  thrust_wav = new WAVFile("/spiffs/thrust.wav");
  success_wav = new WAVFile("/spiffs/extra_ship.wav");
}

void SoundFX::fire(float volume_adjust)
{
  if (output != nullptr)
  {
    output->add_wav_file(fire_wav, volume_adjust * 0.5);
  }
}
void SoundFX::thrust(float volume_adjust)
{
  if (output != nullptr)
  {
    output->add_wav_file(thrust_wav, volume_adjust * 0.5);
  }
}
void SoundFX::success(float volume_adjust)
{
  if (output != nullptr)
  {
    output->add_wav_file(success_wav, volume_adjust * 0.5);
  }
}
void SoundFX::bang_large(float volume_adjust)
{
  if (output != nullptr)
  {
    output->add_wav_file(bang_large_wav, volume_adjust * 0.5);
  }
}
void SoundFX::bang_medium(float volume_adjust)
{
  if (output != nullptr)
  {
    output->add_wav_file(bang_medium_wav, volume_adjust * 0.5);
  }
}
void SoundFX::bang_small(float volume_adjust)
{
  if (output != nullptr)
  {
    output->add_wav_file(bang_small_wav, volume_adjust * 0.5);
  }
}