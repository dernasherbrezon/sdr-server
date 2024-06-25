#include <stdlib.h>
#include <check.h>
#include "../src/config.h"

struct server_config *config = NULL;

START_TEST (test_missing_file) {
	int code = create_server_config(&config, "non-existing-configration-file.config");
	ck_assert_int_eq(code, -1);
}
END_TEST

START_TEST (test_invalid_format) {
	int code = create_server_config(&config, "invalid.format.config");
	ck_assert_int_eq(code, -1);
}
END_TEST

START_TEST (test_missing_required) {
	int code = create_server_config(&config, "invalid.config");
	ck_assert_int_eq(code, -1);
}
END_TEST

START_TEST (test_invalid_timeout) {
	int code = create_server_config(&config, "invalid2.config");
	ck_assert_int_eq(code, -1);
}
END_TEST

START_TEST (test_invalid_queue_size_config) {
    int code = create_server_config(&config, "invalid.queue_size.config");
    ck_assert_int_eq(code, -1);
}
END_TEST

START_TEST (test_minimal_config) {
	int code = create_server_config(&config, "minimal.config");
	ck_assert_int_eq(code, 0);
}
END_TEST

START_TEST (test_success) {
	int code = create_server_config(&config, "configuration.config");
	ck_assert_int_eq(code, 0);
	ck_assert_int_eq(2400000, config->band_sampling_rate);
	ck_assert_int_eq(1, config->gain_mode);
	ck_assert_int_eq(42, config->gain);
	ck_assert_int_eq(0, config->bias_t);
	ck_assert_int_eq(config->ppm, 10);
  ck_assert_int_eq(config->device_index, 1);
  ck_assert_str_eq(config->device_serial, "00000400");
	ck_assert_str_eq(config->bind_address, "127.0.0.1");
	ck_assert_int_eq(config->port, 8089);
	ck_assert_int_eq(config->buffer_size, 131072);
	ck_assert_str_eq(config->base_path, "/tmp/");
	ck_assert_int_eq(config->read_timeout_seconds, 10);
	ck_assert_int_eq(config->use_gzip, 0);
	ck_assert_int_eq(config->queue_size, 64);
	ck_assert_int_eq(config->lpf_cutoff_rate, 5);
}
END_TEST

void teardown() {
	destroy_server_config(config);
	config = NULL;
}

void setup() {
	//do nothing
}

Suite* common_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("config");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_success);
	tcase_add_test(tc_core, test_missing_required);
	tcase_add_test(tc_core, test_missing_file);
	tcase_add_test(tc_core, test_invalid_format);
	tcase_add_test(tc_core, test_minimal_config);
	tcase_add_test(tc_core, test_invalid_timeout);
    tcase_add_test(tc_core, test_invalid_queue_size_config);

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

