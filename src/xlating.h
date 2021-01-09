#ifndef SRC_XLATING_H_
#define SRC_XLATING_H_

typedef struct xlating_t xlating;

int create_frequency_xlating_filter(int decimation, float *taps, int taps_len, double center_freq, double sampling_freq, xlating **filter);

//FIXME process

int destroy_xlating(xlating *filter);

#endif /* SRC_XLATING_H_ */
