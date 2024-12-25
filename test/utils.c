#include <stdlib.h>
#include <unity.h>
#include "utils.h"

void setup_input_data(uint8_t **input, size_t input_offset, size_t len) {
  uint8_t *result = malloc(sizeof(uint8_t) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (uint8_t) (input_offset + i);
  }
  *input = result;
}

void setup_float_input_data(float **input, size_t input_offset, size_t len) {
  float *result = malloc(sizeof(float) * len);
  ck_assert(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = ((float) (input_offset + i)) / 128.0f;
  }
  *input = result;
}


void assert_complex(const float expected[], size_t expected_size, float complex *actual, size_t actual_size) {
  TEST_ASSERT_EQUAL_INT(expected_size, actual_size);
  for (size_t i = 0, j = 0; i < expected_size * 2; i += 2, j++) {
    TEST_ASSERT_EQUAL_INT((int32_t) (expected[i] * 10000), (int32_t) (crealf(actual[j]) * 10000));
    TEST_ASSERT_EQUAL_INT((int32_t) (expected[i + 1] * 10000), (int32_t) (cimagf(actual[j]) * 10000));
  }
}

void assert_float_array(const float expected[], size_t expected_size, float *actual, size_t actual_size) {
  TEST_ASSERT_EQUAL_INT(expected_size, actual_size);
  for (size_t i = 0; i < expected_size; i++) {
    TEST_ASSERT_EQUAL_INT((int32_t) (expected[i] * 10000), (int32_t) (actual[i] * 10000));
  }
}
