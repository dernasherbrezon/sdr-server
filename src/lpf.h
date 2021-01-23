#ifndef SRC_LPF_H_
#define SRC_LPF_H_

#include <stdint.h>

int create_low_pass_filter(float gain, uint32_t sampling_freq, uint32_t cutoff_freq, uint32_t transition_width, float **taps, size_t *len);

#endif /* SRC_LPF_H_ */
