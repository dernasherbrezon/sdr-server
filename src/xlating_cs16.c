#include "xlating_cs16.h"

#include <complex.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <stdfix.h>

struct xlating_cs16_t {
  uint32_t decimation;
  int16_t **taps;
  size_t aligned_taps_len;
  size_t alignment;
  size_t taps_len;
  float *original_taps;

  int16_t *working_buffer;
  size_t history_offset;
  size_t working_len_total;

  int16_t *output;
  size_t output_len;

  int16_t phase_real;
  int16_t phase_imag;
  int16_t phase_incr_real;
  int16_t phase_incr_imag;
};

static void *sdrserver_aligned_alloc(size_t alignment, size_t size) {
  if (size % alignment != 0) {
    size = ((size / alignment) + 1) * alignment;
  }
  return aligned_alloc(alignment, size);
}

static inline int16_t saturate_to_int16(int32_t value) {
  // Saturate using bitwise operations
  int32_t result = (value > INT16_MAX) ? INT16_MAX : value;
  result = (result < INT16_MIN) ? INT16_MIN : result;
  return (int16_t)result;
}

static void process_cs16(size_t input_complex_len, int16_t **output, size_t *output_len, xlating_cs16 *filter) {
  size_t working_len = filter->history_offset + input_complex_len;
  size_t produced = 0;
  size_t current_index = 0;

  // input might not have enough data to produce output sample
  if (working_len > (filter->taps_len - 1)) {
    size_t max_index = working_len - (filter->taps_len - 1);
    for (; current_index < max_index; current_index += filter->decimation, produced += 2) {
      const int16_t *buf = (const int16_t *)(filter->working_buffer + current_index);

      const int16_t *aligned_buffer = (const int16_t *)((size_t)buf & ~(filter->alignment - 1));
      unsigned align_index = buf - aligned_buffer;

      int64_t temp_real = 0;
      int64_t temp_imag = 0;
      for (size_t i = 0; i < filter->taps_len + align_index; i += 2) {
        int16_t ar = aligned_buffer[i];
        int16_t ai = aligned_buffer[i + 1];
        int16_t br = filter->taps[align_index][i];
        int16_t bi = filter->taps[align_index][i + 1];

        temp_real += (int32_t)ar * br - (int32_t)ai * bi;
        temp_imag += (int32_t)ar * bi + (int32_t)ai * br;
      }

      int16_t acc_real = saturate_to_int16(temp_real >> 15);
      int16_t acc_imag = saturate_to_int16(temp_imag >> 15);

      temp_real = acc_real * filter->phase_real - acc_imag * filter->phase_imag;
      temp_imag = acc_real * filter->phase_imag + acc_imag * filter->phase_real;
      filter->output[produced] = saturate_to_int16(temp_real >> 15);
      filter->output[produced + 1] = saturate_to_int16(temp_imag >> 15);

      temp_real = filter->phase_real * filter->phase_incr_real - filter->phase_imag * filter->phase_incr_imag;
      temp_imag = filter->phase_real * filter->phase_incr_imag + filter->phase_imag * filter->phase_incr_real;
      filter->phase_real = saturate_to_int16(temp_real >> 15);
      filter->phase_imag = saturate_to_int16(temp_imag >> 15);
    }
  }
  // preserve history for the next execution
  filter->history_offset = working_len - current_index;
  if (current_index > 0) {
    memmove(filter->working_buffer, filter->working_buffer + current_index, sizeof(int16_t) * filter->history_offset);
  }

  *output = filter->output;
  *output_len = produced;
}

static int create_aligned_taps(xlating_cs16 *filter, int16_t *bpfTaps, size_t taps_len) {
  size_t number_of_aligned = fmax((size_t)1, filter->alignment / sizeof(int16_t));
  // Make a set of taps at all possible alignments
  int16_t **result = malloc(number_of_aligned * sizeof(int16_t *));
  if (result == NULL) {
    return -1;
  }
  for (int i = 0; i < number_of_aligned; i++) {
    size_t aligned_taps_len = taps_len + number_of_aligned - 1;
    result[i] = (int16_t *)sdrserver_aligned_alloc(filter->alignment, 2 * aligned_taps_len * sizeof(int16_t));
    // some taps will be longer than original, but
    // since they contain zeros, multiplication on an input will produce 0
    // there is a tradeoff: multiply unaligned input or
    // multiply aligned input but with additional zeros
    for (size_t j = 0; j < 2 * aligned_taps_len; j++) {
      result[i][j] = 0;
    }
    for (size_t j = 0; j < 2 * taps_len; j++) {
      result[i][i + j] = bpfTaps[j];
    }
  }
  filter->taps = result;
  filter->aligned_taps_len = number_of_aligned;
  return 0;
}

