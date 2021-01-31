#include <stdlib.h>
#include <check.h>
#include "mock_librtlsdr.c"
#include "../src/tcp_server.h"
#include "tcp_client.h"
#include "../src/api.h"

#include <stdio.h>

extern void init_mock_librtlsdr();

tcp_server *server = NULL;
core *core_obj = NULL;
struct server_config *config = NULL;
struct tcp_client *client0 = NULL;
struct tcp_client *client1 = NULL;
struct tcp_client *client2 = NULL;

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

void send_message(struct tcp_client *client, uint8_t protocol, uint8_t type, uint32_t center_freq, uint32_t sampling_rate, uint32_t band_freq) {
	struct message_header header;
	header.protocol_version = protocol;
	header.type = type;
	struct request req;
	req.band_freq = band_freq;
	req.center_freq = center_freq;
	req.sampling_rate = sampling_rate;
	int code = write_client_message(header, req, client);
	ck_assert_int_eq(code, 0);
}

void assert_response(struct tcp_client *client, uint8_t type, uint8_t status, uint8_t details) {
	struct message_header *response_header = NULL;
	struct response *resp = NULL;
	int code = read_data(&response_header, &resp, client);
	ck_assert_int_eq(code, 0);
	ck_assert_int_eq(response_header->type, type);
	ck_assert_int_eq(resp->status, status);
	ck_assert_int_eq(resp->details, details);
	free(resp);
	free(response_header);
}

START_TEST (test_invalid_request) {
	create_and_init_tcpserver();

	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 0);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 0, 460600000);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 0, 48000, 460600000);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, 0x99, TYPE_REQUEST, 460700000, 48000, 460600000);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, 0x99, 460700000, 48000, 460600000);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 47000, 460600000);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 462400000, 48000, 460600000);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

	reconnect_client();
	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 458800000, 48000, 460600000);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
}
END_TEST

START_TEST (test_connect_disconnect) {
	create_and_init_tcpserver();

	send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000);
	assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

	int code = create_client(config->bind_address, config->port, &client1);
	ck_assert_int_eq(code, 0);
	send_message(client1, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000);
	assert_response(client1, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 1);

	code = create_client(config->bind_address, config->port, &client2);
	ck_assert_int_eq(code, 0);
	send_message(client2, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000);
	assert_response(client2, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 2);

	send_message(client1, PROTOCOL_VERSION, TYPE_SHUTDOWN, 0, 0, 0);
	// no response here is expected
	send_message(client0, PROTOCOL_VERSION, TYPE_SHUTDOWN, 0, 0, 0);
	// no response here is expected
}
END_TEST

void teardown() {
	stop_tcp_server(server);
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

