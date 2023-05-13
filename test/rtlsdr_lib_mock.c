#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "../src/sdr/rtlsdr_lib.h"
#include "rtlsdr_lib_mock.h"

struct mock_status {
  uint8_t *buffer;
  int len;

  pthread_mutex_t mutex;
  pthread_cond_t condition;
  int stopped;
  int data_was_read;
};

struct rtlsdr_dev {
  int dummy;
};

struct mock_status mock;

void init_mock_librtlsdr() {
  mock.mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
  mock.condition = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
  mock.stopped = false;
  mock.data_was_read = false;
  mock.buffer = NULL;
}

// make sure data was read
// the core will be terminated gracefully
// so the data will be processed
void wait_for_data_read() {
  pthread_mutex_lock(&mock.mutex);
  while (!mock.data_was_read) {
    pthread_cond_wait(&mock.condition, &mock.mutex);
  }
  pthread_cond_broadcast(&mock.condition);
  pthread_mutex_unlock(&mock.mutex);
}

void setup_mock_data(uint8_t *buffer, int len) {
  pthread_mutex_lock(&mock.mutex);
  mock.buffer = buffer;
  mock.len = len;
  pthread_cond_broadcast(&mock.condition);
  pthread_mutex_unlock(&mock.mutex);
}

void stop_rtlsdr_mock() {
  pthread_mutex_lock(&mock.mutex);
  mock.stopped = true;
  mock.buffer = NULL;
  pthread_cond_broadcast(&mock.condition);
  pthread_mutex_unlock(&mock.mutex);
}

int rtlsdr_read_sync_mocked(rtlsdr_dev_t *dev, void *buf, int len, int *n_read) {
  pthread_mutex_lock(&mock.mutex);
  while (!mock.stopped) {
    if (mock.data_was_read) {
      pthread_cond_broadcast(&mock.condition);
    }
    if (mock.buffer == NULL) {
      pthread_cond_wait(&mock.condition, &mock.mutex);
    }
    if (mock.buffer != NULL) {
      memcpy(buf, mock.buffer, mock.len);
      *n_read = mock.len;
      mock.buffer = NULL;
      mock.data_was_read = true;
      pthread_mutex_unlock(&mock.mutex);
      return 0;
    }
  }
  pthread_mutex_unlock(&mock.mutex);
  *n_read = 0;
  return -1;
}

int rtlsdr_close_mocked(rtlsdr_dev_t *dev) {
  if (dev != NULL) {
    free(dev);
  }
  return 0;
}

int rtlsdr_open_mocked(rtlsdr_dev_t **dev, uint32_t index) {
  mock.stopped = false;
  mock.data_was_read = false;
  rtlsdr_dev_t *result = malloc(sizeof(rtlsdr_dev_t));
  *dev = result;
  return 0;
}

int rtlsdr_set_sample_rate_mocked(rtlsdr_dev_t *dev, uint32_t rate) {
  return 0;
}

int rtlsdr_set_center_freq_mocked(rtlsdr_dev_t *dev, uint32_t freq) {
  return 0;
}

int rtlsdr_set_tuner_gain_mode_mocked(rtlsdr_dev_t *dev, int manual) {
  return 0;
}

int rtlsdr_set_tuner_gain_mocked(rtlsdr_dev_t *dev, int gain) {
  return 0;
}

int rtlsdr_set_bias_tee_mocked(rtlsdr_dev_t *dev, int on) {
  return 0;
}

int rtlsdr_reset_buffer_mocked(rtlsdr_dev_t *dev) {
  return 0;
}

int rtlsdr_get_tuner_gains_mocked(rtlsdr_dev_t *dev, int *gains) {
  if (gains == NULL) {
    return 1;
  }
  gains[0] = 43;
  return 1;
}

int rtlsdr_lib_create(rtlsdr_lib **lib) {
  struct rtlsdr_lib_t *result = malloc(sizeof(struct rtlsdr_lib_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct rtlsdr_lib_t) {0};
  result->rtlsdr_open = rtlsdr_open_mocked;
  result->rtlsdr_close = rtlsdr_close_mocked;
  result->rtlsdr_get_tuner_gains = rtlsdr_get_tuner_gains_mocked;
  result->rtlsdr_reset_buffer = rtlsdr_reset_buffer_mocked;
  result->rtlsdr_set_bias_tee = rtlsdr_set_bias_tee_mocked;
  result->rtlsdr_set_center_freq = rtlsdr_set_center_freq_mocked;
  result->rtlsdr_set_sample_rate = rtlsdr_set_sample_rate_mocked;
  result->rtlsdr_set_tuner_gain = rtlsdr_set_tuner_gain_mocked;
  result->rtlsdr_set_tuner_gain_mode = rtlsdr_set_tuner_gain_mode_mocked;
  result->rtlsdr_read_sync = rtlsdr_read_sync_mocked;

  *lib = result;
  return 0;
}

void rtlsdr_lib_destroy(rtlsdr_lib *lib) {
  if (lib == NULL) {
    return;
  }
  free(lib);
}

