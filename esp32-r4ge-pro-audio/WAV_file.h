#ifndef _WAV_FILE_
#define _WAV_FILE_

#include <SD.h>
#include "WAV_audio.h"
#include "WAV_frame.h"
#include "complex.h"

class WAV_file
{
  public:
    WAV_file();
    bool  loadFile(const char* filename, WAV_audio* wav_audio);
    bool  saveFile(const char* filename, WAV_audio* wav_audio);
    bool  saveDFT(const char* filename, int binCount, double deltaFreq, Complex dft[]);
    bool  saveMFCC(const char* filename, uint32_t frameCount, uint32_t frameSize, uint32_t frameStride, 
                   uint32_t cepstra, double mfcc[][WAV_FRAME_MFCC_CNT]);
    
  private:
    char     _last_error[20];
};

#endif // _WAV_AUDIO_
