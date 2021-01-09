#include <stdlib.h>
#include <math.h>
#include <errno.h>

#include "xlating.h"
#include "rotator.h"

struct xlating_t {
	int decimation;
	float *taps;
	size_t taps_len;
	rotator *rot;
	float *original_taps;
};

void process(uint8_t *input, size_t input_len, float complex *output, size_t output_len, xlating *filter) {
	//FIXME
}

int create_frequency_xlating_filter(int decimation, float *taps, size_t taps_len, double center_freq, double sampling_freq, xlating **filter) {
	struct xlating_t *result = malloc(sizeof(struct xlating_t));
	result->decimation = decimation;
	result->taps_len = taps_len;
	//make code consistent - i.e. destroy all incoming memory in destory_xxx methods
	result->original_taps = taps;

	// The basic principle of this block is to perform:
	//    x(t) -> (mult by -fwT0) -> LPF -> decim -> y(t)
	// We switch things up here to:
	//    x(t) -> BPF -> decim -> (mult by fwT0*decim) -> y(t)
	// The BPF is the baseband filter (LPF) moved up to the
	// center frequency fwT0. We then apply a derotator
	// with -fwT0 to downshift the signal to baseband.
	float *bpfTaps = malloc(sizeof(float) * taps_len);
	if (bpfTaps == NULL) {
		return -ENOMEM;
	}
	float fwT0 = 2 * M_PI * center_freq / sampling_freq;
	for (int i = 0; i < taps_len; i++) {
		float complex cur = 0 + i * fwT0 * I;
		bpfTaps[i] = taps[i] * cexpf(cur);
	}
	result->taps = bpfTaps;
	float complex phase = 1.0 + 0.0 * I;
	float complex phase_incr = cexpf(0.0f + -fwT0 * decimation * I);
	rotator *rot = NULL;
	int code = create_rotator(phase, phase_incr, &rot);
	if (code != 0) {
		return code;
	}
	result->rot = rot;

	*filter = result;
	return 0;
}

int destroy_xlating(xlating *filter) {
	if (filter != NULL) {
		if (filter->taps != NULL) {
			free(filter->taps);
		}
		if (filter->original_taps != NULL) {
			free(filter->original_taps);
		}
		destroy_rotator(filter->rot);
		free(filter);
	}
	return 0;
}
