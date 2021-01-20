#include <stdlib.h>
#include <check.h>
#include <stdbool.h>
#include "mock_librtlsdr.c"


extern struct mock_status mock;
extern void init_mock_librtlsdr();

core *core_obj = NULL;
struct server_config *config = NULL;
struct client_config *client_config = NULL;
uint8_t *input = NULL;

void setup_input_data(size_t input_offset, size_t len) {
	input = malloc(sizeof(uint8_t) * len);
	ck_assert_ptr_ne(input, NULL);
	for (size_t i = 0; i < len; i++) {
		// don't care about the loss of data
		input[i] = (uint8_t) (input_offset + i);
	}
}

START_TEST (test_process_message) {
	int code = create_server_config(&config, "core.config");
	ck_assert_int_eq(code, 0);
	code = create_core(config, &core_obj);
	ck_assert_int_eq(code, 0);

	int length = 200;
	setup_input_data(0, length);
	mock.buffer = input;
	mock.len = length;

	client_config = malloc(sizeof(struct client_config));
	client_config->core = core_obj;
	client_config->id = 0;
	client_config->is_running = true;
	client_config->sampling_rate = 9600 * 2;
	client_config->band_freq = 460100200;
	client_config->center_freq = -12000 + client_config->band_freq;
	code = add_client(client_config);
	ck_assert_int_eq(code, 0);
}
END_TEST

void teardown() {
	destroy_core(core_obj);
	core_obj = NULL;
	destroy_server_config(config);
	config = NULL;
	if (client_config != NULL) {
		free(client_config);
		client_config = NULL;
	}
	if (input != NULL) {
		free(input);
		input = NULL;
	}
}

void setup() {
	init_mock_librtlsdr();
	//do nothing
}

Suite* common_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("core");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_process_message);

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

