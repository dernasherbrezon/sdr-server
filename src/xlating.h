#ifndef SRC_XLATING_H_
#define SRC_XLATING_H_

#include <complex.h>
#include <stdint.h>

typedef struct xlating_t xlating;

int create_frequency_xlating_filter(uint32_t decimation, float *taps, size_t taps_len, int32_t center_freq, uint32_t sampling_freq, uint32_t max_input_buffer_length, xlating **filter);

void process(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter);

int destroy_xlating(xlating *filter);

#endif /* SRC_XLATING_H_ */
