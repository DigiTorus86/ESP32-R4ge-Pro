#ifndef _WAV_FRAME_
#define _WAV_FRAME_

#include "WAV_audio.h"
#include "complex.h"

#define WAV_FRAME_SIZE      512  // # of samples in 25 msec @ 11025 Hz sample rate
#define WAV_FRAME_STRIDE   110  // # of samples in 10 msec @ 11025 Hz sample rate 
#define WAV_FRAMES_PER_SEC  96  // (FREQ - FRAME_SIZE) / FRAME_STRIDE (or less)
#define WAV_FRAMES_NFFT    512  // N for FFT size
#define WAV_FRAME_MFCC_CNT  12  // # of Mel Frequency Cepstrum Coefficients to be calculated for each frame

class WAV_frame
{
  public:
    WAV_frame();
    uint8_t getFrameIndex();
    bool    calcDFT(WAV_audio *wav); 
    bool    calcMelFilterbank();
    bool    calcDCT();
    bool    calcMFCC(WAV_audio *wav);
    double  findMaxRealOutput();
    double  findMaxPowerOutput();
    Complex dft_output[WAV_FRAME_SIZE];
    double  power_spectrum[WAV_FRAME_SIZE];
  
  private:
    Complex  _fft_in[WAV_FRAME_SIZE];  
    float    _mfcc[WAV_FRAME_MFCC_CNT];
    uint8_t  _frame_idx = 0;
};

#endif // _WAV_FRAME_
