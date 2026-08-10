#include "iq_file.h"
#include <stdlib.h>
#include <string.h>

int iq_file_create(char *filename, uint32_t samples, char *data_format, iq_file **file) {
  iq_file *result = malloc(sizeof(iq_file));
  if (result == NULL) {
    return 1;
  }
  *result = (iq_file){0};
  result->width = samples;
  if (strcmp(data_format, "cu8") == 0) {
    result->data_format = CU8_FORMAT;
    result->temp = malloc(2 * sizeof(uint8_t) * result->width);
    if (result->temp == NULL) {
      iq_file_destroy(result);
      return EXIT_FAILURE;
    }
  } else if (strcmp(data_format, "cf32") == 0) {
    result->data_format = CF32_FORMAT;
  } else if (strcmp(data_format, "cs16") == 0) {
    result->data_format = CS16_FORMAT;
    result->temp = malloc(2 * sizeof(int16_t) * result->width);
    if (result->temp == NULL) {
      iq_file_destroy(result);
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "unsupported data format %s\n", data_format);
    iq_file_destroy(result);
    return EXIT_FAILURE;
  }

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
      fprintf(stderr, "premature end of file %s\n", filename);
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
    int code = gzbuffer(result->gz, 128 * 1024);
    if (code != 0) {
      fprintf(stderr, "unable to increase zlib buffer. continue with default 8Kb\n");
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
  long bytes_to_skip;
  if (file->data_format == CF32_FORMAT) {
    bytes_to_skip = (long) (sizeof(fftwf_complex) * samples_to_skip);
  } else if (file->data_format == CS16_FORMAT) {
    bytes_to_skip = (long) (sizeof(int16_t) * 2 * samples_to_skip);
  } else if (file->data_format == CU8_FORMAT) {
    bytes_to_skip = (long) (sizeof(uint8_t) * 2 * samples_to_skip);
  } else {
    return;
  }
  if (file->fp != NULL) {
    fseek(file->fp, bytes_to_skip, SEEK_CUR);
  }
  if (file->gz != NULL) {
    gzseek(file->gz, bytes_to_skip, SEEK_CUR);
  }
}

int iq_file_read(fftwf_complex *in, iq_file *file) {
  if (file->data_format == CF32_FORMAT) {
    if (file->fp != NULL) {
      size_t actually_read = fread(in, sizeof(fftwf_complex), file->width, file->fp);
      if (actually_read != file->width) {
        return 1;
      }
    } else if (file->gz != NULL) {
      int expected = (int) (sizeof(fftwf_complex) * file->width);
      int actually_read = gzread(file->gz, in, expected);
      if (actually_read != expected) {
        return 1;
      }
    } else {
      return 1;
    }
    return 0;
  }
  if (file->data_format == CU8_FORMAT) {
    if (file->fp != NULL) {
      size_t actually_read = fread(file->temp, 2 * sizeof(uint8_t), file->width, file->fp);
      if (actually_read != file->width) {
        return 1;
      }
    } else if (file->gz != NULL) {
      int expected = (int) (2 * sizeof(uint8_t) * file->width);
      int actually_read = gzread(file->gz, file->temp, expected);
      if (actually_read != expected) {
        return 1;
      }
    } else {
      return 1;
    }
    for (size_t i = 0; i < file->width; i++) {
      float real = ((float) file->temp[2 * i] - 127.5F) / 128.0F;
      float imag = ((float) file->temp[2 * i + 1] - 127.5F) / 128.0F;
      in[i] = real + imag * I;
    }
    return 0;
  }
  if (file->data_format == CS16_FORMAT) {
    if (file->fp != NULL) {
      size_t actually_read = fread(file->temp, 2 * sizeof(int16_t), file->width, file->fp);
      if (actually_read != file->width) {
        return 1;
      }
    } else if (file->gz != NULL) {
      int expected = (int) (2 * sizeof(int16_t) * file->width);
      int actually_read = gzread(file->gz, file->temp, expected);
      if (actually_read != expected) {
        return 1;
      }
    } else {
      return 1;
    }
    for (size_t i = 0; i < file->width; i++) {
      int16_t v1 = (int16_t) ((file->temp[4 * i + 1] << 8) | file->temp[4 * i]);
      int16_t v2 = (int16_t) ((file->temp[4 * i + 3] << 8) | file->temp[4 * i + 2]);
      float real = ((float) v1) / 32768.0F;
      float imag = ((float) v2) / 32768.0F;
      in[i] = real + imag * I;
    }
    return 0;
  }
  return 1;
}

void iq_file_get_samples(uint32_t *samples, iq_file *file) {
  size_t sample_size;
  if (file->data_format == CF32_FORMAT) {
    sample_size = sizeof(fftwf_complex);
  } else if (file->data_format == CU8_FORMAT) {
    sample_size = 2 * sizeof(uint8_t);
  } else if (file->data_format == CS16_FORMAT) {
    sample_size = 2 * sizeof(int16_t);
  } else {
    *samples = 0;
    return;
  }
  if (file->fp != NULL) {
    fseek(file->fp, 0L, SEEK_END);
    long number_of_bytes = ftell(file->fp);
    *samples = number_of_bytes / sample_size;
    rewind(file->fp);
  }
  if (file->gz != NULL) {
    *samples = file->gz_file_number_of_bytes / sample_size;
  }
}

void iq_file_destroy(iq_file *file) {
  if (file == NULL) {
    return;
  }

  if (file->temp != NULL) {
    free(file->temp);
  }
  if (file->fp != NULL) {
    fclose(file->fp);
  }
  if (file->gz != NULL) {
    gzclose(file->gz);
  }

  free(file);
}
