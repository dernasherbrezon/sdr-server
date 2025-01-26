#ifndef SRC_XLATING_H_
#define SRC_XLATING_H_

#include <complex.h>
#include <stdint.h>
#include <stddef.h>

typedef struct xlating_t xlating;

int create_frequency_xlating_filter(uint32_t decimation, float *taps, size_t taps_len, int32_t center_freq, uint32_t sampling_freq, uint32_t max_input_buffer_length, xlating **filter);

void process_native_cu8_cf32(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter);

void process_native_cs8_cf32(const int8_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter);

void process_native_cs16_cf32(const int16_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter);

void process_optimized_cu8_cf32(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter);

void process_optimized_cs8_cf32(const int8_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter);

void process_optimized_cs16_cf32(const int16_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter);

// cs16 math and output

void process_native_cu8_cs16(const uint8_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating *filter);

void process_native_cs8_cs16(const int8_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating *filter);

void process_native_cs16_cs16(const int16_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating *filter);

void process_optimized_cu8_cs16(const uint8_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating *filter);

void process_optimized_cs8_cs16(const int8_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating *filter);

void process_optimized_cs16_cs16(const int16_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating *filter);

void destroy_xlating(xlating *filter);

#endif /* SRC_XLATING_H_ */
