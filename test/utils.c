#include "utils.h"

#include <png.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unity.h>
#include <zlib.h>

typedef struct {
  uint32_t width;
  uint32_t height;
  int color_type;
  int bit_depth;
  size_t rowbytes;
  png_bytepp rows;
} png_image_data;

static void read_png_file(const char *filename, png_image_data *result) {
  FILE *fp = fopen(filename, "rb");
  TEST_ASSERT(fp != NULL);

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  TEST_ASSERT(png_ptr != NULL);
  png_infop info_ptr = png_create_info_struct(png_ptr);
  TEST_ASSERT(info_ptr != NULL);

  if (setjmp(png_jmpbuf(png_ptr))) {
    TEST_FAIL_MESSAGE("unable to read png file");
  }

  png_init_io(png_ptr, fp);
  png_read_info(png_ptr, info_ptr);

  result->width = png_get_image_width(png_ptr, info_ptr);
  result->height = png_get_image_height(png_ptr, info_ptr);
  result->color_type = png_get_color_type(png_ptr, info_ptr);
  result->bit_depth = png_get_bit_depth(png_ptr, info_ptr);

  png_read_update_info(png_ptr, info_ptr);
  result->rowbytes = png_get_rowbytes(png_ptr, info_ptr);

  result->rows = malloc(sizeof(png_bytep) * result->height);
  TEST_ASSERT(result->rows != NULL);
  for (uint32_t y = 0; y < result->height; y++) {
    result->rows[y] = malloc(result->rowbytes);
    TEST_ASSERT(result->rows[y] != NULL);
  }

  png_read_image(png_ptr, result->rows);

  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  fclose(fp);
}

static void free_png_image_data(png_image_data *data) {
  if (data->rows == NULL) {
    return;
  }
  for (uint32_t y = 0; y < data->height; y++) {
    free(data->rows[y]);
  }
  free(data->rows);
}

void assert_png(const char *expected, const char *actual) {
  png_image_data expected_data = {0};
  png_image_data actual_data = {0};

  read_png_file(expected, &expected_data);
  read_png_file(actual, &actual_data);

  TEST_ASSERT_EQUAL_UINT32(expected_data.width, actual_data.width);
  TEST_ASSERT_EQUAL_UINT32(expected_data.height, actual_data.height);
  TEST_ASSERT_EQUAL_INT(expected_data.color_type, actual_data.color_type);
  TEST_ASSERT_EQUAL_INT(expected_data.bit_depth, actual_data.bit_depth);

  for (uint32_t y = 0; y < expected_data.height; y++) {
    TEST_ASSERT_EQUAL_MEMORY(expected_data.rows[y], actual_data.rows[y], expected_data.rowbytes);
  }

  free_png_image_data(&expected_data);
  free_png_image_data(&actual_data);
}

void setup_file_cu8(const char *filename, size_t len) {
  FILE *fp = fopen(filename, "wb");
  TEST_ASSERT(fp != NULL);
  uint8_t *buffer = malloc(sizeof(uint8_t) * len);
  TEST_ASSERT(buffer != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    buffer[i] = (uint8_t) i;
  }
  size_t written = fwrite(buffer, sizeof(uint8_t), len, fp);
  TEST_ASSERT(written == len);
  free(buffer);
  fclose(fp);
}

void setup_input_cu8(uint8_t **input, size_t input_offset, size_t len) {
  uint8_t *result = malloc(sizeof(uint8_t) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (uint8_t) (input_offset + i);
  }
  *input = result;
}

void setup_input_cs8(int8_t **input, size_t input_offset, size_t len) {
  int8_t *result = malloc(sizeof(int8_t) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (int8_t) (input_offset + i);
  }
  *input = result;
}

void setup_input_cs16(int16_t **input, size_t input_offset, size_t len) {
  int16_t *result = malloc(sizeof(int16_t) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (int16_t) (input_offset + i) - (int16_t) (len / 2);
  }
  *input = result;
}

void assert_cf32(const float expected[], size_t expected_size, float complex *actual, size_t actual_size) {
  TEST_ASSERT_EQUAL_INT(expected_size, actual_size);
  for (size_t i = 0, j = 0; i < expected_size * 2; i += 2, j++) {
    TEST_ASSERT_EQUAL_INT((int32_t)(expected[i] * 10000), (int32_t)(crealf(actual[j]) * 10000));
    TEST_ASSERT_EQUAL_INT((int32_t)(expected[i + 1] * 10000), (int32_t)(cimagf(actual[j]) * 10000));
  }
}

void assert_cs16(const int16_t expected[], size_t expected_size, int16_t *actual, size_t actual_size) {
  TEST_ASSERT_EQUAL_INT(expected_size, actual_size);
  for (size_t i = 0; i < expected_size * 2; i++) {
    TEST_ASSERT_EQUAL_INT(expected[i], actual[i]);
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
  assert_cf32(expected, expected_size, (float complex *) buffer, actual_size);
  free(buffer);
}

static int read_gzfile_fully(gzFile f, void *result, size_t len) {
  size_t left = len;
  while (left > 0) {
    int received = gzread(f, (char *) result + (len - left), left);
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
  assert_cf32(expected, expected_size, (float complex *) buffer, actual_size);
  free(buffer);
}
