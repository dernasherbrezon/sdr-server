#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <volk/volk.h>
#include <limits.h>
#include <string.h>

#include "xlating.h"
#include "rotator.h"

struct xlating_t {
	int decimation;
	float complex *taps;
	size_t taps_len;
	rotator *rot;
	float *original_taps;

	float *working_buffer;
	size_t history_offset;
	size_t working_len_total;

	float *lookup_table;

	float complex *volk_output;

	float complex *output;
	size_t output_len;
};

void process(int8_t *input, size_t input_len, float complex *output, size_t output_len, xlating *filter) {
	// input_len cannot be more than (working_len_total - history_offset)

	// convert to [-1.0;1.0] working buffer
	size_t input_processed = 0;
	for (size_t i = filter->history_offset; i < filter->working_len_total && input_processed < input_len; i++, input_processed++) {
		filter->working_buffer[i] = filter->lookup_table[(uint8_t) input[input_processed]];
	}

	size_t produced = 0;
	size_t last_taps_index = filter->history_offset + input_processed - filter->taps_len;
	for (size_t i = 0; i < last_taps_index; i += filter->decimation, produced++) {
		volk_32fc_x2_dot_prod_32fc_a(filter->volk_output, (const lv_32fc_t*) (filter->working_buffer + i), filter->taps, filter->taps_len);
		filter->output[produced] = rotator_increment(filter->rot, *filter->volk_output);
	}

	memmove(filter->working_buffer, filter->working_buffer + last_taps_index - 1, filter->taps_len - 1);

	output = filter->output;
	output_len = produced;
}

int create_frequency_xlating_filter(int decimation, float *taps, size_t taps_len, double center_freq, double sampling_freq, uint32_t max_input_buffer_length, xlating **filter) {
	struct xlating_t *result = malloc(sizeof(struct xlating_t));
	if (result == NULL) {
		return -ENOMEM;
	}
	// init all fields with 0
	*result = (struct xlating_t ) { 0 };
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
	size_t alignment = volk_get_alignment();
	float complex *bpfTaps = volk_malloc(sizeof(float complex) * taps_len, alignment);
	if (bpfTaps == NULL) {
		return -ENOMEM;
	}
	float fwT0 = 2 * M_PI * center_freq / sampling_freq;
	for (size_t i = 0; i < taps_len; i++) {
		float complex cur = 0 + i * fwT0 * I;
		bpfTaps[i] = taps[i] * cexpf(cur);
	}
	// reverse taps to allow storing history in array
	for (size_t i = 0; i < taps_len / 2; i++) {
		float complex tmp = bpfTaps[taps_len - 1 - i];
		bpfTaps[taps_len - 1 - i] = bpfTaps[i];
		bpfTaps[i] = tmp;
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

	// max input length + history
	result->working_len_total = max_input_buffer_length + taps_len - 1;
	result->working_buffer = volk_malloc(sizeof(float) * result->working_len_total, alignment);
	if (result->working_buffer == NULL) {
		return -ENOMEM;
	}
	memset(result->working_buffer, 0, result->working_len_total);
	// taps_len - 1 - is a history. initially zeroes
	result->history_offset = taps_len - 1;

	result->output_len = max_input_buffer_length / 2 / decimation;
	result->output = malloc(sizeof(float complex) * result->output_len);
	if (result->output == NULL) {
		return -ENOMEM;
	}

	result->lookup_table = malloc(sizeof(float) * UCHAR_MAX);
	for (size_t i = 0; i < UCHAR_MAX; i++) {
		result->lookup_table[i] = (i - 127.5f) / 128.0f;
	}

	result->volk_output = volk_malloc(1 * sizeof(float complex), alignment);
	if (result->volk_output == NULL) {
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
		volk_free(filter->taps);
	}
	if (filter->original_taps != NULL) {
		free(filter->original_taps);
	}
	if (filter->output != NULL) {
		free(filter->output);
	}
	if (filter->working_buffer != NULL) {
		volk_free(filter->working_buffer);
	}
	if (filter->volk_output != NULL) {
		volk_free(filter->volk_output);
	}
	if (filter->lookup_table != NULL) {
		free(filter->lookup_table);
	}
	destroy_rotator(filter->rot);
	free(filter);
	return 0;
}
