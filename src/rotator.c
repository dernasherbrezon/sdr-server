#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <volk/volk.h>

#include "rotator.h"

struct rotator_t {
	float complex phase;
	float complex phase_incr;
	uint32_t counter;
};

int create_rotator(float complex phase, float complex phase_incr, rotator **rotator) {
	struct rotator_t *result = malloc(sizeof(struct rotator_t));
	if (result == NULL) {
		return -ENOMEM;
	}

	result->phase = phase / cabsf(phase);
	result->phase_incr = phase_incr / cabsf(phase_incr);
	result->counter = 0;

	*rotator = result;
	return 0;
}

float complex rotator_increment(rotator *rotator, float complex input) {
	rotator->counter++;

	float complex result = input * rotator->phase; // rotate in by phase
	rotator->phase *= rotator->phase_incr;     // incr our phase (complex mult == add phases)

	if ((rotator->counter % 512) == 0) {
		rotator->phase /= cabsf(rotator->phase); // Normalize to ensure multiplication is rotation
	}

	return result;
}

void rotator_increment_batch(rotator *rotator, float complex* input, float complex* output, int batch_size) {
	volk_32fc_s32fc_x2_rotator_32fc(output, input, rotator->phase_incr, &rotator->phase, batch_size);
}

int destroy_rotator(rotator *rotator) {
	if (rotator != NULL) {
		free(rotator);
	}
	return 0;
}

