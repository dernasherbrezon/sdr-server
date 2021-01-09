#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#include "lpf.h"

int sanity_check_1f(double sampling_freq, double fa, double transition_width) {
	if (sampling_freq <= 0.0) {
		fprintf(stderr, "sampling frequency should be positive\n");
		return -1;
	}

	if (fa <= 0.0 || fa > sampling_freq / 2) {
		fprintf(stderr, "cutoff frequency should be positive and less than sampling freq / 2. got: %f\n", fa);
		return -1;
	}

	if (transition_width <= 0) {
		fprintf(stderr, "transition width should be positive\n");
		return -1;
	}

	return 0;
}

size_t computeNtaps(double sampling_freq, double transition_width) {
	double a = 53;
	size_t ntaps = (size_t) (a * sampling_freq / (22.0 * transition_width));
	if ((ntaps & 1) == 0) { // if even...
		ntaps++; // ...make odd
	}
	return ntaps;
}

int create_hamming_window(int ntaps, float **output) {
	float *result = malloc(sizeof(float) * ntaps);
	if (result == NULL) {
		return -ENOMEM;
	}
	int m = ntaps - 1;
	for (int n = 0; n < ntaps; n++) {
		result[n] = (float) (0.54 - 0.46 * cos((2 * M_PI * n) / m));
	}
	*output = result;
	return 0;
}

int create_low_pass_filter(double gain, double sampling_freq, double cutoff_freq, double transition_width, float **output_taps, size_t *len) {
	int result = sanity_check_1f(sampling_freq, cutoff_freq, transition_width);
	if (result != 0) {
		return result;
	}

	size_t ntaps = computeNtaps(sampling_freq, transition_width);
	float *taps = malloc(sizeof(float) * ntaps);
	if (taps == NULL) {
		return -ENOMEM;
	}
	float *w = NULL;
	result = create_hamming_window(ntaps, &w);
	if (result != 0) {
		return result;
	}

	int M = (ntaps - 1) / 2;
	double fwT0 = 2 * M_PI * cutoff_freq / sampling_freq;

	for (int n = -M; n <= M; n++) {
		if (n == 0) {
			taps[n + M] = fwT0 / M_PI * w[n + M];
		} else {
			// a little algebra gets this into the more familiar sin(x)/x form
			taps[n + M] = sin(n * fwT0) / (n * M_PI) * w[n + M];
		}
	}

	free(w);

	double fmax = taps[0 + M];
	for (int n = 1; n <= M; n++) {
		fmax += 2 * taps[n + M];
	}

	gain /= fmax; // normalize

	for (int i = 0; i < ntaps; i++) {
		taps[i] *= gain;
	}

	*output_taps = taps;
	*len = ntaps;
	return 0;
}

