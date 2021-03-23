#include <stdlib.h>
#include <check.h>
#include "../src/queue.h"

queue *queue_obj = NULL;

void assert_buffers(const uint8_t *expected, int expected_len, uint8_t *actual, int actual_len) {
	ck_assert_int_eq(expected_len, actual_len);
	for (int i = 0; i < expected_len; i++) {
		ck_assert_int_eq(expected[i], actual[i]);
	}
}

void assert_buffer(const uint8_t *expected, int expected_len) {
	uint8_t *result = NULL;
	int len = 0;
	take_buffer_for_processing(&result, &len, queue_obj);
	// TODO doesn't work. segmentation fault if result is null
	ck_assert(result != NULL);
	assert_buffers(expected, expected_len, result, len);
	complete_buffer_processing(queue_obj);
}

START_TEST (test_terminated_only_after_fully_processed) {
	int code = create_queue(262144, 10, &queue_obj);
	ck_assert_int_eq(code, 0);

	const uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	queue_put(buffer, sizeof(buffer), queue_obj);

	interrupt_waiting_the_data(queue_obj);

	assert_buffer(buffer, 10);
}
END_TEST

START_TEST (test_put_take) {
	int code = create_queue(262144, 10, &queue_obj);
	ck_assert_int_eq(code, 0);

	const uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	queue_put(buffer, sizeof(buffer), queue_obj);

	const uint8_t buffer2[2] = { 1, 2 };
	queue_put(buffer2, sizeof(buffer2), queue_obj);

	assert_buffer(buffer, 10);
	assert_buffer(buffer2, 2);
}
END_TEST

START_TEST (test_overflow) {
	int code = create_queue(262144, 1, &queue_obj);
	ck_assert_int_eq(code, 0);

	const uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	queue_put(buffer, sizeof(buffer), queue_obj);
	const uint8_t buffer2[9] = { 11, 12, 13, 14, 15, 16, 17, 18, 19 };
	queue_put(buffer2, sizeof(buffer2), queue_obj);

	assert_buffer(buffer2, 9);
}
END_TEST

void teardown() {
	destroy_queue(queue_obj);
}

void setup() {
	//do nothing
}

Suite* common_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("queue");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_put_take);
	tcase_add_test(tc_core, test_overflow);
	tcase_add_test(tc_core, test_terminated_only_after_fully_processed);

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

