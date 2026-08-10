#ifndef SPECTROGRAM_H_
#define SPECTROGRAM_H_

#include <stdint.h>

#include "iq_file.h"
#include "png_util.h"

typedef struct {
  uint32_t sampling_rate;
  int width;
  char *data_format;
  char *input_file;
  char *output_file;
  char *fftw_flags;

  iq_file *file;
  png_util *png;
  float *temp;
  fftwf_complex *in;
  fftwf_complex *out;
  fftwf_plan p;
} spectrogram;

int spectrogram_main(spectrogram *req);

void spectrogram_sighandler(int signum);

#endif /* SPECTROGRAM_H_ */
