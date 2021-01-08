#include <stdlib.h>
#include <check.h>
#include "../src/lpf.h"

float *taps = NULL;

START_TEST (test_lowpassTaps) {
	uint32_t len;
	int result = create_low_pass_filter(1.0, 8000, 1750, 500, &taps, &len);
	ck_assert_int_eq(result, 0);

	const float expected_taps[] = { 0.00111410965f, -0.000583702058f, -0.00192639488f, 2.30933896e-18f, 0.00368289859f, 0.00198723329f, -0.0058701504f, -0.00666110823f, 0.0068643163f, 0.0147596458f, -0.00398709066f, -0.0259727165f, -0.0064281947f, 0.0387893915f, 0.0301109217f, -0.0507995859f, -0.0833103433f, 0.0593735874f, 0.310160041f, 0.437394291f, 0.310160041f, 0.0593735874f, -0.0833103433f, -0.0507995859f, 0.0301109217f, 0.0387893915f, -0.0064281947f, -0.0259727165f, -0.00398709066f, 0.0147596458f, 0.0068643163f, -0.00666110823f, -0.0058701504f, 0.00198723329f,
			0.00368289859f, 2.30933896e-18f, -0.00192639488f, -0.000583702058f, 0.00111410965f };

	ck_assert_uint_eq(len, 39);
	for (int i = 0; i < len; i++) {
		ck_assert_float_eq(expected_taps[i], taps[i]);
	}
}
END_TEST

void teardown() {
	if (taps != NULL) {
		free(taps);
		taps = NULL;
	}
}

void setup() {
	//do nothing
}

Suite* common_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("lpf");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_lowpassTaps);

	tcase_add_checked_fixture(tc_core, setup, teardown);
	suite_add_tcase(s, tc_core);

	return s;
}

int main(void) {
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = common_suite();
	sr = srunner_create(s);

	srunner_set_fork_status(sr, CK_NOFORK);
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

