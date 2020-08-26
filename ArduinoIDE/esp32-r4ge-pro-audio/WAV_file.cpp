#include "WAV_file.h"

WAV_file::WAV_file()
{
}

/*
 * Save the WAV to the specified file on the SD card.
 * Returns true on success, false on failure.
 */
bool WAV_file::saveFile(const char *filepath, WAV_audio *wav)
{
  Serial.print(F("Saving WAV file: "));
  Serial.println(filepath);
  
  File file;
  file = SD.open(filepath, FILE_WRITE);
  if (!file)
  {
    Serial.println("Error creating WAV file!");
    Serial.println(filepath);
    return false; // failure
  }
  Serial.println("WAV file started.");
  
  size_t   bytes_written;
  bytes_written = file.write((uint8_t *)&wav->header, sizeof(wav_header_t));
  
  if (bytes_written < sizeof(wav_header_t))
  {
    Serial.println("Error writing to WAV file!");
    return false;  // failure
  }
  
  for (uint16_t i = 0; i < WAV_MAX_SAMPLES; i++)
  {
    file.write(wav->getSample(i, 0));
  }
  file.close();
  Serial.println(F("WAV file successfully created."));
  return true; // success!
}

/*
 * Loads the specified WAV file from the SD card.
 * Returns true on success, false on failure.
 */
bool WAV_file::loadFile(const char *filepath, WAV_audio *wav)
{
  Serial.print(F("Loading WAV file: "));
  Serial.println(filepath);

  int  bytes_read;
  File file;
  
  file = SD.open(filepath, FILE_READ);
  if (!file)
  {
    Serial.println("Error opening WAV file!");
    Serial.println(filepath);
    return false; // failure
  }
  
  bytes_read = file.read((uint8_t *)&wav->header, sizeof(wav_header_t));  
  
  if (bytes_read < sizeof(wav_header_t))
  {
    Serial.println(F("Error reading from WAV file!"));
    return false;  // failure
  }
  
  if (wav->header.riff_header != WAV_HDR_RIFF)
  {
    Serial.print(F("Invalid file format. Must be RIFF. Found: "));
    Serial.println(wav->header.riff_header);
    return false;  // failure
  }
  
  if (wav->header.wave_header != WAV_HDR_WAVE)
  {
    Serial.print("Invalid data format. Must be WAVE. Found: ");
    Serial.println(wav->header.wave_header);
    return false;  // failure
  }

  if (wav->header.fmt_chunk_size != 16)
  {
    Serial.print("Invalid format size: ");
    Serial.println(wav->header.fmt_chunk_size);
    return false;  // failure
  }

  if (wav->header.audio_format != WAV_FMT_PCM)
  {
    Serial.println("Invalid format - must be PCM!");
    return false;  // failure
  }
  
  Serial.print(F("# Channels:   ")); Serial.println(wav->header.num_channels);
  Serial.print(F("Sample rate:  ")); Serial.println(wav->header.sample_rate);
  Serial.print(F("Byte rate:    ")); Serial.println(wav->header.byte_rate);
  Serial.print(F("Sample align: ")); Serial.println(wav->header.sample_alignment);
  Serial.print(F("Bits/sample:  ")); Serial.println(wav->header.bits_per_sample);
  Serial.print(F("Data bytes:   ")); Serial.println(wav->header.data_bytes);

  uint8_t sample;
  uint16_t num_channels = wav->header.num_channels;
  
  for(uint16_t i = 0; i < wav->header.data_bytes && i < WAV_MAX_SAMPLES; i++)
  {
    for (uint16_t channel = 0; channel < num_channels; channel++)
    {
      file.read(&sample, 1); 
      wav->setSample(i, channel, sample);
    }
  }
  file.close();
  
  Serial.println(F("\nWAV file successfully read."));
  return true; // success!
}

bool WAV_file::saveDFT(const char* filename, int binCount, double deltaFreq, Complex dft[])
{
  
}

bool     saveMFCC(const char* filename, uint32_t frameCount, uint32_t frameSize, uint32_t frameStride, 
            uint32_t cepstra, double mfcc[][WAV_FRAME_MFCC_CNT])
{
              
}
