#include "hackrf_lib_mock.h"

#include <errno.h>
#include <hackrf.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/sdr/hackrf_lib.h"

struct mock_status {
  int8_t* buffer;
  int len;

  pthread_t worker_thread;
  hackrf_sample_block_cb_fn callback;
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  bool stopped;
  int data_was_read;
};

struct rtlsdr_dev {
  int dummy;
};

struct mock_status hackrf_mock = {0};

static void* hackrf_worker(void* ctx) {
  pthread_mutex_lock(&hackrf_mock.mutex);
  while (!hackrf_mock.stopped) {
    if (hackrf_mock.data_was_read) {
      pthread_cond_broadcast(&hackrf_mock.condition);
    }
    if (hackrf_mock.buffer == NULL) {
      pthread_cond_wait(&hackrf_mock.condition, &hackrf_mock.mutex);
    }
    if (hackrf_mock.buffer != NULL) {
      hackrf_transfer transfer = {
          .rx_ctx = ctx,
          .device = NULL,
          .buffer = hackrf_mock.buffer,
          .buffer_length = hackrf_mock.len};
      hackrf_mock.callback(&transfer);
      hackrf_mock.buffer = NULL;
      hackrf_mock.data_was_read = true;
      pthread_cond_broadcast(&hackrf_mock.condition);
    }
  }
  pthread_mutex_unlock(&hackrf_mock.mutex);
  return (void*)0;
}

int hackrf_start_rx(hackrf_device* device, hackrf_sample_block_cb_fn callback, void* rx_ctx) {
  hackrf_mock.callback = callback;
  return pthread_create(&hackrf_mock.worker_thread, NULL, &hackrf_worker, rx_ctx);
}

int hackrf_init() {
  return 0;
}
const char* hackrf_error_name(enum hackrf_error errcode) {
  return "unknown";
}
int hackrf_version_string_read(hackrf_device* device, char* version, uint8_t length) {
  snprintf(version, length, "test\0");
  return 0;
}
int hackrf_usb_api_version_read(hackrf_device* device, uint16_t* version) {
  return 0;
}
int hackrf_set_vga_gain(hackrf_device* device, uint32_t value) {
  return 0;
}
int hackrf_set_lna_gain(hackrf_device* device, uint32_t value) {
  return 0;
}
int hackrf_set_amp_enable(hackrf_device* device, const uint8_t value) {
  return 0;
}
int hackrf_set_freq(hackrf_device* device, const uint64_t freq_hz) {
  return 0;
}
int hackrf_set_baseband_filter_bandwidth(hackrf_device* device, const uint32_t bandwidth_hz) {
  return 0;
}
int hackrf_set_sample_rate(hackrf_device* device, const double freq_hz) {
  return 0;
}
int hackrf_stop_rx(hackrf_device* device) {
  return 0;
}
int hackrf_close(hackrf_device* device) {
  return 0;
}
int hackrf_exit() {
  return 0;
}
int hackrf_open(hackrf_device** device) {
  return 0;
}
int hackrf_open_by_serial(const char* const desired_serial_number, hackrf_device** device) {
  return 0;
}
int hackrf_set_antenna_enable(hackrf_device* device, const uint8_t value) {
  return 0;
}

int hackrf_lib_create(hackrf_lib** lib) {
  struct hackrf_lib_t* result = malloc(sizeof(struct hackrf_lib_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct hackrf_lib_t){0};

  result->hackrf_init = hackrf_init;
  result->hackrf_error_name = hackrf_error_name;
  result->hackrf_version_string_read = hackrf_version_string_read;
  result->hackrf_usb_api_version_read = hackrf_usb_api_version_read;
  result->hackrf_set_vga_gain = hackrf_set_vga_gain;
  result->hackrf_set_lna_gain = hackrf_set_lna_gain;
  result->hackrf_set_amp_enable = hackrf_set_amp_enable;
  result->hackrf_set_freq = hackrf_set_freq;
  result->hackrf_set_baseband_filter_bandwidth = hackrf_set_baseband_filter_bandwidth;
  result->hackrf_set_sample_rate = hackrf_set_sample_rate;
  result->hackrf_start_rx = hackrf_start_rx;
  result->hackrf_stop_rx = hackrf_stop_rx;
  result->hackrf_close = hackrf_close;
  result->hackrf_exit = hackrf_exit;
  result->hackrf_open = hackrf_open;
  result->hackrf_open_by_serial = hackrf_open_by_serial;
  result->hackrf_set_antenna_enable = hackrf_set_antenna_enable;

  hackrf_mock.mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  hackrf_mock.condition = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
  hackrf_mock.stopped = false;
  hackrf_mock.data_was_read = false;
  *lib = result;
  return 0;
}

void hackrf_lib_destroy(hackrf_lib* lib) {
  if (lib == NULL) {
    return;
  }
  free(lib);
}

void hackrf_wait_for_data_read() {
  pthread_mutex_lock(&hackrf_mock.mutex);
  while (!hackrf_mock.data_was_read) {
    pthread_cond_wait(&hackrf_mock.condition, &hackrf_mock.mutex);
  }
  pthread_cond_broadcast(&hackrf_mock.condition);
  pthread_mutex_unlock(&hackrf_mock.mutex);
}

void hackrf_setup_mock_data(int8_t* buffer, int len) {
  pthread_mutex_lock(&hackrf_mock.mutex);
  hackrf_mock.buffer = buffer;
  hackrf_mock.len = len;
  pthread_cond_broadcast(&hackrf_mock.condition);
  pthread_mutex_unlock(&hackrf_mock.mutex);
}

void hackrf_stop_mock() {
  pthread_mutex_lock(&hackrf_mock.mutex);
  hackrf_mock.stopped = true;
  hackrf_mock.buffer = NULL;
  pthread_cond_broadcast(&hackrf_mock.condition);
  pthread_mutex_unlock(&hackrf_mock.mutex);
  pthread_join(hackrf_mock.worker_thread, NULL);
}
