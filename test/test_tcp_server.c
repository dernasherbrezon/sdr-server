#include <stdlib.h>
#include <unity.h>

#include "../src/client/tcp_client.h"
#include "../src/tcp_server.h"
#include "airspy_lib_mock.h"
#include "rtlsdr_lib_mock.h"
#include "utils.h"

tcp_server *server = NULL;
struct server_config *config = NULL;
struct tcp_client *client0 = NULL;
struct tcp_client *client1 = NULL;
struct tcp_client *client2 = NULL;
uint8_t *input = NULL;
int16_t *input_cs16 = NULL;

static void reconnect_client() {
  destroy_client(client0);
  client0 = NULL;
  TEST_ASSERT_EQUAL_INT(0, create_client(config->bind_address, config->port, &client0));
}

static void create_and_init_tcpserver() {
  TEST_ASSERT_EQUAL_INT(0, create_server_config(&config, "tcp_server.config"));
  TEST_ASSERT_EQUAL_INT(0, start_tcp_server(config, &server));
  reconnect_client();
}

static void assert_response(struct tcp_client *client, uint8_t type, uint8_t status, uint8_t details) {
  struct message_header *response_header = NULL;
  struct response *resp = NULL;
  TEST_ASSERT_EQUAL_INT(0, read_response(&response_header, &resp, client));
  TEST_ASSERT_EQUAL_INT(type, response_header->type);
  TEST_ASSERT_EQUAL_INT(status, resp->status);
  TEST_ASSERT_EQUAL_INT(details, resp->details);
  free(resp);
  free(response_header);
}

void test_out_of_band_frequency_clients() {
  create_and_init_tcpserver();

  send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

  TEST_ASSERT_EQUAL_INT(0, create_client(config->bind_address, config->port, &client1));
  send_message(client1, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 461600000, REQUEST_DESTINATION_FILE);
  assert_response(client1, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_OUT_OF_BAND_FREQ);
  destroy_client(client1);
  rtlsdr_stop_mock();

  // then the first client disconnects
  send_message(client0, PROTOCOL_VERSION, TYPE_SHUTDOWN, 0, 0, 0, 0);
  gracefully_destroy_client(client0);
  client0 = NULL;

  // now band freq is available
  TEST_ASSERT_EQUAL_INT(0, create_client(config->bind_address, config->port, &client1));
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
  TEST_ASSERT_EQUAL_INT(0, write_data(buffer, sizeof(buffer), client0));
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

  TEST_ASSERT_EQUAL_INT(0, create_client(config->bind_address, config->port, &client1));
  send_message(client1, PROTOCOL_VERSION, TYPE_REQUEST, 460700000, 48000, 460600000, REQUEST_DESTINATION_FILE);
  assert_response(client1, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 1);

  send_message(client0, PROTOCOL_VERSION, TYPE_SHUTDOWN, 0, 0, 0, 0);
  // no response here is expected

  TEST_ASSERT_EQUAL_INT(0, create_client(config->bind_address, config->port, &client2));
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

void test_rtlsdr() {
  TEST_ASSERT_EQUAL_INT(0, create_server_config(&config, "tcp_server.config"));
  config->sdr_type = SDR_TYPE_RTL;
  config->band_sampling_rate = 48000;
  TEST_ASSERT_EQUAL_INT(0, start_tcp_server(config, &server));

  TEST_ASSERT_EQUAL_INT(0, create_client(config->bind_address, config->port, &client0));
  send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, -12000 + 460100200, 9600, 460100200, REQUEST_DESTINATION_SOCKET);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

  TEST_ASSERT_EQUAL_INT(0, create_client(config->bind_address, config->port, &client1));
  send_message(client1, PROTOCOL_VERSION, TYPE_REQUEST, -12000 + 460100200, 9600, 460100200, REQUEST_DESTINATION_FILE);
  assert_response(client1, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 1);

  int length = 200;
  setup_input_cu8(&input, 0, length);
  rtlsdr_setup_mock_data(input, length);
  rtlsdr_wait_for_data_read();

  const float expected[] = {-0.0000000f, -0.0000000f, -0.0005348f, -0.0006878f, 0.0023769f, 0.0014759f, -0.0050460f, -0.0028901f, 0.0100345f, 0.0064948f, -0.0245430f, -0.0142574f, 0.0051531f, -0.2082227f, 0.0151599f, 0.0243944f, -0.0072941f, -0.0100809f, 0.0034306f, 0.0046447f, -0.0012999f,
                            -0.0020388f, 0.0004290f, 0.0008074f, -0.0002939f, -0.0002891f, 0.0002456f, -0.0002504f, 0.0002068f, 0.0002021f, -0.0001587f, 0.0001633f, -0.0001197f, -0.0001152f, 0.0000717f, -0.0000762f, 0.0000326f, 0.0000283f, 0.0000152f, -0.0000109f};
  float *actual = malloc(sizeof(expected));
  TEST_ASSERT(actual != NULL);
  TEST_ASSERT_EQUAL_INT(0, read_data(actual, sizeof(expected), client0));
  assert_float_array(expected, sizeof(expected) / sizeof(float), actual, sizeof(expected) / sizeof(float));
  free(actual);

  rtlsdr_stop_mock();
  stop_tcp_server(server);
  join_tcp_server_thread(server);
  server = NULL;
  assert_file(config, 1, expected, sizeof(expected) / sizeof(float) / 2);
}

