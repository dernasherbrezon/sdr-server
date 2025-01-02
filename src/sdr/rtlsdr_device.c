#include "rtlsdr_device.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct rtlsdr_device_t {
  rtlsdr_dev_t *dev;
  pthread_t rtlsdr_device_thread;
  atomic_bool running;
  struct server_config *server_config;

  uint8_t *output;
  size_t output_len;

  void (*rtlsdr_callback)(uint8_t *buf, uint32_t len, void *ctx);
  void *ctx;

  rtlsdr_lib *lib;
};

#define ERROR_CHECK(x, y)                       \
  do {                                          \
    int __err_rc = (x);                         \
    if (__err_rc != 0) {                        \
      fprintf(stderr, "%s: %d\n", y, __err_rc); \
      device->lib->rtlsdr_close(device->dev);   \
      device->dev = NULL;                       \
      return __err_rc;                          \
    }                                           \
  } while (0)

int find_nearest_gain(struct rtlsdr_device_t *dev, int target_gain, int *nearest) {
  int count = dev->lib->rtlsdr_get_tuner_gains(dev->dev, NULL);
  if (count <= 0) {
    return -1;
  }
  int *gains = malloc(sizeof(int) * count);
  if (gains == NULL) {
    return -ENOMEM;
  }
  int code = dev->lib->rtlsdr_get_tuner_gains(dev->dev, gains);
  if (code <= 0) {
    free(gains);
    return -1;
  }
  *nearest = gains[0];
  for (int i = 0; i < count; i++) {
    int err1 = abs(target_gain - *nearest);
    int err2 = abs(target_gain - gains[i]);
    if (err2 < err1) {
      *nearest = gains[i];
    }
  }
  free(gains);
  return 0;
}

int rtlsdr_device_create(uint32_t id, struct server_config *server_config, rtlsdr_lib *lib, void (*rtlsdr_callback)(uint8_t *buf, uint32_t len, void *ctx), void *ctx, void **plugin) {
  struct rtlsdr_device_t *device = malloc(sizeof(struct rtlsdr_device_t));
  if (device == NULL) {
    return -ENOMEM;
  }
  *device = (struct rtlsdr_device_t){0};
  device->lib = lib;
  device->rtlsdr_callback = rtlsdr_callback;
  device->ctx = ctx;
  device->output_len = server_config->buffer_size;
  device->output = malloc(server_config->buffer_size * sizeof(uint8_t));
  if (device->output == NULL) {
    rtlsdr_device_destroy(device);
    return -ENOMEM;
  }
  memset(device->output, 0, server_config->buffer_size);
  device->server_config = server_config;
  fprintf(stdout, "rtl-sdr device created\n");
  *plugin = device;
  return 0;
}

static void *rtlsdr_callback(void *arg) {
  struct rtlsdr_device_t *device = (struct rtlsdr_device_t *)arg;
  device->running = true;
  int n_read = 0;
  while (device->running) {
    int code = device->lib->rtlsdr_read_sync(device->dev, device->output, device->output_len, &n_read);
    if (code != 0) {
      break;
    }
    device->rtlsdr_callback(device->output, n_read, device->ctx);
  }
  return (void *)0;
}

int rtlsdr_device_start_rx(uint32_t band_freq, void *plugin) {
  struct rtlsdr_device_t *device = (struct rtlsdr_device_t *)plugin;
  rtlsdr_lib *lib = device->lib;
  struct server_config *server_config = device->server_config;
  int device_index = -1;
  if (server_config->device_serial != NULL) {
    device_index = lib->rtlsdr_get_index_by_serial(server_config->device_serial);
    if (device_index < 0) {
      fprintf(stdout, "can't find device by serial: %s. fallback to device index\n", server_config->device_serial);
    }
  }
  if (device_index < 0) {
    device_index = server_config->device_index;
  }
  ERROR_CHECK(lib->rtlsdr_open(&device->dev, device_index), "<3>unable to open device");
  ERROR_CHECK(lib->rtlsdr_set_sample_rate(device->dev, server_config->band_sampling_rate), "<3>unable to set sample rate");
  ERROR_CHECK(lib->rtlsdr_set_tuner_gain_mode(device->dev, server_config->gain_mode), "<3>unable to set gain mode");
  if (server_config->ppm != 0) {
    ERROR_CHECK(lib->rtlsdr_set_freq_correction(device->dev, server_config->ppm), "<3>unable to set freq correction");
  }
  if (server_config->gain_mode == 1) {
    int nearest_gain = 0;
    ERROR_CHECK(find_nearest_gain(device, server_config->gain, &nearest_gain), "<3>unable to find nearest gain");
    if (nearest_gain != server_config->gain) {
      fprintf(stdout, "the actual nearest supported gain is: %f\n", (float)nearest_gain / 10);
    }
    ERROR_CHECK(lib->rtlsdr_set_tuner_gain(device->dev, nearest_gain), "<3>unable to set gain");
  }
  ERROR_CHECK(lib->rtlsdr_set_bias_tee(device->dev, server_config->bias_t), "<3>unable to set bias tee");
  ERROR_CHECK(lib->rtlsdr_reset_buffer(device->dev), "<3>unable to reset buffers");
  ERROR_CHECK(lib->rtlsdr_set_center_freq(device->dev, band_freq), "<3>unable to set freq");
  int code = pthread_create(&device->rtlsdr_device_thread, NULL, &rtlsdr_callback, device);
  if (code != 0) {
    return 0x04;
  }
  return 0;
}

void rtlsdr_device_stop_rx(void *plugin) {
  if (plugin == NULL) {
    return;
  }
  struct rtlsdr_device_t *device = (struct rtlsdr_device_t *)plugin;
  device->lib->rtlsdr_close(device->dev);
  device->dev = NULL;
  device->running = false;
  pthread_join(device->rtlsdr_device_thread, NULL);
}

void rtlsdr_device_destroy(void *plugin) {
  if (plugin == NULL) {
    return;
  }
  struct rtlsdr_device_t *device = (struct rtlsdr_device_t *)plugin;
  if (device->output != NULL) {
    free(device->output);
  }
  free(device);
  fprintf(stdout, "rtl-sdr device destroyed\n");
}
