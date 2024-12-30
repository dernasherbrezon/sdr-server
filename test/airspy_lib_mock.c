#include "airspy_lib_mock.h"

#include <airspy.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/sdr/airspy_lib.h"

struct mock_status {
  int16_t *buffer;
  int len;

  pthread_t worker_thread;
  airspy_sample_block_cb_fn callback;
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  bool stopped;
  int data_was_read;
};

struct rtlsdr_dev {
  int dummy;
};

struct mock_status airspy_mock = {0};

int airspy_open(struct airspy_device **device) {
  return 0;
}

int airspy_set_sample_type(struct airspy_device *device, enum airspy_sample_type sample_type) {
  return 0;
}

int airspy_close(struct airspy_device *device) {
  return 0;
}

int airspy_set_lna_agc(struct airspy_device *device, uint8_t value) {
  return 0;
}

int airspy_set_mixer_agc(struct airspy_device *device, uint8_t value) {
  return 0;
}

int airspy_set_samplerate(struct airspy_device *device, uint32_t samplerate) {
  return 0;
}

int airspy_set_packing(struct airspy_device *device, uint8_t value) {
  return 0;
}

int airspy_set_rf_bias(struct airspy_device *dev, uint8_t value) {
  return 0;
}

int airspy_set_lna_gain(struct airspy_device *device, uint8_t value) {
  return 0;
}

int airspy_set_vga_gain(struct airspy_device *device, uint8_t value) {
  return 0;
}

int airspy_set_mixer_gain(struct airspy_device *device, uint8_t value) {
  return 0;
}

int airspy_set_linearity_gain(struct airspy_device *device, uint8_t value) {
  return 0;
}

int airspy_set_sensitivity_gain(struct airspy_device *device, uint8_t value) {
  return 0;
}

static void *airspy_worker(void *ctx) {
  pthread_mutex_lock(&airspy_mock.mutex);
  while (!airspy_mock.stopped) {
    if (airspy_mock.data_was_read) {
      pthread_cond_broadcast(&airspy_mock.condition);
    }
    if (airspy_mock.buffer == NULL) {
      pthread_cond_wait(&airspy_mock.condition, &airspy_mock.mutex);
    }
    if (airspy_mock.buffer != NULL) {
      airspy_transfer transfer = {
          .ctx = ctx,
          .device = NULL,
          .dropped_samples = 0,
          .sample_count = airspy_mock.len / 2,
          .sample_type = AIRSPY_SAMPLE_INT16_IQ,
          .samples = airspy_mock.buffer};
      airspy_mock.callback(&transfer);
      airspy_mock.buffer = NULL;
      airspy_mock.data_was_read = true;
      pthread_cond_broadcast(&airspy_mock.condition);
    }
  }
  pthread_mutex_unlock(&airspy_mock.mutex);
  return (void *)0;
}

int airspy_start_rx(struct airspy_device *device, airspy_sample_block_cb_fn callback, void *rx_ctx) {
  airspy_mock.callback = callback;
  return pthread_create(&airspy_mock.worker_thread, NULL, &airspy_worker, rx_ctx);
}

int airspy_set_freq(struct airspy_device *device, const uint32_t freq_hz) {
  return 0;
}

int airspy_stop_rx(struct airspy_device *device) {
  airspy_stop_mock();
  return 0;
}

void airspy_lib_version(airspy_lib_version_t *lib_version) {
}

int airspy_lib_create(airspy_lib **lib) {
  struct airspy_lib_t *result = malloc(sizeof(struct airspy_lib_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct airspy_lib_t){0};
  result->airspy_close = airspy_close;
  result->airspy_lib_version = airspy_lib_version;
  result->airspy_open = airspy_open;
  result->airspy_set_freq = airspy_set_freq;
  result->airspy_set_linearity_gain = airspy_set_linearity_gain;
  result->airspy_set_lna_agc = airspy_set_lna_agc;
  result->airspy_set_lna_gain = airspy_set_lna_gain;
  result->airspy_set_mixer_agc = airspy_set_mixer_agc;
  result->airspy_set_mixer_gain = airspy_set_mixer_gain;
  result->airspy_set_packing = airspy_set_packing;
  result->airspy_set_rf_bias = airspy_set_rf_bias;
  result->airspy_set_sample_type = airspy_set_sample_type;
  result->airspy_set_samplerate = airspy_set_samplerate;
  result->airspy_set_sensitivity_gain = airspy_set_sensitivity_gain;
  result->airspy_set_vga_gain = airspy_set_vga_gain;
  result->airspy_start_rx = airspy_start_rx;
  result->airspy_stop_rx = airspy_stop_rx;

  airspy_mock.mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  airspy_mock.condition = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
  airspy_mock.stopped = false;
  airspy_mock.data_was_read = false;
  *lib = result;
  return 0;
}

void airspy_lib_destroy(airspy_lib *lib) {
  if (lib == NULL) {
    return;
  }
  free(lib);
}

void airspy_wait_for_data_read() {
  pthread_mutex_lock(&airspy_mock.mutex);
  while (!airspy_mock.data_was_read) {
    pthread_cond_wait(&airspy_mock.condition, &airspy_mock.mutex);
  }
  pthread_cond_broadcast(&airspy_mock.condition);
  pthread_mutex_unlock(&airspy_mock.mutex);
}

void airspy_setup_mock_data(int16_t *buffer, int len) {
  pthread_mutex_lock(&airspy_mock.mutex);
  airspy_mock.buffer = buffer;
  airspy_mock.len = len;
  pthread_cond_broadcast(&airspy_mock.condition);
  pthread_mutex_unlock(&airspy_mock.mutex);
}

void airspy_stop_mock() {
  pthread_mutex_lock(&airspy_mock.mutex);
  airspy_mock.stopped = true;
  airspy_mock.buffer = NULL;
  pthread_cond_broadcast(&airspy_mock.condition);
  pthread_mutex_unlock(&airspy_mock.mutex);
}