void test_airspy() {
  TEST_ASSERT_EQUAL_INT(0, create_server_config(&config, "tcp_server.config"));
  config->sdr_type = SDR_TYPE_AIRSPY;
  config->band_sampling_rate = 48000;
  config->use_gzip = true;
  TEST_ASSERT_EQUAL_INT(0, start_tcp_server(config, &server));

  TEST_ASSERT_EQUAL_INT(0, create_client(config->bind_address, config->port, &client0));
  send_message(client0, PROTOCOL_VERSION, TYPE_REQUEST, -12000 + 460100200, 9600, 460100200, REQUEST_DESTINATION_SOCKET);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);

  TEST_ASSERT_EQUAL_INT(0, create_client(config->bind_address, config->port, &client1));
  send_message(client1, PROTOCOL_VERSION, TYPE_REQUEST, -12000 + 460100200, 9600, 460100200, REQUEST_DESTINATION_FILE);
  assert_response(client1, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 1);

  int length = 200;
  setup_input_cs16(&input_cs16, 0, length);
  airspy_setup_mock_data(input_cs16, length);
  airspy_wait_for_data_read();

  const float expected[] = {-0.000000f, -0.000000f, -0.000002f, -0.000002f, 0.000007f, 0.000004f, -0.000015f, -0.000009f, 0.000031f, 0.000020f, -0.000075f, -0.000043f, 0.000013f, -0.000639f, 0.000047f, 0.000074f, -0.000022f, -0.000031f, 0.000010f, 0.000014f, -0.000004f, -0.000006f, 0.000002f, 0.000002f, -0.000001f, -0.000001f, 0.000000f, -0.000001f, 0.000000f, 0.000000f, -0.000000f, 0.000000f, 0.000000f, 0.000000f, -0.000000f, 0.000000f, -0.000000f, -0.000000f, 0.000001f, -0.000001f};
  float *actual = malloc(sizeof(expected));
  TEST_ASSERT(actual != NULL);
  TEST_ASSERT_EQUAL_INT(0, read_data(actual, sizeof(expected), client0));
  assert_float_array(expected, sizeof(expected) / sizeof(float), actual, sizeof(expected) / sizeof(float));
  free(actual);

  airspy_stop_mock();
  stop_tcp_server(server);
  join_tcp_server_thread(server);
  server = NULL;
  assert_gzfile(config, 1, expected, sizeof(expected) / sizeof(float) / 2);
}

void test_ping() {
  create_and_init_tcpserver();

  send_message(client0, PROTOCOL_VERSION, TYPE_PING, 0, 0, 0, 0);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);
}

void tearDown() {
  rtlsdr_stop_mock();
  stop_tcp_server(server);
  join_tcp_server_thread(server);
  server = NULL;
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
  if (input_cs16 != NULL) {
    free(input_cs16);
    input_cs16 = NULL;
  }
}

void setUp() {
  // do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_connect_disconnect);
  RUN_TEST(test_invalid_request);
  RUN_TEST(test_partial_request);
  RUN_TEST(test_connect_and_keep_quiet);
  RUN_TEST(test_connect_disconnect_single_client);
  RUN_TEST(test_disconnect_client);
  RUN_TEST(test_rtlsdr);
  RUN_TEST(test_airspy);
  RUN_TEST(test_out_of_band_frequency_clients);
  RUN_TEST(test_ping);
  return UNITY_END();
}
