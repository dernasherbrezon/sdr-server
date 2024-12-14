#include <stdlib.h>
#include <unity.h>
#include <stdbool.h>
#include "mock_librtlsdr.c"
#include <stdio.h>
#include <complex.h>
#include <zlib.h>
#include "utils.h"

extern void init_mock_librtlsdr();

extern void wait_for_data_read();

extern void setup_mock_data(uint8_t *buffer, int len);

core *core_obj = NULL;
struct server_config *config = NULL;
struct client_config *client_config0 = NULL;
struct client_config *client_config1 = NULL;
uint8_t *input = NULL;

void create_client_config(int id, struct client_config **client_config) {
  struct client_config *result = malloc(sizeof(struct client_config));
  TEST_ASSERT(result != NULL);
  result->core = core_obj;
  result->id = id;
  result->is_running = true;
  result->sampling_rate = 9600;
  result->band_freq = 460100200;
  result->center_freq = -12000 + result->band_freq;
  result->client_socket = 0;
  result->destination = REQUEST_DESTINATION_FILE;
  *client_config = result;
}

void assert_file(int id, const float expected[], size_t expected_size) {
  char file_path[4096];
  snprintf(file_path, sizeof(file_path), "%s/%d.cf32", config->base_path, id);
  fprintf(stdout, "checking: %s\n", file_path);
  FILE *f = fopen(file_path, "rb");
  TEST_ASSERT(f != NULL);
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  TEST_ASSERT(fsize > 0);
  fseek(f, 0, SEEK_SET);
  uint8_t *buffer = malloc(fsize);
  TEST_ASSERT(buffer != NULL);
  fread(buffer, 1, fsize, f);
  fclose(f);
  size_t actual_size = fsize / sizeof(float complex);
  assert_complex(expected, expected_size, (float complex *) buffer, actual_size);
  free(buffer);
}

int read_gzfile_fully(gzFile f, void *result, size_t len) {
  size_t left = len;
  while (left > 0) {
    int received = gzread(f, (char *) result + (len - left), left);
    if (received <= 0) {
      perror("unable to read the message");
      return -1;
    }
    left -= received;
  }
  return 0;
}

void assert_gzfile(int id, const float expected[], size_t expected_size) {
  char file_path[4096];
  snprintf(file_path, sizeof(file_path), "%s/%d.cf32.gz", config->base_path, id);
  fprintf(stdout, "checking: %s\n", file_path);
  gzFile f = gzopen(file_path, "rb");
  TEST_ASSERT(f != NULL);
  size_t expected_size_bytes = sizeof(float complex) * expected_size;
  uint8_t *buffer = malloc(expected_size_bytes);
  TEST_ASSERT(buffer != NULL);
  int code = read_gzfile_fully(f, buffer, expected_size_bytes);
  TEST_ASSERT_EQUAL_INT(code, 0);
  gzclose(f);
  size_t actual_size = expected_size;
  assert_complex(expected, expected_size, (float complex *) buffer, actual_size);
  free(buffer);
}

void test_gzoutput() {
  int code = create_server_config(&config, "core.config");
  TEST_ASSERT_EQUAL_INT(code, 0);
  config->use_gzip = true;
  code = create_core(config, &core_obj);
  TEST_ASSERT_EQUAL_INT(code, 0);

  create_client_config(0, &client_config0);
  code = add_client(client_config0);
  TEST_ASSERT_EQUAL_INT(code, 0);

  int length = 200;
  setup_input_data(&input, 0, length);
  setup_mock_data(input, length);
  wait_for_data_read();

  // flush data to files and close them
  destroy_core(core_obj);
  core_obj = NULL;

  const float expected[] = {-0.0001035f, -0.0001027f, -0.0000105f, 0.0001232f, -0.0000066f, -0.0002575f, 0.0001184f, 0.0002886f, -0.0001268f, -0.0002544f, 0.0000687f, 0.0003351f, -0.0001032f, -0.0004956f, 0.0001945f, 0.0005987f, -0.0002240f, -0.0006826f, 0.0002285f, 0.0008483f, -0.0002878f,
                            -0.0010620f, 0.0003668f, 0.0012673f, -0.0004268f, -0.0015031f, 0.0005049f, 0.0017931f, -0.0006021f, -0.0021068f, 0.0006850f, 0.0024705f, -0.0007959f, -0.0029283f, 0.0009737f, 0.0034315f, -0.0011436f, -0.0039606f, 0.0012771f, 0.0046368f};
  size_t expected_length = 20;
  assert_gzfile(0, expected, expected_length);
}

