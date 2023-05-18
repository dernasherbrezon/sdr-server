#include <stdlib.h>
#include <check.h>

#include "utils.h"
#include "../src/tcp_server.h"
#include "../src/client/tcp_client.h"
#include "rtlsdr_lib_mock.h"

tcp_server *server = NULL;
core *core_obj = NULL;
struct server_config *config = NULL;
struct tcp_client *client0 = NULL;
struct tcp_client *client1 = NULL;
struct tcp_client *client2 = NULL;
uint8_t *input = NULL;

void reconnect_client() {
	destroy_client(client0);
	client0 = NULL;
	int code = create_client(config->bind_address, config->port, &client0);
	ck_assert_int_eq(code, 0);
}

void create_and_init_tcpserver() {
	int code = create_server_config(&config, "tcp_server.config");
	ck_assert_int_eq(code, 0);
	code = create_core(config, &core_obj);
	ck_assert_int_eq(code, 0);
	code = start_tcp_server(config, core_obj, &server);
	ck_assert_int_eq(code, 0);
	reconnect_client();
}

void assert_response(struct tcp_client *client, uint8_t type, uint8_t status, uint8_t details) {
	struct message_header *response_header = NULL;
	struct response *resp = NULL;
	int code = read_response(&response_header, &resp, client);
	ck_assert_int_eq(code, 0);
	ck_assert_int_eq(response_header->type, type);
	ck_assert_int_eq(resp->status, status);
	ck_assert_int_eq(resp->details, details);
	free(resp);
	free(response_header);
}

START_TEST (test_out_of_band_frequency_clients) {
	create_and_init_tcpserver();

	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

	int code = create_client(config->bind_address, config->port, &client1);
	ck_assert_int_eq(code, 0);
	send_message(client1, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 461600000, REQUEST_DESTINATION_FILE);
	assert_response(client1, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_OUT_OF_BAND_FREQ);
	destroy_client(client1);
	stop_rtlsdr_mock();

	// then the first client disconnects
	send_message(client0, PROTOCOL_VERSION, TYPE_SHUTDOWN, 0, 0, 0, 0);
	gracefully_destroy_client(client0);
	client0 = NULL;

	// now band freq is available
	code = create_client(config->bind_address, config->port, &client1);
	ck_assert_int_eq(code, 0);
	send_message(client1, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 461600000, REQUEST_DESTINATION_FILE);
	assert_response(client1, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 2);
}
END_TEST

// this test will ensure tcp server stops
// if client connected and didn't send anything
START_TEST (test_connect_and_keep_quiet) {
	create_and_init_tcpserver();
}
END_TEST

START_TEST (test_partial_request) {
	create_and_init_tcpserver();
	uint8_t buffer[] = { PROTOCOL_VERSION };
	int code = write_data(buffer, sizeof(buffer), client0);
	ck_assert_int_eq(code, 0);
}
END_TEST

START_TEST (test_invalid_request) {
	create_and_init_tcpserver();

	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 0, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 0, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 0, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, 0x99, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, 0x99, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 47000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 462400000, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 458800000, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, 0x99);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
}
END_TEST

START_TEST (test_connect_disconnect) {
	create_and_init_tcpserver();

	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

	int code = create_client(config->bind_address, config->port, &client1);
	ck_assert_int_eq(code, 0);
	send_message(client1, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client1, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 1);

	send_message(client0, PROTOCOL_VERSION, TYPE_SHUTDOWN, 0, 0, 0, 0);
	// no response here is expected

	code = create_client(config->bind_address, config->port, &client2);
	ck_assert_int_eq(code, 0);
	send_message(client2, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client2, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 2);

	send_message(client1, PROTOCOL_VERSION, TYPE_SHUTDOWN, 0, 0, 0, 0);
	// no response here is expected
}
END_TEST

START_TEST (test_connect_disconnect_single_client) {
	create_and_init_tcpserver();

	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);
}
END_TEST

START_TEST (test_disconnect_client) {
	create_and_init_tcpserver();

	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

	destroy_client(client0);
	client0 = NULL;
}
END_TEST

START_TEST (test_destination_socket) {
	// make input/output compatible with test_core
	int code = create_server_config(&config, "tcp_server.config");
	ck_assert_int_eq(code, 0);
	config->band_sampling_rate = 48000;
	code = create_core(config, &core_obj);
	ck_assert_int_eq(code, 0);
	code = start_tcp_server(config, core_obj, &server);
	ck_assert_int_eq(code, 0);
	reconnect_client();

	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, -12000 + 460100200, 9600, 460100200, REQUEST_DESTINATION_SOCKET);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

	int length = 200;
	setup_input_data(&input, 0, length);
	setup_mock_data(input, length);
	wait_for_data_read();

	const float expected[] = { 0.000863, 0.000856, -0.000321, -0.001891, 0.000503, 0.007740, -0.002556, -0.018693, 0.006283, 0.040774, -0.030721, -0.127291, 0.027252, -0.129404, -0.004634, 0.040908, 0.002169, -0.018314, -0.000892, 0.007892, 0.000249, -0.002613, -0.000070, 0.000939, -0.000113, 0.000749, -0.000698, -0.000163, 0.000214, -0.000648, 0.000597, 0.000264, -0.000315, 0.000547, -0.000496, -0.000365, 0.000415, -0.000446, 0.000395, 0.000466 };

	float *actual = malloc(sizeof(expected));
	ck_assert(actual != NULL);
	code = read_data(actual, sizeof(expected), client0);
	ck_assert_int_eq(code, 0);
	assert_float_array(expected, sizeof(expected) / sizeof(float), actual, sizeof(expected) / sizeof(float));
	free(actual);
}
END_TEST

START_TEST (test_ping) {
	create_and_init_tcpserver();

	send_message(client0, PROTOCOL_VERSION, TYPE_PING, 0, 0, 0, 0);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);
}
END_TEST

void teardown() {
	stop_rtlsdr_mock();
	stop_tcp_server(server);
	join_tcp_server_thread(server);
	server = NULL;
	destroy_core(core_obj);
	core_obj = NULL;
	destroy_server_config(config);
	config = NULL;
	destroy_client(client0);
	client0 = NULL;
	destroy_client(client1);
	client1 = NULL;
	destroy_client(client2);
	client2 = NULL;
	if (input != NULL) {
		free(input);
		input = NULL;
	}
}

void setup() {
	init_mock_librtlsdr();
}

Suite* common_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("tcp_server");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_connect_disconnect);
	tcase_add_test(tc_core, test_invalid_request);
	tcase_add_test(tc_core, test_partial_request);
	tcase_add_test(tc_core, test_connect_and_keep_quiet);
	tcase_add_test(tc_core, test_connect_disconnect_single_client);
	tcase_add_test(tc_core, test_disconnect_client);
	tcase_add_test(tc_core, test_destination_socket);
	tcase_add_test(tc_core, test_out_of_band_frequency_clients);
	tcase_add_test(tc_core, test_ping);

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

