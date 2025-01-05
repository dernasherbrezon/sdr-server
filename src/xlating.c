#include "xlating.h"

#include <complex.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct xlating_t {
  uint32_t decimation;
  float complex **taps;
  size_t aligned_taps_len;
  size_t alignment;
  size_t taps_len;
  float *original_taps;

  float complex *working_buffer;
  size_t history_offset;
  size_t working_len_total;

  float complex *output;
  size_t output_len;

  float complex phase;
  float complex phase_incr;
};

void *sdrserver_aligned_alloc(size_t alignment, size_t size) {
  if (size % alignment != 0) {
    size = ((size / alignment) + 1) * alignment;
  }
  return aligned_alloc(alignment, size);
}

#if defined(NO_MANUAL_SIMD) || (!defined(__ARM_NEON) && !defined(__AVX__))

#ifdef NO_MANUAL_SIMD
const char *SIMD_STATUS = "Manually turned off";
#endif
#if (!defined(__ARM_NEON) && !defined(__AVX__))
const char *SIMD_STATUS = "Not detected";
#endif

void process_cf32(size_t input_complex_len, float complex **output, size_t *output_len, xlating *filter) {
  size_t working_len = filter->history_offset + input_complex_len;
  size_t produced = 0;
  size_t current_index = 0;

  // input might not have enough data to produce output sample
  if (working_len > (filter->taps_len - 1)) {
    size_t max_index = working_len - (filter->taps_len - 1);
    for (; current_index < max_index; current_index += filter->decimation, produced++) {
      const float complex *buf = (const float complex *)(filter->working_buffer + current_index);

      const float complex *aligned_buffer = (const float complex *)((size_t)buf & ~(filter->alignment - 1));
      unsigned align_index = buf - aligned_buffer;

      float complex temp = 0.0f + I * 0.0f;
      for (size_t i = 0; i < filter->taps_len + align_index; i++) {
        temp += aligned_buffer[i] * filter->taps[align_index][i];
      }
      filter->output[produced] = temp * filter->phase;
      filter->phase = filter->phase * filter->phase_incr;
    }
  }
  // preserve history for the next execution
  filter->history_offset = working_len - current_index;
  if (current_index > 0) {
    memmove(filter->working_buffer, filter->working_buffer + current_index, sizeof(float complex) * filter->history_offset);
  }

  *output = filter->output;
  *output_len = produced;
}

#elif defined(__ARM_NEON)
const char *SIMD_STATUS = "ARM NEON";
#include <arm_neon.h>

