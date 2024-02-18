#include <stdlib.h>
#include <check.h>
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
  ck_assert(result != NULL);
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
  ck_assert(f != NULL);
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  ck_assert_int_gt(fsize, 0);
  fseek(f, 0, SEEK_SET);
  uint8_t *buffer = malloc(fsize);
  ck_assert(buffer != NULL);
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
  ck_assert(f != NULL);
  size_t expected_size_bytes = sizeof(float complex) * expected_size;
  uint8_t *buffer = malloc(expected_size_bytes);
  ck_assert(buffer != NULL);
  int code = read_gzfile_fully(f, buffer, expected_size_bytes);
  ck_assert_int_eq(code, 0);
  gzclose(f);
  size_t actual_size = expected_size;
  assert_complex(expected, expected_size, (float complex *) buffer, actual_size);
  free(buffer);
}

START_TEST (test_gzoutput) {
  int code = create_server_config(&config, "core.config");
  ck_assert_int_eq(code, 0);
  config->use_gzip = true;
  code = create_core(config, &core_obj);
  ck_assert_int_eq(code, 0);

  create_client_config(0, &client_config0);
  code = add_client(client_config0);
  ck_assert_int_eq(code, 0);

  int length = 200;
  setup_input_data(&input, 0, length);
  setup_mock_data(input, length);
  wait_for_data_read();

  // flush data to files and close them
  destroy_core(core_obj);
  core_obj = NULL;

  const float expected[] = {0.0000000f, 0.0000008f, 0.0000077f, 0.0000069f, -0.0000159f, 0.0000107f, -0.0000109f, -0.0000187f, 0.0000177f, -0.0000184f, 0.0000307f, 0.0000257f, -0.0000398f, 0.0000320f, -0.0000255f, -0.0000400f, 0.0000308f, -0.0000364f, 0.0000587f, 0.0000432f, -0.0000691f, 0.0000545f,
                            -0.0000343f, -0.0000628f, 0.0000376f, -0.0000529f, -0.0000375f, -0.0004878f, 0.0000285f, 0.0004105f, 0.0000801f, -0.0004931f, -0.0000730f, 0.0007020f, -0.0000130f, -0.0008453f, 0.0000021f, 0.0009631f, 0.0000697f, -0.0012152f};
  size_t expected_length = 20;
  assert_gzfile(0, expected, expected_length);
}

END_TEST

START_TEST (test_invalid_lpf_config) {
  int code = create_server_config(&config, "core.config");
  ck_assert_int_eq(code, 0);
  code = create_core(config, &core_obj);
  ck_assert_int_eq(code, 0);

  create_client_config(0, &client_config0);
  client_config0->sampling_rate = 49000;
  code = add_client(client_config0);
  ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_process_message) {
  int code = create_server_config(&config, "core.config");
  ck_assert_int_eq(code, 0);
  code = create_core(config, &core_obj);
  ck_assert_int_eq(code, 0);

  create_client_config(0, &client_config0);
  code = add_client(client_config0);
  ck_assert_int_eq(code, 0);

  create_client_config(1, &client_config1);
  code = add_client(client_config1);
  ck_assert_int_eq(code, 0);

  int length = 200;
  setup_input_data(&input, 0, length);
  setup_mock_data(input, length);
  wait_for_data_read();

  // just to increase code coverage
  remove_client(client_config1);

  // flush data to files and close them
  destroy_core(core_obj);
  core_obj = NULL;

  const float expected[] = {0.0000000f, 0.0000008f, 0.0000077f, 0.0000069f, -0.0000159f, 0.0000107f, -0.0000109f, -0.0000187f, 0.0000177f, -0.0000184f, 0.0000307f, 0.0000257f, -0.0000398f, 0.0000320f, -0.0000255f, -0.0000400f, 0.0000308f, -0.0000364f, 0.0000587f, 0.0000432f, -0.0000691f, 0.0000545f,
                            -0.0000343f, -0.0000628f, 0.0000376f, -0.0000529f, -0.0000375f, -0.0004878f, 0.0000285f, 0.0004105f, 0.0000801f, -0.0004931f, -0.0000730f, 0.0007020f, -0.0000130f, -0.0008453f, 0.0000021f, 0.0009631f, 0.0000697f, -0.0012152f};
  size_t expected_length = 20;
  assert_file(0, expected, expected_length);
  assert_file(1, expected, expected_length);
}

END_TEST

START_TEST (test_invalid_config) {
  int code = create_server_config(&config, "invalid.basepath.config");
  ck_assert_int_eq(code, 0);
  code = create_core(config, &core_obj);
  ck_assert_int_eq(code, 0);

  code = add_client(NULL);
  ck_assert_int_eq(code, -1);

  create_client_config(0, &client_config0);
  config->use_gzip = true;
  code = add_client(client_config0);
  ck_assert_int_eq(code, -1);

  // check both types handled properly
  config->use_gzip = false;
  code = add_client(client_config0);
  ck_assert_int_eq(code, -1);

}

END_TEST

void teardown() {
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

void setup() {
  init_mock_librtlsdr();
  //do nothing
}

Suite *common_suite(void) {
  Suite *s;
  TCase *tc_core;

  s = suite_create("core");

  /* Core test case */
  tc_core = tcase_create("Core");

  tcase_add_test(tc_core, test_process_message);
  tcase_add_test(tc_core, test_invalid_lpf_config);
  tcase_add_test(tc_core, test_gzoutput);
  tcase_add_test(tc_core, test_invalid_config);

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

