#include <stdint.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <complex.h> // should go before fftw3.h
#include <fftw3.h>
#include <zlib.h>
#include <string.h>
#include <math.h>
#include "png_util.h"

volatile sig_atomic_t do_exit = 0;
gzFile gz = NULL;
FILE *fp = NULL;
png_util *png = NULL;

static void sighandler(int signum) {
  fprintf(stderr, "Signal caught, exiting!\n");
  do_exit = 1;
}

void usage() {
  printf("Usage:\n");
  printf("  -h  print help\n");
  printf("  -w <width> image width (default: 1024)\n");
  printf("  -s <sampling_rate> sampling rate (default: 48000)\n");
  printf("  -d <data_format> data format: cu8, cf32 (default: cu8)\n");
  printf("  -i <input_file> I/Q input file\n");
  printf("  -o <output_file> spectrogram output\n");
}

// FIXME data format
static int read_fully(fftwf_complex *in, int width) {
  if (fp != NULL) {
    size_t actually_read = fread(in, sizeof(fftwf_complex), width, fp);
    if (actually_read != width) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}

static int skip(uint32_t samples_to_skip) {
  if (fp != NULL) {
    fseek(fp, sizeof(fftwf_complex) * samples_to_skip, SEEK_CUR);
  }
  return 0;
}

static int calculate_height(uint32_t *samples) {
  if (fp != NULL) {
    fseek(fp, 0L, SEEK_END);
    long number_of_bytes = ftell(fp);
    *samples = number_of_bytes / sizeof(float) / 2;
    rewind(fp);
  }
  return 0;
}

int main(int argc, char **argv) {
  uint32_t sampling_rate = 48000;
  int width = 1024;
  char *data_format = "cu8";
  char *input_file = NULL;
  char *output_file = NULL;

  int dopt;
  while ((dopt = getopt(argc, argv, "hw:s:d:i:o:")) != EOF) {
    switch (dopt) {
      case 'h':
        usage();
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
      default:
        exit(EXIT_FAILURE);
    }
  }

  if (input_file == NULL) {
    fprintf(stderr, "-i (input file) parameter is missing\n");
    usage();
    return EXIT_FAILURE;
  }
  if (output_file == NULL) {
    fprintf(stderr, "-o (output file) parameter is missing\n");
    usage();
    return EXIT_FAILURE;
  }

  if (strstr(input_file, ".gz") != NULL) {
    gz = gzopen(input_file, "rb");
    if (gz == NULL) {
      fprintf(stderr, "unable to read input file %s\n", input_file);
      return EXIT_FAILURE;
    }
  } else {
    fp = fopen(input_file, "rb");
    if (fp == NULL) {
      fprintf(stderr, "unable to read input file %s\n", input_file);
      return EXIT_FAILURE;
    }
  }

  uint32_t numberOfFftPerRow = sampling_rate / width;
  uint32_t skipPerRow = sampling_rate % width;
  int half_width = width / 2;

  uint32_t samples = 0;
  int code = calculate_height(&samples);
  if (code != 0) {
    //FIXME
    return EXIT_FAILURE;
  }

  uint32_t height = samples / sampling_rate;

  FILE *png_fp = fopen(output_file, "wb");
  if (png_fp == NULL) {
    //FIXME
    return EXIT_FAILURE;
  }

  code = png_util_init(width, height, png_fp, &png);
  if (code != 0) {
    png_util_destroy(png);
    //FIXME more de-init
    return EXIT_FAILURE;
  }

  float *temp = malloc(sizeof(float) * width);
  if (temp == NULL) {
    return EXIT_FAILURE;
  }
  float normalization_factor = 1.0f / width;

  fftwf_complex *in = fftwf_malloc(sizeof(fftwf_complex) * width);
  fftwf_complex *out = fftwf_malloc(sizeof(fftwf_complex) * width);
  fftwf_plan p = fftwf_plan_dft_1d(width, in, out, FFTW_FORWARD, FFTW_ESTIMATE); // TODO switch to FFTW_MEASURE
  uint32_t current_row = 0;
  while (!do_exit && current_row < height) {
    for (int j = 0; j < width; j++) {
      temp[j] = -255.0f;
    }

    for (int i = 0; i < numberOfFftPerRow; i++) {
      code = read_fully(in, width);
      if (code != 0) {
        break;
      }
      fftwf_execute(p);
      for (int j = 0; j < width; j++) {
        fftwf_complex cur = out[j] * normalization_factor;
        float power = 10 * log10f(crealf(cur) * crealf(cur) + cimagf(cur) * cimagf(cur) + 1e-20f);
        temp[j] = fmaxf(temp[j], power);
      }
    }

    for (int j = 0; j < half_width; j++) {
      float cur = temp[j];
      temp[j] = temp[half_width + j];
      temp[half_width + j] = cur;
    }

    png_util_set_data(current_row, temp, png);

    skip(skipPerRow);
    current_row++;
  }

  png_util_write_image(png);
  png_util_destroy(png);

  fftwf_destroy_plan(p);
  fftwf_free(in);
  fftwf_free(out);
  free(temp);

  return EXIT_SUCCESS;
}
