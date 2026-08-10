#ifndef IQ_FILE_H_
#define IQ_FILE_H_

#include <zlib.h>
#include <stdio.h>
#include <complex.h> // must be defined before fftw3
#include <fftw3.h>
#include <stdint.h>

#define CU8_FORMAT 0
#define CS16_FORMAT 1
#define CF32_FORMAT 2

typedef struct {
  gzFile gz;
  FILE *fp;
  int data_format;
  long gz_file_number_of_bytes;
  uint8_t *temp;
  uint32_t width;
} iq_file;

int iq_file_create(const char *filename, uint32_t samples, const char *data_format, iq_file **result);

void iq_file_skip(uint32_t samples_to_skip, iq_file *file);

int iq_file_read(fftwf_complex *in, iq_file *file);

void iq_file_get_samples(uint32_t *samples, iq_file *file);

void iq_file_destroy(iq_file *file);

#endif /* IQ_FILE_H_ */
