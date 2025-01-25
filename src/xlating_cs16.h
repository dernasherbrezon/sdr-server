#ifndef SRC_XLATING_CS16_H_
#define SRC_XLATING_CS16_H_

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

typedef struct xlating_cs16_t xlating_cs16;

int create_frequency_xlating_cs16_filter(uint32_t decimation, float *taps, size_t taps_len, int32_t center_freq, uint32_t sampling_freq, uint32_t max_input_buffer_length, xlating_cs16 **filter);

void process_cu8_cs16(const uint8_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating_cs16 *filter);

void process_cs8_cs16(const int8_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating_cs16 *filter);

void process_cs16_cs16(const int16_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating_cs16 *filter);

void destroy_xlating_cs16(xlating_cs16 *filter);

#endif /* SRC_XLATING_CS16_H_ */