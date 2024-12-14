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

void rotator_increment_batch(rotator *rotator, float complex *input, float complex *output, int batch_size) {
  volk_32fc_s32fc_x2_rotator2_32fc(output, input, &rotator->phase_incr, &rotator->phase, batch_size);
}

void destroy_rotator(rotator *rotator) {
  if (rotator == NULL) {
    return;
  }
  free(rotator);
}

