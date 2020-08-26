#include "WAV_audio.h"

WAV_audio::WAV_audio()
{
  header.num_channels     = 1; // mono
  header.byte_rate        = header.sample_rate * header.num_channels;
  header.sample_alignment = header.num_channels;
}

/*
 * Get the number of audio channels.  
 * 1 = mono, 2 = stereo
 */
uint8_t WAV_audio::getChannels()
{
  return header.num_channels;
}

/*
 * Set the number of audio channels.  
 * 1 = mono, 2 = stereo (currently not supported!)
 */
void  WAV_audio::setChannels(uint16_t num_channels)
{
  header.num_channels = num_channels;
}

/*
 * Returns the sample at the specified position.
 * Returns silence if position is outside the sample bounds.
 */
int16_t WAV_audio::getSample(uint16_t position, uint16_t channel)
{
  if (position < WAV_MAX_SAMPLES && channel < header.num_channels)
	  return _samples[position]; // TODO:  add channel?
  else
	  return WAV_SILENCE;
}

int16_t WAV_audio::getNormalizedSample(uint16_t position, uint16_t channel)
{
  if (position < WAV_MAX_SAMPLES && channel < header.num_channels)
    return _samples[position] - WAV_SILENCE; // TODO:  add channel?
  else
    return WAV_SILENCE;
}

/*
 * Sets the sample value at the specified position.
 */
void  WAV_audio::setSample(uint16_t position, uint16_t channel, int16_t sample)
{
  if (position < WAV_MAX_SAMPLES && channel < header.num_channels)	
	  _samples[position] = sample;
}

/*
 * Applies a pre-emphasis filter to the sample data.
 */
void WAV_audio::applyPreemphasis()
{
    for (uint16_t i = header.data_bytes - 1; i > 0; i--)
    {
      _samples[i] -= 0.95 * _samples[i - 1];
    }
}

int16_t WAV_audio::silence()
{
  if (header.bits_per_sample == 8)
    return WAV_SILENCE_8BIT;
  else
    return WAV_SILENCE;
}

/*
 * Fill the audio buffer with a sine waveform 
 */
void WAV_audio::fillWaveform(double freq, uint16_t channel)
{
  if (freq <= 0) return;
  if (header.sample_rate == 0)
    header.sample_rate = 11025;
  
  double two_pi_scaled = 2 * PI * freq / (double)header.sample_rate;
  for(int i = 0; i < WAV_MAX_SAMPLES; i++) 
  {
      _samples[i] = (double)32000 * sin((double)i * two_pi_scaled);
      if (i < 400) Serial.println(_samples[i]);
  }

  
}
