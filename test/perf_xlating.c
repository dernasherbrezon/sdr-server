#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../src/lpf.h"
#include "../src/xlating.h"

#ifndef CMAKE_C_FLAGS
#define CMAKE_C_FLAGS ""
#endif

extern const char *SIMD_STATUS;

int main(void) {
  printf("SIMD optimization: %s\n", SIMD_STATUS);
  printf("compilation flags: %s\n", CMAKE_C_FLAGS);
  uint32_t sampling_freq = 2016000;
  uint32_t target_freq = 48000;
  float *taps = NULL;
  size_t len;
  int code = create_low_pass_filter(1.0f, sampling_freq, target_freq / 2, 2000, &taps, &len);
  if (code != 0) {
    exit(EXIT_FAILURE);
  }
  size_t max_input = 200000;
  xlating *filter = NULL;
  code = create_frequency_xlating_filter((int)(sampling_freq / target_freq), taps, len, -12000, sampling_freq, max_input, &filter);
  if (code != 0) {
    exit(EXIT_FAILURE);
  }
  uint8_t *input = NULL;
  input = malloc(sizeof(float) * max_input);
  if (input == NULL) {
    exit(EXIT_FAILURE);
  }
  for (size_t i = 0; i < max_input; i++) {
    // don't care about the loss of data
    input[i] = ((float)(i)) / 128.0f;
  }

  int total_executions = 1000;

  clock_t begin = clock();
  for (int i = 0; i < total_executions; i++) {
    float complex *output;
    size_t output_len = 0;
    process_native_cu8_cf32(input, max_input, &output, &output_len, filter);
  }
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  printf("native         cu8_cf32: %f seconds\n", time_spent / total_executions);
  begin = clock();
  for (int i = 0; i < total_executions; i++) {
    float complex *output;
    size_t output_len = 0;
    process_optimized_cu8_cf32(input, max_input, &output, &output_len, filter);
  }
  end = clock();
  time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  printf("optimized      cu8_cf32: %f seconds\n", time_spent / total_executions);

  begin = clock();
  for (int i = 0; i < total_executions; i++) {
    int16_t *output;
    size_t output_len = 0;
    process_native_cu8_cs16(input, max_input, &output, &output_len, filter);
  }
  end = clock();
  time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  printf("native         cu8_cs16: %f seconds\n", time_spent / total_executions);

  begin = clock();
  for (int i = 0; i < total_executions; i++) {
    int16_t *output;
    size_t output_len = 0;
    process_optimized_cu8_cs16(input, max_input, &output, &output_len, filter);
  }
  end = clock();
  time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  printf("optimized      cu8_cs16: %f seconds\n", time_spent / total_executions);

  // MacBook Air
  // volk_generic   cu8_cf32: 0.005615 seconds
  // volk optimized cu8_cf32: 0.002649 seconds
  // optimized      cu8_cf32: 0.002038 seconds
  // native         cu8_cf32: 0.002902 seconds

  // MacBook Air M1
  // volk_generic   cu8_cf32: 0.001693 seconds
  // volk optimized cu8_cf32: 0.001477 seconds
  // native         cu8_cf32: 0.001440 seconds
  // optimized      cu8_cf32: 0.003617 seconds
  // native         cu8_cs16: 0.001161 seconds
  // optimized      cu8_cs16: 0.001161 seconds

  // Raspberrypi 3
  // volk_generic   cu8_cf32: 0.073828 seconds
  // volk optimized cu8_cf32: 0.024855 seconds

  // Raspberrypi 4
  // volk_generic   cu8_cf32: 0.041116 seconds
  // volk optimized cu8_cf32: 0.013621 seconds
  // native         cu8_cf32: 0.039529 seconds
  // optimized      cu8_cf32: 0.011978 seconds

  // Raspberrypi 1
  // volk_generic   cu8_cf32: 0.291598 seconds
  // volk optimized cu8_cf32: 0.332934 seconds

  // Intel(R) Core(TM) i5-7500 CPU @ 3.40GHz
  // volk_generic   cu8_cf32: 0.003249 seconds
  // volk optimized cu8_cf32: 0.001609 seconds
  // native         cu8_cf32: 0.001603 seconds
  // optimized      cu8_cf32: 0.001605 seconds
  return 0;
}
