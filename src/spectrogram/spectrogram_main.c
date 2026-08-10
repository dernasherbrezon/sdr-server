#include <stdint.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <complex.h> // should go before fftw3.h
#include <fftw3.h>
#include <signal.h>

#include <string.h>
#include <math.h>
#include "png_util.h"
#include "iq_file.h"

volatile sig_atomic_t do_exit = 0;
iq_file *file = NULL;
png_util *png = NULL;

static void spectrogram_sighandler(int signum) {
  fprintf(stderr, "Signal caught, exiting!\n");
  do_exit = 1;
}

static void spectrogram_usage() {
  printf("Usage:\n");
  printf("  -h  print help\n");
  printf("  -w <width> image width (default: 1024)\n");
  printf("  -s <sampling_rate> sampling rate (default: 48000)\n");
  printf("  -d <data_format> data format: cu8, cs16, cf32 (default: cu8)\n");
  printf("  -i <input_file> I/Q input file\n");
  printf("  -o <output_file> spectrogram output\n");
  printf("  -f <fftw_flags> supported values: FFTW_ESTIMATE (quick start, but not optimal for large files), FFTW_MEASURE (slow to start, but more efficient on large files). Default: FFTW_MEASURE\n");
}

static void spectrogram_shutdown() {
  if (file != NULL) {
    iq_file_destroy(file);
  }
  if (png != NULL) {
    png_util_destroy(png);
  }
}

static unsigned int spectrogram_convert_flags(char *fftw_flags) {
  if (strcmp(fftw_flags, "FFTW_MEASURE") == 0) {
    return FFTW_MEASURE;
  }
  if (strcmp(fftw_flags, "FFTW_ESTIMATE") == 0) {
    return FFTW_ESTIMATE;
  }
  fprintf(stderr, "unsupported fftw flag: %s fallback to FFTW_ESTIMATE\n", fftw_flags);
  return FFTW_ESTIMATE;
}

int main(int argc, char **argv) {
  uint32_t sampling_rate = 48000;
  int width = 1024;
  char *data_format = "cu8";
  char *input_file = NULL;
  char *output_file = NULL;
  char *fftw_flags = "FFTW_MEASURE";

  int dopt;
  while ((dopt = getopt(argc, argv, "hw:s:d:i:o:f:")) != EOF) {
    switch (dopt) {
      case 'h':
        spectrogram_usage();
        return EXIT_SUCCESS;
      case 'w':
        width = atoi(optarg);
        break;
      case 's':
        sampling_rate = (uint32_t) atof(optarg);
        break;
      case 'd':
        data_format = optarg;
        break;
      case 'i':
        input_file = optarg;
        break;
      case 'o':
        output_file = optarg;
        break;
      case 'f':
        fftw_flags = optarg;
        break;
      default:
        exit(EXIT_FAILURE);
    }
  }

  if (input_file == NULL) {
    fprintf(stderr, "-i (input file) parameter is missing\n");
    spectrogram_usage();
    return EXIT_FAILURE;
  }
  if (output_file == NULL) {
    fprintf(stderr, "-o (output file) parameter is missing\n");
    spectrogram_usage();
    return EXIT_FAILURE;
  }

  int code = iq_file_create(input_file, width, data_format, &file);
  if (code != 0) {
    spectrogram_shutdown();
    return EXIT_FAILURE;
  }

  uint32_t numberOfFftPerRow = sampling_rate / width;
  uint32_t skipPerRow = sampling_rate % width;
  int half_width = width / 2;
  int odd_width = width % 2;

  uint32_t samples = 0;
  iq_file_get_samples(&samples, file);

  uint32_t height = samples / sampling_rate;

  FILE *png_fp = fopen(output_file, "wb");
  if (png_fp == NULL) {
    fprintf(stderr, "unable to write to output: %s\n", output_file);
    spectrogram_shutdown();
    return EXIT_FAILURE;
  }

  code = png_util_init(width, height, png_fp, &png);
  if (code != 0) {
    spectrogram_shutdown();
    return EXIT_FAILURE;
  }

  float *temp = malloc(sizeof(float) * width);
  if (temp == NULL) {
    spectrogram_shutdown();
    return EXIT_FAILURE;
  }
  float normalization_factor = 1.0f / width;

  signal(SIGINT, spectrogram_sighandler);
  signal(SIGHUP, spectrogram_sighandler);
  signal(SIGTERM, spectrogram_sighandler);

  fftwf_complex *in = fftwf_malloc(sizeof(fftwf_complex) * width);
  fftwf_complex *out = fftwf_malloc(sizeof(fftwf_complex) * width);
  fftwf_plan p = fftwf_plan_dft_1d(width, in, out, FFTW_FORWARD, spectrogram_convert_flags(fftw_flags));
  uint32_t current_row = 0;
  while (!do_exit && current_row < height) {
    for (int j = 0; j < width; j++) {
      temp[j] = -255.0f;
    }

    for (int i = 0; i < numberOfFftPerRow; i++) {
      code = iq_file_read(in, file);
      if (code != 0) {
        break;
      }
      fftwf_execute(p);
      for (int j = 0; j < width; j++) {
        fftwf_complex cur = out[j] * normalization_factor;
        float power = crealf(cur) * crealf(cur) + cimagf(cur) * cimagf(cur) + 1e-20f;
        temp[j] = fmaxf(temp[j], power);
      }
    }
    // no more data in the file
    // or file cannot be read
    if (code != 0) {
      break;
    }
    for (int j = 0; j < half_width; j++) {
      float cur = 10 * log10f(temp[j]);
      temp[j] = 10 * log10f(temp[half_width + j]);
      temp[half_width + j] = cur;
    }
    if (odd_width) {
      temp[width - 1] = 10 * log10f(temp[width - 1]);
    }

    png_util_set_data(temp, png);

    iq_file_skip(skipPerRow, file);
    current_row++;
  }

  png_util_write_image(png);

  spectrogram_shutdown();

  fftwf_destroy_plan(p);
  fftwf_free(in);
  fftwf_free(out);
  free(temp);

  return EXIT_SUCCESS;
}
