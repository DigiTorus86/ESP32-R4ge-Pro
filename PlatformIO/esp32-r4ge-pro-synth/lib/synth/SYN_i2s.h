#ifndef _SYN_I2S_
#define _SYN_I2S_

#include "SYN_common.h"
#include "SYN_buffer.h"
#include "driver/i2s.h"

#define SYN_I2S_DEFAULT_LRCK_PIN  25
#define SYN_I2S_DEFAULT_BCLK_PIN  26
#define SYN_I2S_DEFAULT_DOUT_PIN   4

#define SYN_I2S_SAMPLE_RATE     11025
#define SYN_I2S_DMA_BUFF_CNT        8
#define SYN_I2S_DMA_BUFF_LEN       64

#define   SYN_I2S_BUFFER_SIZE          512 
#define   SYN_I2S_SAMPLES_PER_BUFFER   256   // 2 bytes per sample

class SYN_i2s
{
  public:
    SYN_i2s(int lrck_pin, int bclk_pin, int dout_pin);
    bool initAudio();
    void playAudio(SYN_buffer *buff, size_t length);
    void stopAudio();
    
    
  private:
    int _port_num; 
    i2s_config_t _i2s_config;
    i2s_pin_config_t _pin_config;
    bool _initialized;
};

#endif // _SYN_I2S_