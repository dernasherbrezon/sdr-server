
#ifndef UTILS_H_
#define UTILS_H_

#include <complex.h>
#include <stdint.h>
#include <stddef.h>

void setup_input_cu8(uint8_t **input, size_t input_offset, size_t len);
void setup_input_cs16(int16_t **input, size_t input_offset, size_t len);
void setup_input_cf32(float **input, size_t input_offset, size_t len);
void assert_complex(const float expected[], size_t expected_size, float complex *actual, size_t actual_size);
void assert_float_array(const float expected[], size_t expected_size, float *actual, size_t actual_size);

#endif /* UTILS_H_ */
