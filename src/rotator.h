#ifndef SRC_ROTATOR_H_
#define SRC_ROTATOR_H_

#include <complex.h>

typedef struct rotator_t rotator;

int create_rotator(float complex phase, float complex phase_incr, rotator **rotator);

float complex rotator_increment(rotator *rotator, float complex input);

int destroy_rotator(rotator *rotator);

#endif /* SRC_ROTATOR_H_ */
