#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unity.h>
#include <zlib.h>

void setup_input_cu8(uint8_t **input, size_t input_offset, size_t len) {
  uint8_t *result = malloc(sizeof(uint8_t) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (uint8_t)(input_offset + i);
  }
  *input = result;
}

void setup_input_cs8(int8_t **input, size_t input_offset, size_t len) {
  int8_t *result = malloc(sizeof(int8_t) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (int8_t)(input_offset + i);
  }
  *input = result;
}

void setup_input_cs16(int16_t **input, size_t input_offset, size_t len) {
  int16_t *result = malloc(sizeof(int16_t) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (int16_t)(input_offset + i) - (int16_t)(len / 2);
  }
  *input = result;
}

void assert_complex(const float expected[], size_t expected_size, float complex *actual, size_t actual_size) {
  TEST_ASSERT_EQUAL_INT(expected_size, actual_size);
  for (size_t i = 0, j = 0; i < expected_size * 2; i += 2, j++) {
    TEST_ASSERT_EQUAL_INT((int32_t)(expected[i] * 10000), (int32_t)(crealf(actual[j]) * 10000));
    TEST_ASSERT_EQUAL_INT((int32_t)(expected[i + 1] * 10000), (int32_t)(cimagf(actual[j]) * 10000));
  }
}

void assert_complex_cs16(const float expected[], size_t expected_size, int16_t *actual, size_t actual_size) {
  TEST_ASSERT_EQUAL_INT(expected_size, 2 * actual_size);
  for (size_t i = 0; i < expected_size * 2; i++) {
    TEST_ASSERT_EQUAL_INT((int32_t)(expected[i] * 10000), (int32_t)((actual[i] / (1 << 15)) * 10000));
  }
}

void assert_float_array(const float expected[], size_t expected_size, const float *actual, size_t actual_size) {
  TEST_ASSERT_EQUAL_INT(expected_size, actual_size);
  for (size_t i = 0; i < expected_size; i++) {
    TEST_ASSERT_EQUAL_INT((int32_t)(expected[i] * 10000), (int32_t)(actual[i] * 10000));
  }
}

void assert_file(struct server_config *config, int id, const float expected[], size_t expected_size) {
  char file_path[4096];
  snprintf(file_path, sizeof(file_path), "%s/%d.cf32", config->base_path, id);
  fprintf(stdout, "checking: %s\n", file_path);
  FILE *f = fopen(file_path, "rb");
  TEST_ASSERT(f != NULL);
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  TEST_ASSERT(fsize > 0);
  fseek(f, 0, SEEK_SET);
  uint8_t *buffer = malloc(fsize);
  TEST_ASSERT(buffer != NULL);
  fread(buffer, 1, fsize, f);
  fclose(f);
  size_t actual_size = fsize / sizeof(float complex);
  assert_complex(expected, expected_size, (float complex *)buffer, actual_size);
  free(buffer);
}

static int read_gzfile_fully(gzFile f, void *result, size_t len) {
  size_t left = len;
  while (left > 0) {
    int received = gzread(f, (char *)result + (len - left), left);
    if (received <= 0) {
      perror("unable read the file");
      return -1;
    }
    left -= received;
  }
  return 0;
}

void assert_gzfile(struct server_config *config, int id, const float expected[], size_t expected_size) {
  char file_path[4096];
  snprintf(file_path, sizeof(file_path), "%s/%d.cf32.gz", config->base_path, id);
  fprintf(stdout, "checking: %s\n", file_path);
  gzFile f = gzopen(file_path, "rb");
  TEST_ASSERT(f != NULL);
  size_t expected_size_bytes = sizeof(float complex) * expected_size;
  uint8_t *buffer = malloc(expected_size_bytes);
  TEST_ASSERT(buffer != NULL);
  int code = read_gzfile_fully(f, buffer, expected_size_bytes);
  gzclose(f);
  TEST_ASSERT_EQUAL_INT(0, code);
  size_t actual_size = expected_size;
  assert_complex(expected, expected_size, (float complex *)buffer, actual_size);
  free(buffer);
}
