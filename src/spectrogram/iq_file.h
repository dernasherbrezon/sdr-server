#ifndef IQ_FILE_H_
#define IQ_FILE_H_

#include <zlib.h>
#include <stdio.h>
#include <fftw3.h>

typedef struct {
  gzFile gz;
  FILE *fp;
  char *data_format;
} iq_file;

int iq_file_create(char *filename, char *data_format, iq_file **result);

void iq_file_skip(uint32_t samples_to_skip, iq_file *file);

int iq_file_read(fftwf_complex *in, uint32_t samples, iq_file *file);

void iq_file_get_samples(uint32_t *samples, iq_file *file);

void iq_file_destroy(iq_file *file);

#endif /* IQ_FILE_H_ */