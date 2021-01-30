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
struct tcp_client *client = NULL;

void create_and_init_tcpserver() {
	int code = create_server_config(&config, "tcp_server.config");
	ck_assert_int_eq(code, 0);
	code = create_core(config, &core_obj);
	ck_assert_int_eq(code, 0);
	code = start_tcp_server(config, core_obj, &server);
	ck_assert_int_eq(code, 0);
	code = create_client(config->bind_address, config->port, &client);
	ck_assert_int_eq(code, 0);
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
	int code = write_data(buffer, sizeof(buffer), client);
	ck_assert_int_eq(code, 0);
}
END_TEST

START_TEST (test_invalid_request) {
	create_and_init_tcpserver();

	struct message_header header;
	header.protocol_version = PROTOCOL_VERSION;
	header.type = TYPE_REQUEST;
	struct request req;
	req.band_freq = 0;
	req.center_freq = 460700000;
	req.sampling_rate = 48000;
	int code = write_client_message(header, req, client);
	ck_assert_int_eq(code, 0);

	struct message_header *response_header = NULL;
	struct response *resp = NULL;
	code = read_data(&response_header, &resp, client);
	ck_assert_int_eq(code, 0);
	ck_assert_int_eq(response_header->type, TYPE_RESPONSE);
	ck_assert_int_eq(resp->status, RESPONSE_STATUS_FAILURE);
	ck_assert_int_eq(resp->details, RESPONSE_DETAILS_INVALID_REQUEST);
	free(resp);
	free(response_header);
}
END_TEST

START_TEST (test_connect_disconnect) {
	create_and_init_tcpserver();

	struct message_header header;
	header.protocol_version = PROTOCOL_VERSION;
	header.type = TYPE_REQUEST;
	struct request req;
	req.band_freq = 460600000;
	req.center_freq = 460700000;
	req.sampling_rate = 48000;
	int code = write_client_message(header, req, client);
	ck_assert_int_eq(code, 0);

	struct message_header *response_header = NULL;
	struct response *resp = NULL;
	code = read_data(&response_header, &resp, client);
	ck_assert_int_eq(code, 0);
	ck_assert_int_eq(response_header->type, TYPE_RESPONSE);
	ck_assert_int_eq(resp->status, RESPONSE_STATUS_SUCCESS);
	ck_assert_int_eq(resp->details, RESPONSE_DETAILS_SUCCESS);
	free(resp);
	free(response_header);

	header.type = TYPE_SHUTDOWN;
	code = write_client_message(header, req, client);
	ck_assert_int_eq(code, 0);
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
	destroy_client(client);
	client = NULL;
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

