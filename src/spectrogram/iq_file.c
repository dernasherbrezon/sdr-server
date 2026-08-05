#include "iq_file.h"
#include <stdlib.h>
#include <string.h>

int iq_file_create(char *filename, char *data_format, iq_file **file) {
  iq_file *result = malloc(sizeof(iq_file));
  if (result == NULL) {
    return 1;
  }
  *result = (iq_file){0};
  result->data_format = data_format;

  if (strstr(filename, ".gz") != NULL) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
      fprintf(stderr, "unable to read input file %s\n", filename);
      iq_file_destroy(result);
      return EXIT_FAILURE;
    }
    if (fseek(fp, -4, SEEK_END) != 0) {
      fclose(fp);
      iq_file_destroy(result);
      return EXIT_FAILURE;
    }
    uint8_t buf[4];
    if (fread(buf, 1, 4, fp) != 4) {
      fclose(fp);
      iq_file_destroy(result);
      return EXIT_FAILURE;
    }
    fclose(fp);
    result->gz_file_number_of_bytes = (uint32_t) buf[0]
                                      | ((uint32_t) buf[1] << 8)
                                      | ((uint32_t) buf[2] << 16)
                                      | ((uint32_t) buf[3] << 24);
    result->gz = gzopen(filename, "rb");
    if (result->gz == NULL) {
      fprintf(stderr, "unable to read input file %s\n", filename);
      iq_file_destroy(result);
      return EXIT_FAILURE;
    }
  } else {
    result->fp = fopen(filename, "rb");
    if (result->fp == NULL) {
      fprintf(stderr, "unable to read input file %s\n", filename);
      iq_file_destroy(result);
      return EXIT_FAILURE;
    }
  }

  *file = result;
  return 0;
}

void iq_file_skip(uint32_t samples_to_skip, iq_file *file) {
  if (file->fp != NULL) {
    fseek(file->fp, sizeof(fftwf_complex) * samples_to_skip, SEEK_CUR);
  }
  if (file->gz != NULL) {
    gzseek(file->gz, sizeof(fftwf_complex) * samples_to_skip, SEEK_CUR);
  }
}

int iq_file_read(fftwf_complex *in, uint32_t samples, iq_file *file) {
  if (file->fp != NULL) {
    size_t actually_read = fread(in, sizeof(fftwf_complex), samples, file->fp);
    if (actually_read != samples) {
      return 1;
    }
  }
  if (file->gz != NULL) {
    int expected = (int) (sizeof(fftwf_complex) * samples);
    int actually_read = gzread(file->gz, in, expected);
    if (actually_read != expected) {
      return 1;
    }
  }
  return 0;
}

void iq_file_get_samples(uint32_t *samples, iq_file *file) {
  if (file->fp != NULL) {
    fseek(file->fp, 0L, SEEK_END);
    long number_of_bytes = ftell(file->fp);
    *samples = number_of_bytes / sizeof(float) / 2;
    rewind(file->fp);
  }
  if (file->gz != NULL) {
    *samples = file->gz_file_number_of_bytes / sizeof(float) / 2;
  }
}

void iq_file_destroy(iq_file *file) {
  if (file == NULL) {
    return;
  }

  if (file->fp != NULL) {
    fclose(file->fp);
  }
  if (file->gz != NULL) {
    gzclose(file->gz);
  }

  free(file);
}
