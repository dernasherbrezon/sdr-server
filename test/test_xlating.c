#include <stdlib.h>
#include <check.h>
#include "../src/xlating.h"
#include "../src/lpf.h"

xlating *filter = NULL;

START_TEST (test_max_input_buffer_size) {
	float *taps = NULL;
	size_t len;
	int code = create_low_pass_filter(1.0, 8000, 1750, 500, &taps, &len);
	ck_assert_int_eq(code, 0);
	code = create_frequency_xlating_filter(4, taps, len, 0.0, 0.0, 1024, &filter);
	ck_assert_int_eq(code, 0);
}
END_TEST

START_TEST (test_parital_input_buffer_size) {
}
END_TEST

START_TEST (test_parital_input_less_taps) {
}
END_TEST

void teardown() {
	destroy_xlating(filter);
	filter = NULL;
}

void setup() {
//do nothing
}

Suite* common_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("xlating");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_max_input_buffer_size);
	tcase_add_test(tc_core, test_parital_input_buffer_size);
	tcase_add_test(tc_core, test_parital_input_less_taps);

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

