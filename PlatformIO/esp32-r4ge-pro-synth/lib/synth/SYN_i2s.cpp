#include "SYN_i2s.h"

SYN_i2s::SYN_i2s(int lrck_pin, int bclk_pin, int dout_pin)
{
    // i2s configuration
    // See https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/include/driver/driver/i2s.h

    _i2s_config = 
    {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SYN_I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,  // (i2s_bits_per_sample_t) 8
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,  // MONO
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),  // | I2S_COMM_FORMAT_PCM    
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,      // high interrupt priority. See esp_intr_alloc.h for more
        .dma_buf_count = SYN_I2S_DMA_BUFF_CNT,
        .dma_buf_len = SYN_I2S_DMA_BUFF_LEN,        
        .use_apll = false,        // I2S using APLL as main I2S clock, enable it to get accurate clock
        .tx_desc_auto_clear = 0,  // helps in avoiding noise in case of data unavailability
        .fixed_mclk = 0
    };

    _pin_config = 
    {
        .bck_io_num = bclk_pin, 
        .ws_io_num = lrck_pin, // left-right data indicator
        .data_out_num = dout_pin, // this is DATA output pin (DIN on PCM5102)
        .data_in_num = -1   // Not used (normally for microphone)
    };
}

/*
 * Initialize the I2S audio output
 */
bool SYN_i2s::initAudio()
{
	esp_err_t err;
	
	err = i2s_driver_install((i2s_port_t)_port_num, &_i2s_config, 0, NULL);
	if (err != ESP_OK)
	{
		Serial.print("I2S driver install fail: ");
		Serial.println(err);
        _initialized = false;
		return false;
	}	
	i2s_set_pin((i2s_port_t)_port_num, &_pin_config);
	i2s_set_clk((i2s_port_t)_port_num, SYN_I2S_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
	
    Serial.println("I2S initialized.");
    _initialized = true;
	return true;
}

/* 
 * Plays the audio sample.  
 * Does not return until sample is done playing.
 */
void SYN_i2s::playAudio(SYN_buffer *buff, size_t length)
{
  bool     audio_playing = false;
  int      sample_pos = 0;
  float    buff_val = 0;
  int16_t  temp = 0;
  size_t   samples_read = 0;
  size_t   bytes_out = 0;
  uint8_t  audio_buffer[SYN_I2S_BUFFER_SIZE];
  SYN_buff_err err;

  if (!_initialized)
  {
      audio_playing = initAudio();
  }  
  else
  {
      audio_playing = true;
  }
  
  // Fill I2S transfer audio buffer from sample buffer
  while (audio_playing)
  {
    for (int i = 0; i < SYN_I2S_SAMPLES_PER_BUFFER; i++)
    {
        // get the next sample from the buffer
        err = buff->pop(&buff_val);
        if (err == SYN_BUFF_ERR_OK)
        {
            samples_read++;
        }
        else
        {
            Serial.print(F("I2S play buffer pop error: ")); Serial.println(err);
            audio_playing = false;
            buff_val = 0;
            break;
        }

        // Convert audio -1.0 to 1.0 buffer sample to 16-bit signed int sample
        temp = (int16_t)(buff_val * 16000); // sample[sample_pos + i] * 16000);

        audio_buffer[i * 2] = (uint8_t)temp & 0xff;
        temp = temp >> 8;
        audio_buffer[i * 2 + 1] = (uint8_t)temp;
    }
    
    // Write data to I2S DMA buffer.  Blocking call, last parameter = ticks to wait or portMAX_DELAY for no timeout
    i2s_write((i2s_port_t)_port_num, (const char *)&audio_buffer, sizeof(audio_buffer), &bytes_out, 100);
    if (bytes_out != sizeof(audio_buffer)) Serial.println("I2S write timeout");

    sample_pos += SYN_I2S_SAMPLES_PER_BUFFER;
    if (sample_pos >= length - 1) audio_playing = false;
  }
  buff->readComplete(samples_read);
}

void SYN_i2s::stopAudio()
{
  i2s_driver_uninstall((i2s_port_t)_port_num);
  _initialized = false;
}