
#ifndef UTILS_H_
#define UTILS_H_

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

#include "../src/config.h"

void setup_input_cu8(uint8_t **input, size_t input_offset, size_t len);
void setup_input_cs16(int16_t **input, size_t input_offset, size_t len);
void setup_input_cs8(int8_t **input, size_t input_offset, size_t len);
void setup_input_cf32(float **input, size_t input_offset, size_t len);

void setup_file(const char *filename, size_t len, int format);
void setup_gzfile(const char *filename, size_t len, int format);

void assert_png(const char *expected, const char *actual);

void assert_cf32(const float expected[], size_t expected_size, float complex *actual, size_t actual_size);
void assert_cs16(const int16_t expected[], size_t expected_size, int16_t *actual, size_t actual_size);
void assert_float_array(const float expected[], size_t expected_size, const float *actual, size_t actual_size);
void assert_file(struct server_config *config, int id, const float expected[], size_t expected_size);
void assert_gzfile(struct server_config *config, int id, const float expected[], size_t expected_size);

#endif /* UTILS_H_ */
