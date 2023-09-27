#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <volk/volk.h>
#include <limits.h>
#include <string.h>

#include "xlating.h"
#include "rotator.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct xlating_t {
  uint32_t decimation;
  float complex **taps;
  size_t aligned_taps_len;
  size_t alignment;
  size_t taps_len;
  rotator *rot;
  float *original_taps;

  float *working_buffer;
  size_t history_offset;
  size_t working_len_total;

  float complex *volk_output;

  float complex *output;
  size_t output_len;
};

void process(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter) {
  // input_len cannot be more than (working_len_total - history_offset)

  // convert to [-1.0;1.0] working buffer
  volk_8i_s32f_convert_32f_u(filter->working_buffer + filter->history_offset, (const signed char *) input, 128.0F, input_len);
  size_t working_len = filter->history_offset + input_len;
  size_t produced = 0;
  size_t current_index = 0;
  // input might not have enough data to produce output sample
  if (working_len > (filter->taps_len - 1) * 2) {
    size_t max_index = working_len - (filter->taps_len - 1) * 2;
    for (; current_index < max_index; current_index += 2 * filter->decimation, produced++) {
      const lv_32fc_t *buf = (const lv_32fc_t *) (filter->working_buffer + current_index);

      const lv_32fc_t *aligned_buffer = (const lv_32fc_t *) ((size_t) buf & ~(filter->alignment - 1));
      unsigned align_index = buf - aligned_buffer;

      volk_32fc_x2_dot_prod_32fc_a(filter->volk_output, aligned_buffer, (const lv_32fc_t *) filter->taps[align_index], filter->taps_len + align_index);
      filter->output[produced] = *filter->volk_output;
    }

    rotator_increment_batch(filter->rot, filter->output, filter->output, produced);
  }
  // preserve history for the next execution
  filter->history_offset = working_len - current_index;
  if (current_index > 0) {
    memmove(filter->working_buffer, filter->working_buffer + current_index, sizeof(float) * filter->history_offset);
  }

  *output = filter->output;
  *output_len = produced;
}

int create_aligned_taps(xlating *filter, float complex *bpfTaps, size_t taps_len) {
  size_t alignment = volk_get_alignment();
  size_t number_of_aligned = fmax((size_t) 1, alignment / sizeof(float complex));
  // Make a set of taps at all possible alignments
  float complex **result = malloc(number_of_aligned * sizeof(float complex *));
  if (result == NULL) {
    return -1;
  }
  for (int i = 0; i < number_of_aligned; i++) {
    size_t aligned_taps_len = taps_len + number_of_aligned - 1;
    result[i] = (float complex *) volk_malloc(aligned_taps_len * sizeof(float complex), alignment);
    // some taps will be longer than original, but
    // since they contain zeros, multiplication on an input will produce 0
    // there is a tradeoff: multiply unaligned input or
    // multiply aligned input but with additional zeros
    for (size_t j = 0; j < aligned_taps_len; j++) {
      result[i][j] = 0;
    }
    for (size_t j = 0; j < taps_len; j++) {
      result[i][i + j] = bpfTaps[j];
    }
  }
  filter->taps = result;
  filter->aligned_taps_len = number_of_aligned;
  filter->alignment = alignment;
  return 0;
}

int create_frequency_xlating_filter(uint32_t decimation, float *taps, size_t taps_len, int32_t center_freq, uint32_t sampling_freq, uint32_t max_input_buffer_length, xlating **filter) {
  if (taps_len == 0) {
    return -1;
  }
  struct xlating_t *result = malloc(sizeof(struct xlating_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0
  *result = (struct xlating_t) {0};
  result->decimation = decimation;
  result->taps_len = taps_len;
  //make code consistent - i.e. destroy all incoming memory in destory_xxx methods
  result->original_taps = taps;

  // The basic principle of this block is to perform:
  //    x(t) -> (mult by -fwT0) -> LPF -> decim -> y(t)
  // We switch things up here to:
  //    x(t) -> BPF -> decim -> (mult by fwT0*decim) -> y(t)
  // The BPF is the baseband filter (LPF) moved up to the
  // center frequency fwT0. We then apply a derotator
  // with -fwT0 to downshift the signal to baseband.
  float complex *bpfTaps = malloc(sizeof(float complex) * taps_len);
  if (bpfTaps == NULL) {
    destroy_xlating(result);
    return -ENOMEM;
  }
  float fwT0 = 2 * M_PI * center_freq / sampling_freq;
  for (size_t i = 0; i < taps_len; i++) {
    float complex cur = 0 + i * fwT0 * I;
    bpfTaps[i] = taps[i] * cexpf(cur);
  }
  // reverse taps to allow storing history in array
  for (size_t i = 0; i <= taps_len / 2; i++) {
    float complex tmp = bpfTaps[taps_len - 1 - i];
    bpfTaps[taps_len - 1 - i] = bpfTaps[i];
    bpfTaps[i] = tmp;
  }
  int code = create_aligned_taps(result, bpfTaps, taps_len);
  if (code != 0) {
    free(bpfTaps);
    destroy_xlating(result);
    return code;
  }
  free(bpfTaps);

  float complex phase = 1.0 + 0.0 * I;
  float complex phase_incr = cexpf(0.0f + -fwT0 * decimation * I);
  rotator *rot = NULL;
  code = create_rotator(phase, phase_incr, &rot);
  if (code != 0) {
    destroy_xlating(result);
    return code;
  }
  result->rot = rot;

  // max input length + history
  // taps_len is a complex number. i.e. 2 floats
  result->history_offset = (taps_len - 1) * 2;
  result->working_len_total = max_input_buffer_length + result->history_offset;
  size_t alignment = volk_get_alignment();
  result->working_buffer = volk_malloc(sizeof(float) * result->working_len_total, alignment);
  if (result->working_buffer == NULL) {
    destroy_xlating(result);
    return -ENOMEM;
  }
  for (size_t i = 0; i < result->working_len_total; i++) {
    result->working_buffer[i] = 0.0;
  }

  // +1 for case when round-up needed.
  result->output_len = max_input_buffer_length / 2 / decimation + 1;
  result->output = malloc(sizeof(float complex) * result->output_len);
  if (result->output == NULL) {
    destroy_xlating(result);
    return -ENOMEM;
  }

  result->volk_output = volk_malloc(1 * sizeof(float complex), alignment);
  if (result->volk_output == NULL) {
    destroy_xlating(result);
    return -ENOMEM;
  }

  *filter = result;
  return 0;
}

void destroy_xlating(xlating *filter) {
  if (filter == NULL) {
    return;
  }
  if (filter->taps != NULL) {
    for (size_t i = 0; i < filter->aligned_taps_len; i++) {
      volk_free(filter->taps[i]);
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
    volk_free(filter->working_buffer);
  }
  if (filter->volk_output != NULL) {
    volk_free(filter->volk_output);
  }
  destroy_rotator(filter->rot);
  free(filter);
}
