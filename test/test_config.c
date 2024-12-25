#include <unity.h>
#include "../src/config.h"

struct server_config *config = NULL;

void test_missing_file() {
  int code = create_server_config(&config, "non-existing-configration-file.config");
  TEST_ASSERT_EQUAL_INT(code, -1);
}

void test_invalid_format() {
  int code = create_server_config(&config, "invalid.format.config");
  TEST_ASSERT_EQUAL_INT(code, -1);
}

void test_missing_required() {
  int code = create_server_config(&config, "invalid.config");
  TEST_ASSERT_EQUAL_INT(code, -1);
}

void test_invalid_timeout() {
  int code = create_server_config(&config, "invalid2.config");
  TEST_ASSERT_EQUAL_INT(code, -1);
}

void test_invalid_queue_size_config() {
  int code = create_server_config(&config, "invalid.queue_size.config");
  TEST_ASSERT_EQUAL_INT(code, -1);
}

void test_minimal_config() {
  int code = create_server_config(&config, "minimal.config");
  TEST_ASSERT_EQUAL_INT(code, 0);
}

void test_success() {
  int code = create_server_config(&config, "configuration.config");
  TEST_ASSERT_EQUAL_INT(code, 0);
  TEST_ASSERT_EQUAL_INT(2400000, config->band_sampling_rate);
  TEST_ASSERT_EQUAL_INT(1, config->gain_mode);
  TEST_ASSERT_EQUAL_INT(42, config->gain);
  TEST_ASSERT_EQUAL_INT(0, config->bias_t);
  TEST_ASSERT_EQUAL_INT(config->ppm, 10);
  TEST_ASSERT_EQUAL_INT(config->device_index, 1);
  TEST_ASSERT_EQUAL_STRING(config->device_serial, "00000400");
  TEST_ASSERT_EQUAL_STRING(config->bind_address, "127.0.0.1");
  TEST_ASSERT_EQUAL_INT(config->port, 8089);
  TEST_ASSERT_EQUAL_INT(config->buffer_size, 131072);
  TEST_ASSERT_EQUAL_STRING(config->base_path, "/tmp/");
  TEST_ASSERT_EQUAL_INT(config->read_timeout_seconds, 10);
  TEST_ASSERT_EQUAL_INT(config->use_gzip, 0);
  TEST_ASSERT_EQUAL_INT(config->queue_size, 64);
  TEST_ASSERT_EQUAL_INT(config->lpf_cutoff_rate, 5);
}

void tearDown() {
  destroy_server_config(config);
  config = NULL;
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_success);
  RUN_TEST(test_missing_required);
  RUN_TEST(test_missing_file);
  RUN_TEST(test_invalid_format);
  RUN_TEST(test_minimal_config);
  RUN_TEST(test_invalid_timeout);
  RUN_TEST(test_invalid_queue_size_config);
  return UNITY_END();
}
