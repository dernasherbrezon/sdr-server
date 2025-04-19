#include <stdlib.h>
#include <unity.h>
#include "../src/lpf.h"

float *taps = NULL;

void test_bounds1() {
  size_t len;
  int code = create_low_pass_filter(1.0f, 0, 1750, 500, &taps, &len);
  TEST_ASSERT_EQUAL_INT(code, -1);
}

void test_bounds2() {
  size_t len;
  int code = create_low_pass_filter(1.0f, 8000, 5000, 500, &taps, &len);
  TEST_ASSERT_EQUAL_INT(code, -1);
}

void test_bounds3() {
  size_t len;
  int code = create_low_pass_filter(1.0f, 8000, 1750, 0, &taps, &len);
  TEST_ASSERT_EQUAL_INT(code, -1);
}

void test_lowpassTaps() {
  size_t len;
  int code = create_low_pass_filter(1.0f, 8000, 1750, 500, &taps, &len);
  TEST_ASSERT_EQUAL_INT(code, 0);

  const float expected_taps[] = {0.00111410965f, -0.000583702058f, -0.00192639488f, 2.30933896e-18f, 0.00368289859f, 0.00198723329f, -0.0058701504f, -0.00666110823f, 0.0068643163f, 0.0147596458f, -0.00398709066f, -0.0259727165f, -0.0064281947f, 0.0387893915f, 0.0301109217f, -0.0507995859f,
                                 -0.0833103433f, 0.0593735874f, 0.310160041f, 0.437394291f, 0.310160041f, 0.0593735874f, -0.0833103433f, -0.0507995859f, 0.0301109217f, 0.0387893915f, -0.0064281947f, -0.0259727165f, -0.00398709066f, 0.0147596458f, 0.0068643163f, -0.00666110823f, -0.0058701504f,
                                 0.00198723329f,
                                 0.00368289859f, 2.30933896e-18f, -0.00192639488f, -0.000583702058f, 0.00111410965f};

  TEST_ASSERT_EQUAL_INT(len, 39);
  for (int i = 0; i < len; i++) {
    TEST_ASSERT_EQUAL_INT((int32_t) (expected_taps[i] * 10000), (int32_t) (taps[i] * 10000));
  }
}

void tearDown() {
  if (taps != NULL) {
    free(taps);
    taps = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_lowpassTaps);
  RUN_TEST(test_bounds1);
  RUN_TEST(test_bounds2);
  RUN_TEST(test_bounds3);
  return UNITY_END();
}