void process_cf32(size_t input_complex_len, float complex **output, size_t *output_len, xlating *filter) {
  size_t working_len = filter->history_offset + input_complex_len;
  size_t produced = 0;
  size_t current_index = 0;
  // input might not have enough data to produce output sample
  if (working_len > (filter->taps_len - 1)) {
    size_t max_index = working_len - (filter->taps_len - 1);
    for (; current_index < max_index; current_index += filter->decimation, produced++) {
      const float complex *buf = (const float complex *)(filter->working_buffer + current_index);

      const float complex *aligned_buffer = (const float complex *)((size_t)buf & ~(filter->alignment - 1));
      unsigned align_index = buf - aligned_buffer;

      float *pSrcA = (float *)aligned_buffer;
      float *pSrcB = (float *)filter->taps[align_index];
      uint32_t numSamples = filter->taps_len + align_index;

      uint32_t blkCnt;                            /* Loop counter */
      float32_t real_sum = 0.0f, imag_sum = 0.0f; /* Temporary result variables */
      float32_t a0, b0, c0, d0;

      float32x4x2_t vec1, vec2, vec3, vec4;
      float32x4_t accR, accI;
      float32x2_t accum = vdup_n_f32(0);

      accR = vdupq_n_f32(0.0f);
      accI = vdupq_n_f32(0.0f);

      /* Loop unrolling: Compute 8 outputs at a time */
      blkCnt = numSamples >> 3U;

      while (blkCnt > 0U) {
        /* C = (A[0]+jA[1])*(B[0]+jB[1]) + ...  */
        /* Calculate dot product and then store the result in a temporary buffer. */

        vec1 = vld2q_f32(pSrcA);
        vec2 = vld2q_f32(pSrcB);

        /* Increment pointers */
        pSrcA += 8;
        pSrcB += 8;

        /* Re{C} = Re{A}*Re{B} - Im{A}*Im{B} */
        accR = vmlaq_f32(accR, vec1.val[0], vec2.val[0]);
        accR = vmlsq_f32(accR, vec1.val[1], vec2.val[1]);

        /* Im{C} = Re{A}*Im{B} + Im{A}*Re{B} */
        accI = vmlaq_f32(accI, vec1.val[1], vec2.val[0]);
        accI = vmlaq_f32(accI, vec1.val[0], vec2.val[1]);

        vec3 = vld2q_f32(pSrcA);
        vec4 = vld2q_f32(pSrcB);

        /* Increment pointers */
        pSrcA += 8;
        pSrcB += 8;

        /* Re{C} = Re{A}*Re{B} - Im{A}*Im{B} */
        accR = vmlaq_f32(accR, vec3.val[0], vec4.val[0]);
        accR = vmlsq_f32(accR, vec3.val[1], vec4.val[1]);

        /* Im{C} = Re{A}*Im{B} + Im{A}*Re{B} */
        accI = vmlaq_f32(accI, vec3.val[1], vec4.val[0]);
        accI = vmlaq_f32(accI, vec3.val[0], vec4.val[1]);

        /* Decrement the loop counter */
        blkCnt--;
      }

      accum = vpadd_f32(vget_low_f32(accR), vget_high_f32(accR));
      real_sum += vget_lane_f32(accum, 0) + vget_lane_f32(accum, 1);

      accum = vpadd_f32(vget_low_f32(accI), vget_high_f32(accI));
      imag_sum += vget_lane_f32(accum, 0) + vget_lane_f32(accum, 1);

      /* Tail */
      blkCnt = numSamples & 0x7;

      while (blkCnt > 0U) {
        a0 = *pSrcA++;
        b0 = *pSrcA++;
        c0 = *pSrcB++;
        d0 = *pSrcB++;

        real_sum += a0 * c0;
        imag_sum += a0 * d0;
        real_sum -= b0 * d0;
        imag_sum += b0 * c0;

        /* Decrement loop counter */
        blkCnt--;
      }
      float complex sum_temp = real_sum + I * imag_sum;
      filter->output[produced] = sum_temp * filter->phase;
      filter->phase = filter->phase * filter->phase_incr;
    }
  }
  // preserve history for the next execution
  filter->history_offset = working_len - current_index;
  if (current_index > 0) {
    memmove(filter->working_buffer, filter->working_buffer + current_index, sizeof(float complex) * filter->history_offset);
  }

  *output = filter->output;
  *output_len = produced;
}

#elif defined(__AVX__)
const char *SIMD_STATUS = "AVX";
#include <immintrin.h>

void process_cf32(size_t input_complex_len, float complex **output, size_t *output_len, xlating *filter) {
  size_t working_len = filter->history_offset + input_complex_len;
  size_t produced = 0;
  size_t current_index = 0;

  // input might not have enough data to produce output sample
  if (working_len > (filter->taps_len - 1)) {
    size_t max_index = working_len - (filter->taps_len - 1);
    for (; current_index < max_index; current_index += filter->decimation, produced++) {
      const float complex *buf = (const float complex *)(filter->working_buffer + current_index);

      const float complex *aligned_buffer = (const float complex *)((size_t)buf & ~(filter->alignment - 1));
      unsigned align_index = buf - aligned_buffer;

      float *pSrcA = (float *)aligned_buffer;
      float *pSrcB = (float *)filter->taps[align_index];
      uint32_t numSamples = filter->taps_len + align_index;

      uint32_t blkCnt; /* Loop counter */
      __m256 x, y, yl, yh, z, tmp1, tmp2, res;

      res = _mm256_setzero_ps();

      /* Loop unrolling: Compute 8 outputs at a time */
      blkCnt = numSamples >> 2U;

      while (blkCnt > 0U) {
        x = _mm256_load_ps(pSrcA);
        y = _mm256_load_ps(pSrcB);

        pSrcA += 8;
        pSrcB += 8;

        yl = _mm256_moveldup_ps(y);
        yh = _mm256_movehdup_ps(y);
        tmp1 = _mm256_mul_ps(x, yl);
        x = _mm256_shuffle_ps(x, x, 0xB1);
        tmp2 = _mm256_mul_ps(x, yh);
        z = _mm256_addsub_ps(tmp1, tmp2);
        res = _mm256_add_ps(res, z);
        blkCnt--;
      }

      __attribute__((aligned(32))) float complex store[4];
      _mm256_store_ps((float *)store, res);

      /* Tail */
      blkCnt = numSamples & 0x3;
      float real_sum = 0.0f, imag_sum = 0.0f;
      float a0, b0, c0, d0;
      while (blkCnt > 0U) {
        a0 = *pSrcA++;
        b0 = *pSrcA++;
        c0 = *pSrcB++;
        d0 = *pSrcB++;

        real_sum += a0 * c0;
        imag_sum += a0 * d0;
        real_sum -= b0 * d0;
        imag_sum += b0 * c0;

        /* Decrement loop counter */
        blkCnt--;
      }
      float complex sum_temp = real_sum + I * imag_sum;
      filter->output[produced] = (sum_temp + store[0] + store[1] + store[2] + store[3]) * filter->phase;
      filter->phase = filter->phase * filter->phase_incr;
    }
  }
  // preserve history for the next execution
  filter->history_offset = working_len - current_index;
  if (current_index > 0) {
    memmove(filter->working_buffer, filter->working_buffer + current_index, sizeof(float complex) * filter->history_offset);
  }

  *output = filter->output;
  *output_len = produced;
}

