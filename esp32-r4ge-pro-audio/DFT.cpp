#include "DFT.h"

DFT::DFT()
{
  _success = false;
}

bool DFT::calculateDFT(WAV_audio *wav)
{
  Serial.println("DFT starting....");
  int16_t silence = wav->silence();

  // Windowing and conversion to Complex 
  for (uint16_t i = 0; i < DFT_FRAME_SIZE; i++)
  {
    double window = -.5 * cos(2.0 * PI * (double)i / (double)DFT_FRAME_SIZE) + .5;
    double real = (double)(wav->getSample(i, 0) - silence) * window;
    dft_input[i].set(real, 0);
  }
  Serial.println("DFT windowing done.");
  calculateDFT();
  
  return true;
}

bool DFT::calculateDFT()
{
  int n = DFT_FRAME_SIZE;
  
  for (uint16_t k = 0; k < n; k++)
  {  // For each output element
    Complex sum = Complex(0, 0);
    for (uint16_t t = 0; t < n; t++)
    {  // For each input element
      double angle = 2 * PI * t * k / n;
      Complex comp = Complex(0, -angle);
      sum += dft_input[t] * comp.c_exp();
    }
    Serial.print(k); Serial.print(", ");
    dft_output[k] = sum;
  }
  Serial.println("");
  return true; // success
}

double DFT::getPowerCoef(uint16_t index)
{
  if (index < DFT_FRAME_SIZE)
    return _inverse * pow(dft_output[index].c_abs(), 2);
  else
    return 0;
}


double DFT::findMaxRealOutput()
{
  double max_value = 0;

  // Skip first bin (DC offset)
  for (int i = 1; i < DFT_FRAME_SIZE; i++)
  {
    if (dft_output[i].real() > max_value)
      max_value = dft_output[i].real();
  }
  return max_value;
}
