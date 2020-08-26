#ifndef _WAV_AUDIO_
#define _WAV_AUDIO_

#include "esp32_r4ge_pro.h"

#define WAV_MAX_SAMPLES  11025

#define WAV_HDR_RIFF     0x46464952  // "RIFF" big endian
#define WAV_HDR_WAVE     0x45564157  // "WAVE" big endian
#define WAV_HDR_FMT      0x20746D66  // "fmt " big endian (with trailing space)
#define WAV_FMT_PCM      1
#define WAV_SILENCE      0
#define WAV_SILENCE_8BIT 64  // should be 128(?)
#define WAV_CHNL_LEFT    0
#define WAV_CHNL_RIGHT   1


typedef struct wav_riff_t {
    uint32_t chunkID = 0x52494646;  // RIFF
    uint32_t chunkSize;
    uint32_t format;
};

typedef struct wav_props_t {
    uint32_t chunkID;
    uint32_t chunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

typedef struct wav_header_t {
    // RIFF Header
    uint32_t riff_header = WAV_HDR_RIFF;  // "RIFF" big endian
    uint32_t wav_size; // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
    uint32_t wave_header = WAV_HDR_WAVE;  // "WAVE" big endian
    // Format Header subchunk
    uint32_t fmt_header = WAV_HDR_FMT;  // "fmt " (with trailing space) big endian
    uint32_t fmt_chunk_size = 16; // Should be 16 for PCM
    uint16_t audio_format = WAV_FMT_PCM;  // Should be 1 for PCM. 3 for IEEE Float
    uint16_t num_channels = 1;  // default to Mono
    uint32_t sample_rate = 11025;    // Require 11025 (0x2B11)
    uint32_t byte_rate = 22050; // Bytes per second = sample_rate * num_channels * Bytes Per Sample
    uint16_t sample_alignment = 2; // = num_channels * Bytes Per Sample
    uint16_t bits_per_sample = 16;
    
    // Data sub-chunk
    uint32_t data_header = 0x61746164;  // "data" big endian
    uint32_t data_bytes; //  = sample count * num_channels * sample byte size
    // uint8_t bytes[]; // Remainder of wav file is sample data bytes
};

class WAV_audio
{
  public:
    WAV_audio();
    uint8_t getChannels();
    void    setChannels(uint16_t num_channels);
    int16_t getSample(uint16_t position, uint16_t channel);
    int16_t getNormalizedSample(uint16_t position, uint16_t channel);
    void    setSample(uint16_t position, uint16_t channel, int16_t sample);
    void    applyPreemphasis();
    void    fillWaveform(double freq, uint16_t channel);
    int16_t silence();
    wav_header_t header;
    
    
  private:
    //wav_header_t _header;
    int16_t   _samples[WAV_MAX_SAMPLES];
    uint16_t   _sample_cnt = 0;
    uint16_t   _current_pos = 0;
};

#endif // _WAV_AUDIO_