void test_invalid_lpf_config() {
  int code = create_server_config(&config, "core.config");
  TEST_ASSERT_EQUAL_INT(code, 0);
  code = create_core(config, &core_obj);
  TEST_ASSERT_EQUAL_INT(code, 0);

  create_client_config(0, &client_config0);
  client_config0->sampling_rate = 49000;
  code = add_client(client_config0);
  TEST_ASSERT_EQUAL_INT(code, -1);
}

void test_process_message() {
  int code = create_server_config(&config, "core.config");
  TEST_ASSERT_EQUAL_INT(code, 0);
  code = create_core(config, &core_obj);
  TEST_ASSERT_EQUAL_INT(code, 0);

  create_client_config(0, &client_config0);
  code = add_client(client_config0);
  TEST_ASSERT_EQUAL_INT(code, 0);

  create_client_config(1, &client_config1);
  code = add_client(client_config1);
  TEST_ASSERT_EQUAL_INT(code, 0);

  int length = 200;
  setup_input_data(&input, 0, length);
  setup_mock_data(input, length);
  wait_for_data_read();

  // just to increase code coverage
  remove_client(client_config1);

  // flush data to files and close them
  destroy_core(core_obj);
  core_obj = NULL;

  const float expected[] = {-0.0001035f, -0.0001027f, -0.0000105f, 0.0001232f, -0.0000066f, -0.0002575f, 0.0001184f, 0.0002886f, -0.0001268f, -0.0002544f, 0.0000687f, 0.0003351f, -0.0001032f, -0.0004956f, 0.0001945f, 0.0005987f, -0.0002240f, -0.0006826f, 0.0002285f, 0.0008483f, -0.0002878f,
                            -0.0010620f, 0.0003668f, 0.0012673f, -0.0004268f, -0.0015031f, 0.0005049f, 0.0017931f, -0.0006021f, -0.0021068f, 0.0006850f, 0.0024705f, -0.0007959f, -0.0029283f, 0.0009737f, 0.0034315f, -0.0011436f, -0.0039606f, 0.0012771f, 0.0046368f};
  size_t expected_length = 20;
  assert_file(0, expected, expected_length);
  assert_file(1, expected, expected_length);
}

void test_invalid_config() {
  int code = create_server_config(&config, "invalid.basepath.config");
  TEST_ASSERT_EQUAL_INT(code, 0);
  code = create_core(config, &core_obj);
  TEST_ASSERT_EQUAL_INT(code, 0);

  code = add_client(NULL);
  TEST_ASSERT_EQUAL_INT(code, -1);

  create_client_config(0, &client_config0);
  config->use_gzip = true;
  code = add_client(client_config0);
  TEST_ASSERT_EQUAL_INT(code, -1);

  // check both types handled properly
  config->use_gzip = false;
  code = add_client(client_config0);
  TEST_ASSERT_EQUAL_INT(code, -1);
}

void tearDown() {
  destroy_core(core_obj);
  core_obj = NULL;
  destroy_server_config(config);
  config = NULL;
  if (client_config0 != NULL) {
    free(client_config0);
    client_config0 = NULL;
  }
  if (client_config1 != NULL) {
    free(client_config1);
    client_config1 = NULL;
  }
  if (input != NULL) {
    free(input);
    input = NULL;
  }
}

void setUp() {
  init_mock_librtlsdr();
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_process_message);
  RUN_TEST(test_invalid_lpf_config);
  RUN_TEST(test_gzoutput);
  RUN_TEST(test_invalid_config);
  return UNITY_END();
}
