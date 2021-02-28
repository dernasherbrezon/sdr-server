#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "../src/xlating.h"
#include "../src/lpf.h"

int main(void) {
	uint32_t sampling_freq = 2016000;
	uint32_t target_freq = 48000;
	float *taps = NULL;
	size_t len;
	int code = create_low_pass_filter(1.0, sampling_freq, target_freq / 2, 2000, &taps, &len);
	if (code != 0) {
		exit(EXIT_FAILURE);
	}
	size_t max_input = 200000;
	xlating *filter = NULL;
	code = create_frequency_xlating_filter((int) (sampling_freq / target_freq), taps, len, -12000, sampling_freq, max_input, &filter);
	if (code != 0) {
		exit(EXIT_FAILURE);
	}
	float *input = NULL;
	input = malloc(sizeof(float) * max_input);
	if (input == NULL) {
		exit(EXIT_FAILURE);
	}
	for (size_t i = 0; i < max_input; i++) {
		input[i] = i / 128.0F;
	}

	int total_executions = 1000;

	clock_t begin = clock();
	for (int i = 0; i < total_executions; i++) {
		float complex *output;
		size_t output_len = 0;
		process(input, max_input, &output, &output_len, filter);
	}
	clock_t end = clock();
	double time_spent = (double) (end - begin) / CLOCKS_PER_SEC;
	// MacBook Air
	// VOLK_GENERIC=1:
	// completed in: 0.003935 seconds
	// tuned kernel:
	// completed in: 0.002038 seconds

	// MacBook Air M1
	// VOLK_GENERIC=1:
	// completed in: 0.001693 seconds
	// tuned kernel:
	// completed in: 0.001477 seconds

	// Raspberrypi 3
	// VOLK_GENERIC=1:
	// completed in: 0.076204 seconds
	// tuned kernel:
	// completed in: 0.026148 seconds
	printf("completed in: %f seconds\n", time_spent / total_executions);
	return 0;
}