#endif

void process_cu8(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter) {
  // input_len cannot be more than (working_len_total - history_offset)
  // convert to [-1.0;1.0] working buffer
  size_t input_complex_len = input_len / 2;
  for (size_t i = 0; i < input_complex_len; i++) {
    float real = ((float)input[2 * i] - 127.5F) / 128.0F;
    float imag = ((float)input[2 * i + 1] - 127.5F) / 128.0F;
    filter->working_buffer[i + filter->history_offset] = real + imag * I;
  }
  process_cf32(input_complex_len, output, output_len, filter);
}

void process_cs8(const int8_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter) {
  size_t input_complex_len = input_len / 2;
  for (size_t i = 0; i < input_complex_len; i++) {
    float real = input[2 * i] / 128.0F;
    float imag = input[2 * i + 1] / 128.0F;
    filter->working_buffer[i + filter->history_offset] = real + imag * I;
  }
  process_cf32(input_complex_len, output, output_len, filter);
}

void process_cs16(const int16_t *input, size_t input_len, float complex **output, size_t *output_len, xlating *filter) {
  size_t input_complex_len = input_len / 2;
  for (size_t i = 0; i < input_complex_len; i++) {
    float real = input[2 * i] / 32768.0F;
    float imag = input[2 * i + 1] / 32768.0F;
    filter->working_buffer[i + filter->history_offset] = real + imag * I;
  }
  process_cf32(input_complex_len, output, output_len, filter);
}

int create_aligned_taps(xlating *filter, float complex *bpfTaps, size_t taps_len) {
  size_t number_of_aligned = fmax((size_t)1, filter->alignment / sizeof(float complex));
  // Make a set of taps at all possible alignments
  float complex **result = malloc(number_of_aligned * sizeof(float complex *));
  if (result == NULL) {
    return -1;
  }
  for (int i = 0; i < number_of_aligned; i++) {
    size_t aligned_taps_len = taps_len + number_of_aligned - 1;
    result[i] = (float complex *)sdrserver_aligned_alloc(filter->alignment, aligned_taps_len * sizeof(float complex));
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
  *result = (struct xlating_t){0};
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

  result->phase = 1.0f + I * 0.0f;
  result->phase_incr = cexpf(0.0f + -fwT0 * decimation * I);

  // max input length + history
  result->history_offset = (taps_len - 1);
  result->working_len_total = max_input_buffer_length / 2 + result->history_offset;
  result->working_buffer = sdrserver_aligned_alloc(result->alignment, sizeof(float complex) * result->working_len_total);
  if (result->working_buffer == NULL) {
    destroy_xlating(result);
    return -ENOMEM;
  }
  memset(result->working_buffer, 0, sizeof(float complex) * result->working_len_total);

  // +1 for case when round-up needed.
  result->output_len = max_input_buffer_length / 2 / decimation + 1;
  result->output = malloc(sizeof(float complex) * result->output_len);
  if (result->output == NULL) {
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
