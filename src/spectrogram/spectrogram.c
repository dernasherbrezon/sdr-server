#include "spectrogram.h"
#include <errno.h>
#include <complex.h> // should go before fftw3.h
#include <fftw3.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>

volatile sig_atomic_t do_exit = 0;

void spectrogram_sighandler(int signum) {
  fprintf(stderr, "Signal caught, exiting!\n");
  do_exit = 1;
}

static void spectrogram_shutdown(spectrogram *spec) {
  if (spec->file != NULL) {
    iq_file_destroy(spec->file);
  }
  if (spec->png != NULL) {
    png_util_destroy(spec->png);
  }
  if (spec->temp != NULL) {
    free(spec->temp);
  }
  if (spec->p != NULL) {
    fftwf_destroy_plan(spec->p);
  }
  if (spec->in != NULL) {
    fftwf_free(spec->in);
  }
  if (spec->out != NULL) {
    fftwf_free(spec->out);
  }
}

static unsigned int spectrogram_convert_flags(const char *fftw_flags) {
  if (strcmp(fftw_flags, "FFTW_MEASURE") == 0) {
    return FFTW_MEASURE;
  }
  if (strcmp(fftw_flags, "FFTW_ESTIMATE") == 0) {
    return FFTW_ESTIMATE;
  }
  fprintf(stderr, "unsupported fftw flag: %s fallback to FFTW_ESTIMATE\n", fftw_flags);
  return FFTW_ESTIMATE;
}

int spectrogram_main(spectrogram *req) {
  if (req->input_file == NULL) {
    fprintf(stderr, "-i (input file) parameter is missing\n");
    return -EINVAL;
  }
  if (req->output_file == NULL) {
    fprintf(stderr, "-o (output file) parameter is missing\n");
    return -EINVAL;
  }

  if (req->width <= 0) {
    fprintf(stderr, "-w (width) should be positive. got: %d\n", req->width);
    return -EINVAL;
  }
  if (req->sampling_rate <= 0) {
    fprintf(stderr, "-s (sampling_rate) should be positive. got: %d\n", req->sampling_rate);
    return -EINVAL;
  }
  if (req->width > req->sampling_rate) {
    fprintf(stderr, "-w (%d) is too wide for sampling rate %d\n", req->width, req->sampling_rate);
    return -EINVAL;
  }

  int code = iq_file_create(req->input_file, req->width, req->data_format, &req->file);
  if (code != 0) {
    spectrogram_shutdown(req);
    return code;
  }

  uint32_t fft_per_row = req->sampling_rate / req->width;
  uint32_t skip_per_row = req->sampling_rate % req->width;
  int half_width = req->width / 2;
  int odd_width = req->width % 2;

  uint32_t samples = 0;
  iq_file_get_samples(&samples, req->file);

  uint32_t height = samples / req->sampling_rate;

  FILE *png_fp = fopen(req->output_file, "wb");
  if (png_fp == NULL) {
    fprintf(stderr, "unable to write to output: %s\n", req->output_file);
    spectrogram_shutdown(req);
    return -1;
  }

  code = png_util_init(req->width, height, png_fp, &req->png);
  if (code != 0) {
    spectrogram_shutdown(req);
    return code;
  }

  code = setjmp(png_jmpbuf(req->png->png_ptr));
  if (code != 0) {
    fprintf(stderr, "unable to process png file. exit\n");
    spectrogram_shutdown(req);
    return code;
  }

  req->temp = malloc(sizeof(float) * req->width);
  if (req->temp == NULL) {
    spectrogram_shutdown(req);
    return -ENOMEM;
  }
  float normalization_factor = 1.0f / req->width;

  req->in = fftwf_malloc(sizeof(fftwf_complex) * req->width);
  if (req->in == NULL) {
    spectrogram_shutdown(req);
    return -ENOMEM;
  }
  req->out = fftwf_malloc(sizeof(fftwf_complex) * req->width);
  if (req->out == NULL) {
    spectrogram_shutdown(req);
    return -ENOMEM;
  }
  req->p = fftwf_plan_dft_1d(req->width, req->in, req->out, FFTW_FORWARD, spectrogram_convert_flags(req->fftw_flags));
  if (req->p == NULL) {
    spectrogram_shutdown(req);
    return -ENOMEM;
  }
  uint32_t current_row = 0;
  while (!do_exit && current_row < height) {
    for (uint32_t j = 0; j < req->width; j++) {
      req->temp[j] = -255.0f;
    }

    for (uint32_t i = 0; i < fft_per_row; i++) {
      code = iq_file_read(req->in, req->file);
      if (code != 0) {
        break;
      }
      fftwf_execute(req->p);
      for (uint32_t j = 0; j < req->width; j++) {
        fftwf_complex cur = req->out[j] * normalization_factor;
        float power = crealf(cur) * crealf(cur) + cimagf(cur) * cimagf(cur) + 1e-20f;
        req->temp[j] = fmaxf(req->temp[j], power);
      }
    }
    // no more data in the file
    // or file cannot be read
    if (code != 0) {
      break;
    }
    for (uint32_t j = 0; j < half_width; j++) {
      float cur = 10 * log10f(req->temp[j]);
      req->temp[j] = 10 * log10f(req->temp[half_width + j]);
      req->temp[half_width + j] = cur;
    }
    if (odd_width) {
      req->temp[req->width - 1] = 10 * log10f(req->temp[req->width - 1]);
    }

    png_util_set_data(req->temp, req->png);

    iq_file_skip(skip_per_row, req->file);
    current_row++;
  }

  png_util_write_image(req->png);

  spectrogram_shutdown(req);

  return 0;
}
