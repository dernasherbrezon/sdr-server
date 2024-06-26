#include <stdlib.h>
#include <unity.h>

#include "utils.h"
#include "mock_librtlsdr.c"
#include "../src/tcp_server.h"
#include "../src/client/tcp_client.h"

extern void init_mock_librtlsdr();

extern void wait_for_data_read();

extern void setup_mock_data(uint8_t *buffer, int len);

extern void stop_rtlsdr_mock();

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
  TEST_ASSERT_EQUAL_INT(code, 0);
}

void create_and_init_tcpserver() {
  int code = create_server_config(&config, "tcp_server.config");
  TEST_ASSERT_EQUAL_INT(code, 0);
  code = create_core(config, &core_obj);
  TEST_ASSERT_EQUAL_INT(code, 0);
  code = start_tcp_server(config, core_obj, &server);
  TEST_ASSERT_EQUAL_INT(code, 0);
  reconnect_client();
}

void assert_response(struct tcp_client *client, uint8_t type, uint8_t status, uint8_t details) {
  struct message_header *response_header = NULL;
  struct response *resp = NULL;
  int code = read_response(&response_header, &resp, client);
  TEST_ASSERT_EQUAL_INT(code, 0);
  TEST_ASSERT_EQUAL_INT(response_header->type, type);
  TEST_ASSERT_EQUAL_INT(resp->status, status);
  TEST_ASSERT_EQUAL_INT(resp->details, details);
  free(resp);
  free(response_header);
}

void test_out_of_band_frequency_clients() {
  create_and_init_tcpserver();

  send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

  int code = create_client(config->bind_address, config->port, &client1);
  TEST_ASSERT_EQUAL_INT(code, 0);
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
  TEST_ASSERT_EQUAL_INT(code, 0);
  send_message(client1, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 461600000, REQUEST_DESTINATION_FILE);
  assert_response(client1, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 2);
}

// this test will ensure tcp server stops
// if client connected and didn't send anything
void test_connect_and_keep_quiet() {
  create_and_init_tcpserver();
}

void test_partial_request() {
  create_and_init_tcpserver();
  uint8_t buffer[] = {PROTOCOL_VERSION};
  int code = write_data(buffer, sizeof(buffer), client0);
  TEST_ASSERT_EQUAL_INT(code, 0);
}

void test_invalid_request() {
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

void test_connect_disconnect() {
  create_and_init_tcpserver();

  send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

  int code = create_client(config->bind_address, config->port, &client1);
  TEST_ASSERT_EQUAL_INT(code, 0);
  send_message(client1, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
  assert_response(client1, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 1);

  send_message(client0, PROTOCOL_VERSION, TYPE_SHUTDOWN, 0, 0, 0, 0);
  // no response here is expected

  code = create_client(config->bind_address, config->port, &client2);
  TEST_ASSERT_EQUAL_INT(code, 0);
  send_message(client2, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
  assert_response(client2, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 2);

  send_message(client1, PROTOCOL_VERSION, TYPE_SHUTDOWN, 0, 0, 0, 0);
  // no response here is expected
}

void test_connect_disconnect_single_client() {
  create_and_init_tcpserver();

  send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);
}

void test_disconnect_client() {
  create_and_init_tcpserver();

  send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

  destroy_client(client0);
  client0 = NULL;
}

void test_destination_socket() {
  // make input/output compatible with test_core
  int code = create_server_config(&config, "tcp_server.config");
  TEST_ASSERT_EQUAL_INT(code, 0);
  config->band_sampling_rate = 48000;
  code = create_core(config, &core_obj);
  TEST_ASSERT_EQUAL_INT(code, 0);
  code = start_tcp_server(config, core_obj, &server);
  TEST_ASSERT_EQUAL_INT(code, 0);
  reconnect_client();

  send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, -12000 + 460100200, 9600, 460100200, REQUEST_DESTINATION_SOCKET);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

  int length = 200;
  setup_input_data(&input, 0, length);
  setup_mock_data(input, length);
  wait_for_data_read();

  const float expected[] = {0.0000000f, 0.0000000f, -0.0000305f, 0.0000397f, -0.0000284f, -0.0001087f, 0.0000315f, 0.0000721f, 0.0001696f, -0.0001918f, -0.0002347f, 0.0007635f, -0.0027987f, -0.0011748f, 0.0006935f, -0.0004690f, -0.0000522f, 0.0003383f, -0.0000859f, 0.0001224f, -0.0002705f,
                            -0.0001877f, 0.0002557f, -0.0002522f, 0.0002613f, 0.0002652f, 0.0007585f, 0.0013720f, -0.0015104f, -0.0039876f, 0.0041792f, 0.0114027f, -0.0093692f, -0.0255084f, 0.0198582f, 0.0540531f, -0.0603861f, -0.1393571f, 0.0422274f, -0.3586643f};

  float *actual = malloc(sizeof(expected));
  TEST_ASSERT(actual != NULL);
  code = read_data(actual, sizeof(expected), client0);
  TEST_ASSERT_EQUAL_INT(code, 0);
  assert_float_array(expected, sizeof(expected) / sizeof(float), actual, sizeof(expected) / sizeof(float));
  free(actual);
}

void test_ping() {
  create_and_init_tcpserver();

  send_message(client0, PROTOCOL_VERSION, TYPE_PING, 0, 0, 0, 0);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);
}

void tearDown() {
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

void setUp() {
  init_mock_librtlsdr();
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_connect_disconnect);
  RUN_TEST(test_invalid_request);
  RUN_TEST(test_partial_request);
  RUN_TEST(test_connect_and_keep_quiet);
  RUN_TEST(test_connect_disconnect_single_client);
  RUN_TEST(test_disconnect_client);
  RUN_TEST(test_destination_socket);
  RUN_TEST(test_out_of_band_frequency_clients);
  RUN_TEST(test_ping);
  return UNITY_END();
}

