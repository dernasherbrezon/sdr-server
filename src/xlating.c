#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <volk/volk.h>

#include "xlating.h"
#include "rotator.h"

struct xlating_t {
	int decimation;
	float complex *taps;
	size_t taps_len;
	rotator *rot;
	float *original_taps;

	float complex *output;
	size_t output_len;
};

void process(int8_t *input, size_t input_len, float complex *output, size_t output_len, xlating *filter) {

	//volk_32fc_x2_dot_prod_32fc_a()
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
	float complex *bpfTaps = malloc(sizeof(float complex) * taps_len);
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
	//FIXME tune buffer
	result->output_len = 48000;
	result->output = malloc(sizeof(float complex) * result->output_len);
	if (result->output == NULL) {
		return -ENOMEM;
	}

	*filter = result;
	return 0;
}

int destroy_xlating(xlating *filter) {
	if (filter == NULL) {
		return 0;
	}
	if (filter->taps != NULL) {
		free(filter->taps);
	}
	if (filter->original_taps != NULL) {
		free(filter->original_taps);
	}
	if( filter->output != NULL ) {
		free(filter->output);
	}
	destroy_rotator(filter->rot);
	free(filter);
	return 0;
}
