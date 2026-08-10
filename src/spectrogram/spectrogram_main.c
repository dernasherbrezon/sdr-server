#include <stdint.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include "spectrogram.h"

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


int main(int argc, char **argv) {
  spectrogram spec;
  spec.sampling_rate = 48000;
  spec.width = 1024;
  spec.data_format = "cu8";
  spec.input_file = NULL;
  spec.output_file = NULL;
  spec.fftw_flags = "FFTW_MEASURE";

  int dopt;
  while ((dopt = getopt(argc, argv, "hw:s:d:i:o:f:")) != EOF) {
    switch (dopt) {
      case 'h':
        spectrogram_usage();
        return EXIT_SUCCESS;
      case 'w':
        spec.width = atoi(optarg);
        break;
      case 's':
        spec.sampling_rate = (uint32_t) atof(optarg);
        break;
      case 'd':
        spec.data_format = optarg;
        break;
      case 'i':
        spec.input_file = optarg;
        break;
      case 'o':
        spec.output_file = optarg;
        break;
      case 'f':
        spec.fftw_flags = optarg;
        break;
      default:
        exit(EXIT_FAILURE);
    }
  }

  signal(SIGINT, spectrogram_sighandler);
  signal(SIGHUP, spectrogram_sighandler);
  signal(SIGTERM, spectrogram_sighandler);

  return spectrogram_main(&spec);
}
