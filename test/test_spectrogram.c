#include <stdlib.h>
#include <unity.h>
#include <string.h>
#include <iq_file.h>
#include "utils.h"
#include <spectrogram.h>

static char test_dir[PATH_MAX];
char input_file[PATH_MAX];
char output_file[PATH_MAX];

spectrogram spec;

void test_plain_file() {
  setup_file(input_file, 256, CU8_FORMAT);
  TEST_ASSERT_EQUAL_INT(0, spectrogram_main(&spec));
  assert_png("spectrogram_cu8.png", output_file);

  setup_file(input_file, 256, CS16_FORMAT);
  spec.data_format = "cs16";
  TEST_ASSERT_EQUAL_INT(0, spectrogram_main(&spec));
  assert_png("spectrogram_cs16.png", output_file);

  setup_file(input_file, 256, CF32_FORMAT);
  spec.data_format = "cf32";
  TEST_ASSERT_EQUAL_INT(0, spectrogram_main(&spec));
  assert_png("spectrogram_cf32.png", output_file);

  // test odd with skip on every row
  spec.width = 63;
  TEST_ASSERT_EQUAL_INT(0, spectrogram_main(&spec));
  assert_png("spectrogram_cf32_odd.png", output_file);

  // unsupported flags just logged and fallback
  spec.fftw_flags = "unsupported";
  TEST_ASSERT_EQUAL_INT(0, spectrogram_main(&spec));
  assert_png("spectrogram_cf32_odd.png", output_file);
}

void test_gzipped_file() {
  strcat(input_file, ".gz");
  setup_gzfile(input_file, 256, CU8_FORMAT);
  spec.data_format = "cu8";
  TEST_ASSERT_EQUAL_INT(0, spectrogram_main(&spec));
  assert_png("spectrogram_cu8.png", output_file);

  setup_gzfile(input_file, 256, CS16_FORMAT);
  spec.data_format = "cs16";
  TEST_ASSERT_EQUAL_INT(0, spectrogram_main(&spec));
  assert_png("spectrogram_cs16.png", output_file);

  setup_gzfile(input_file, 256, CF32_FORMAT);
  spec.data_format = "cf32";
  TEST_ASSERT_EQUAL_INT(0, spectrogram_main(&spec));
  assert_png("spectrogram_cf32.png", output_file);

  // test odd with skip on every row
  spec.width = 63;
  TEST_ASSERT_EQUAL_INT(0, spectrogram_main(&spec));
  assert_png("spectrogram_cf32_odd.png", output_file);
}

void test_invalid_arguments() {
  spec.input_file = NULL;
  TEST_ASSERT_FALSE(0 == spectrogram_main(&spec));
  spec.input_file = input_file;

  spec.output_file = NULL;
  TEST_ASSERT_FALSE(0 == spectrogram_main(&spec));
  spec.output_file = output_file;

  spec.width = 0;
  TEST_ASSERT_FALSE(0 == spectrogram_main(&spec));
  spec.width = 64;

  spec.sampling_rate = 0;
  TEST_ASSERT_FALSE(0 == spectrogram_main(&spec));
  spec.sampling_rate = 128;

  spec.width = (int) (spec.sampling_rate + 1);
  TEST_ASSERT_FALSE(0 == spectrogram_main(&spec));
  spec.width = 64;

  spec.input_file = "/tmp/non-existing-file";
  TEST_ASSERT_FALSE(0 == spectrogram_main(&spec));
  spec.input_file = input_file;

  spec.data_format = "unsupported";
  TEST_ASSERT_FALSE(0 == spectrogram_main(&spec));
  spec.data_format = "cu8";
}

void tearDown() {
  // ignore all status codes
  remove(input_file);
  remove(output_file);
  if (rmdir(test_dir) != 0) {
    fprintf(stderr, "can't remove directory: %s\n", test_dir);
  }
}

void setUp() {
  memset(input_file, 0, sizeof(input_file));
  memset(output_file, 0, sizeof(output_file));
  strcpy(test_dir, "/tmp/sdr_spectrogram_test_XXXXXX");
  mkdtemp(test_dir);
  strcat(input_file, test_dir);
  strcat(input_file, "/input.raw");
  strcat(output_file, test_dir);
  strcat(output_file, "/spectrogram.png");
  spec.sampling_rate = 128;
  spec.width = 64;
  spec.data_format = "cu8";
  spec.input_file = input_file;
  spec.output_file = output_file;
  spec.fftw_flags = "FFTW_MEASURE";
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_plain_file);
  RUN_TEST(test_gzipped_file);
  RUN_TEST(test_invalid_arguments);
  return UNITY_END();
}
