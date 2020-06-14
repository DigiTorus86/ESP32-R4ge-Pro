#include "WAV_frame.h"

WAV_frame::WAV_frame()
{
  _frame_idx = 0;
}

/*
 * Calculates the Discrete Fourier Transform of the frame.
 */
bool WAV_frame::calcDFT(WAV_audio *wav)
{
  // Windowing
  double window = .5;
  _fft_in[0].set(wav->getSample(0, 0) * window, 0); 
  for (uint16_t i = 1; i < WAV_FRAME_SIZE; i++) 
  {
    window = -.5 * cos(2.0 * PI * (double)i / (double)WAV_FRAME_SIZE) + .5;
    _fft_in[i].set((float)(wav->getSample(i, 0) * window), 0);
    //_fft_in[i].set((float)(wav->getSample(i, 0) - 0.97 * wav->getSample(i - 1, 0) * window), 0);
  }
  
  // DFT
  double pi2 = -2.0 * PI;
  double angleTerm, cosineA, sineA;
  double invs = 1.0 / WAV_FRAME_SIZE;
  
  for(uint16_t y = 0;y < WAV_FRAME_SIZE; y++) 
  {
        dft_output[y] = 0;
        for(uint16_t x = 0; x < WAV_FRAME_SIZE; x++) 
    {
            angleTerm = pi2 * y * x * invs;
            cosineA = cos(angleTerm);
            sineA = sin(angleTerm);
            dft_output[y].addReal(_fft_in[x].real() * cosineA - _fft_in[x].imag() * sineA);
            dft_output[y].addImag(_fft_in[x].real() * sineA + _fft_in[x].imag() * cosineA);
        }
    }
  
  // Compute the Periodogram
  for (uint16_t i = 0; i < WAV_FRAME_SIZE; i++) 
  {
    power_spectrum[i] = invs * pow(dft_output[i].c_abs(), 2);
  }
  
  return true;
}

/*
 * Calculates the Mel-spaced filterbank.
 */
bool   WAV_frame::calcMelFilterbank()
{
  // TODO
  return false;
}

/*
 * Calculates the Discrete Cosine Transformation
 */
bool   WAV_frame::calcDCT()
{
  // TODO
  return false;
}

/*
 * Calculates the Mel Frequency Cepstral Coefficients for the frame.
 * Returns true on success, false on failure.
 */
bool   WAV_frame::calcMFCC(WAV_audio *wav)
{
  bool success = false;
  success = calcDFT(wav);
  if (!success) return false;
  
  success = calcMelFilterbank();
  if (!success) return false;
  
  return calcDCT();
}

/*
 * Finds the maximum real value of the DFT output
 */
double WAV_frame::findMaxRealOutput()
{
  double max_value = 0;

  // Skip first bin (DC offset)
  for (int i = 1; i < WAV_FRAME_SIZE / 2; i++)
  {
    if (dft_output[i].real() > max_value)
      max_value = dft_output[i].real();
  }
  return max_value;
}

/*
 * Finds the maximum real value of the DFT output
 */
double WAV_frame::findMaxPowerOutput()
{
  double max_value = 0;

  // Skip first bin (DC offset)
  for (int i = 2; i < WAV_FRAME_SIZE / 2; i++)
  {
    if (power_spectrum[i] > max_value)
      max_value = power_spectrum[i];
  }
  return max_value;
}

/*
 * Returns the current Frame Index set by fillFrame()
 */
uint8_t WAV_frame::getFrameIndex()
{
  return _frame_idx;
}
