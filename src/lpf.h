#ifndef SRC_LPF_H_
#define SRC_LPF_H_

#include <unistd.h>

int create_low_pass_filter(double gain, double sampling_freq, double cutoff_freq, double transition_width, float **taps, uint32_t *len);

#endif /* SRC_LPF_H_ */
