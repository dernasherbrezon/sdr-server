#include <stdlib.h>
#include <unity.h>
#include <string.h>

#include "utils.h"
#include <spectrogram.h>

char tmp_template[] = "/tmp/sdr_spectrogram_test_XXXXXX";
char *test_dir = NULL;
char input_file[1024];
char output_file[1024];

void test_success() {
  spectrogram spec;
  spec.sampling_rate = 128;
  spec.width = 64;
  spec.data_format = "cu8";
  spec.input_file = input_file;
  spec.output_file = output_file;
  spec.fftw_flags = "FFTW_MEASURE";

  setup_file_cu8(input_file, 2 * 256);
  TEST_ASSERT_EQUAL_INT(0, spectrogram_main(&spec));
  assert_png("spectrogram_cu8.png", output_file);
}

void tearDown() {
  // ignore all status codes
  remove(input_file);
  remove(output_file);
  rmdir(test_dir);
}

void setUp() {
  test_dir = mkdtemp(tmp_template);
  strcat(input_file, test_dir);
  strcat(input_file, "/input.raw");
  strcat(output_file, test_dir);
  strcat(output_file, "/spectrogram.png");
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_success);
  return UNITY_END();
}
