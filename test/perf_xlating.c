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
	int code = create_low_pass_filter(1.0f, sampling_freq, target_freq / 2, 2000, &taps, &len);
	if (code != 0) {
		exit(EXIT_FAILURE);
	}
	size_t max_input = 200000;
	xlating *filter = NULL;
	code = create_frequency_xlating_filter((int) (sampling_freq / target_freq), taps, len, -12000, sampling_freq, max_input, &filter);
	if (code != 0) {
		exit(EXIT_FAILURE);
	}
	uint8_t *input = NULL;
	input = malloc(sizeof(uint8_t) * max_input);
	if (input == NULL) {
		exit(EXIT_FAILURE);
	}
	for (size_t i = 0; i < max_input; i++) {
		// don't care about the loss of data
		input[i] = (uint8_t) (i);
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
	// completed in: 0.005615 seconds
	// tuned kernel:
	// completed in: 0.002649 seconds
	// tuned cu8 -> cf32:
	// completed in: 0.002038 seconds
	// NO_MANUAL_SIMD
	// completed in: 0.002902 seconds
	// manual simd (avx)
	// completed in: 0.002093 seconds

	// MacBook Air M1
	// VOLK_GENERIC=1:
	// completed in: 0.001693 seconds
	// tuned kernel:
	// completed in: 0.001477 seconds

	// Raspberrypi 3
	// VOLK_GENERIC=1:
	// completed in: 0.073828 seconds
	// tuned kernel:
	// completed in: 0.024855 seconds

	// Raspberrypi 4
	// VOLK_GENERIC=1:
	// completed in: 0.041116 seconds
	// tuned kernel:
	// completed in: 0.013621 seconds
	// NO_MANUAL_SIMD
	// completed in: 0.039529 seconds
	// manual simd
	// completed in: 0.011978 seconds

    // Raspberrypi 1
    // VOLK_GENERIC=1:
    // completed in: 0.291598 seconds
    // tuned kernel:
    // completed in: 0.332934 seconds

    // Intel(R) Core(TM) i5-7500 CPU @ 3.40GHz
    // VOLK_GENERIC=1:
    // completed in: 0.003249 seconds
    // tuned kernel:
    // completed in: 0.001609 seconds
	// NO_MANUAL_SIMD
	// completed in: 0.001603 seconds
	// manual simd
	// completed in: 0.001605 seconds
	printf("completed in: %f seconds\n", time_spent / total_executions);
	return 0;
}
