#ifndef _DFT_
#define _DFT_

#include "WAV_audio.h"
#include "complex.h"

#define DFT_FRAME_SIZE  512
#define DFT_NFFT        512



class DFT
{
  public:
    DFT();
    bool    calculateDFT(WAV_audio *wav);
    bool    calculateDFT();
    double  getPowerCoef(uint16_t index);
    double  findMaxRealOutput();
    Complex dft_input[DFT_FRAME_SIZE];
    Complex dft_output[DFT_FRAME_SIZE];
    
  private:
    bool    _success = false;
    const double  _inverse = 1.0 / (double)DFT_FRAME_SIZE;

};

#endif // _DFT_
