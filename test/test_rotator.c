#include <unity.h>
#include <math.h>
#include "../src/rotator.h"
#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

rotator *rot = NULL;

void test_success_batch() {
  float complex phase = 1 + 0 * I;
  float complex phase_incr = cosf(2 * M_PI / 1003) + sinf(2 * M_PI / 1003) * I;
  int code = create_rotator(phase, phase_incr, &rot);
  TEST_ASSERT_EQUAL_INT(code, 0);
  float input[] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
  float expected[] = {1.0000000f, 0.0000000f, 0.9999804f, 0.0062644f, 0.9999216f, 0.0125285f, 0.9998235f, 0.0187921f};
  rotator_increment_batch(rot, (float complex *) input, (float complex *) input, 4);
  assert_complex(expected, 4, (float complex *) input, 4);
}

void tearDown() {
  destroy_rotator(rot);
  rot = NULL;
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_success_batch);
  return UNITY_END();
}