int create_frequency_xlating_cs16_filter(uint32_t decimation, float *taps, size_t taps_len, int32_t center_freq, uint32_t sampling_freq, uint32_t max_input_buffer_length, xlating_cs16 **filter) {
  if (taps_len == 0) {
    return -1;
  }
  struct xlating_cs16_t *result = malloc(sizeof(struct xlating_cs16_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0
  *result = (struct xlating_cs16_t){0};
  result->decimation = decimation;
  result->taps_len = taps_len;
  // make code consistent - i.e. destroy all incoming memory in destory_xxx methods
  result->original_taps = taps;
  result->alignment = 32;

  // The basic principle of this block is to perform:
  //    x(t) -> (mult by -fwT0) -> LPF -> decim -> y(t)
  // We switch things up here to:
  //    x(t) -> BPF -> decim -> (mult by fwT0*decim) -> y(t)
  // The BPF is the baseband filter (LPF) moved up to the
  // center frequency fwT0. We then apply a derotator
  // with -fwT0 to downshift the signal to baseband.
  int16_t *bpfTaps = malloc(sizeof(int16_t) * taps_len * 2);
  if (bpfTaps == NULL) {
    destroy_xlating_cs16(result);
    return -ENOMEM;
  }
  float fwT0 = 2 * M_PI * center_freq / sampling_freq;
  for (size_t i = 0; i < taps_len; i++) {
    float complex cur = 0 + i * fwT0 * I;
    cur = taps[i] * cexpf(cur);
    bpfTaps[2 * i] = crealf(cur) * (1 << 15);
    bpfTaps[2 * i + 1] = cimagf(cur) * (1 << 15);
  }
  // reverse taps to allow storing history in array
  for (size_t i = 0; i <= taps_len / 2; i++) {
    int16_t tmp_real = bpfTaps[2 * (taps_len - 1 - i)];
    int16_t tmp_imag = bpfTaps[2 * (taps_len - 1 - i) + 1];
    bpfTaps[2 * (taps_len - 1 - i)] = bpfTaps[2 * i];
    bpfTaps[2 * (taps_len - 1 - i) + 1] = bpfTaps[2 * i + 1];
    bpfTaps[2 * i] = tmp_real;
    bpfTaps[2 * i + 1] = tmp_imag;
  }
  int code = create_aligned_taps(result, bpfTaps, taps_len);
  if (code != 0) {
    free(bpfTaps);
    destroy_xlating_cs16(result);
    return code;
  }
  free(bpfTaps);

  result->phase_real = (int16_t)(1 << 15);
  result->phase_imag = 0;
  complex float temp = cexpf(0.0f + -fwT0 * decimation * I);
  result->phase_incr_real = crealf(temp) * (1 << 15);
  result->phase_incr_imag = cimagf(temp) * (1 << 15);

  // max input length + history
  result->history_offset = 2 * (taps_len - 1);
  result->working_len_total = max_input_buffer_length + result->history_offset;
  result->working_buffer = sdrserver_aligned_alloc(result->alignment, sizeof(int16_t) * result->working_len_total);
  if (result->working_buffer == NULL) {
    destroy_xlating_cs16(result);
    return -ENOMEM;
  }
  memset(result->working_buffer, 0, sizeof(int16_t) * result->working_len_total);

  // +1 for case when round-up needed.
  result->output_len = max_input_buffer_length / decimation + 1;
  result->output = malloc(sizeof(int16_t) * result->output_len);
  if (result->output == NULL) {
    destroy_xlating_cs16(result);
    return -ENOMEM;
  }

  *filter = result;
  return 0;
}

void process_cu8_cs16(const uint8_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating_cs16 *filter) {
  for (size_t i = 0; i < input_len; i++) {
    filter->working_buffer[i + filter->history_offset] = (((int16_t)input[i]) - 127) << 8;
  }
  process_cs16(input_len, output, output_len, filter);
}

void process_cs8_cs16(const int8_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating_cs16 *filter) {
}

void process_cs16_cs16(const int16_t *input, size_t input_len, int16_t **output, size_t *output_len, xlating_cs16 *filter) {
}

void destroy_xlating_cs16(xlating_cs16 *filter) {
  if (filter == NULL) {
    return;
  }
  if (filter->taps != NULL) {
    for (size_t i = 0; i < filter->aligned_taps_len; i++) {
      free(filter->taps[i]);
    }
    free(filter->taps);
  }
  if (filter->original_taps != NULL) {
    free(filter->original_taps);
  }
  if (filter->output != NULL) {
    free(filter->output);
  }
  if (filter->working_buffer != NULL) {
    free(filter->working_buffer);
  }
  free(filter);
}