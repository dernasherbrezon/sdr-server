
#ifndef UTILS_H_
#define UTILS_H_

#include <complex.h>

void setup_input_data(uint8_t **input, size_t input_offset, size_t len);
void assert_complex(const float expected[], size_t expected_size, float complex *actual, size_t actual_size);
void assert_float_array(const float expected[], size_t expected_size, float *actual, size_t actual_size);

#endif /* UTILS_H_ */
