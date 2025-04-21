#include "sdr_device.h"

#include <complex.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "sdr/airspy_device.h"
#include "sdr/hackrf_device.h"
#include "sdr/rtlsdr_device.h"

struct sdr_device_t {
  struct server_config *server_config;

  rtlsdr_lib *rtllib;
  airspy_lib *airspy;
  hackrf_lib *hackrf;
  void (*sdr_callback)(uint8_t *buf, uint32_t buf_len, void *ctx);
  void *ctx;

  void *plugin;
  void (*destroy)(void *plugin);
  int (*start_rx)(uint32_t band_freq, void *plugin);
  void (*stop_rx)(void *plugin);
};

int sdr_device_create(void (*sdr_callback)(uint8_t *buf, uint32_t buf_len, void *ctx), void *ctx, struct server_config *server_config, sdr_device **device) {
  struct sdr_device_t *result = malloc(sizeof(struct sdr_device_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct sdr_device_t){0};
  result->server_config = server_config;
  result->sdr_callback = sdr_callback;
  result->ctx = ctx;
  int code = -1;
  switch (server_config->sdr_type) {
    case SDR_TYPE_RTL: {
      code = rtlsdr_lib_create(&result->rtllib);
      result->destroy = rtlsdr_device_destroy;
      result->start_rx = rtlsdr_device_start_rx;
      result->stop_rx = rtlsdr_device_stop_rx;
      break;
    }
    case SDR_TYPE_AIRSPY: {
      code = airspy_lib_create(&result->airspy);
      result->destroy = airspy_device_destroy;
      result->start_rx = airspy_device_start_rx;
      result->stop_rx = airspy_device_stop_rx;
      break;
    }
    case SDR_TYPE_HACKRF: {
      code = hackrf_lib_create(&result->hackrf);
      result->destroy = hackrf_device_destroy;
      result->start_rx = hackrf_device_start_rx;
      result->stop_rx = hackrf_device_stop_rx;
      break;
    }
    default: {
      fprintf(stderr, "<3>unsupported sdr type: %d\n", server_config->sdr_type);
      code = -1;
      break;
    }
  }
  if (code != 0) {
    sdr_device_destroy(result);
    return code;
  }
  *device = result;
  return 0;
}

int sdr_device_start(client_config *config, sdr_device *device) {
  if (device->plugin == NULL) {
    int code = -1;
    switch (config->sdr_type) {
      case SDR_TYPE_RTL: {
        code = rtlsdr_device_create(device->server_config, device->rtllib, device->sdr_callback, device->ctx, &device->plugin);
        break;
      }
      case SDR_TYPE_AIRSPY: {
        code = airspy_device_create(device->server_config, device->airspy, device->sdr_callback, device->ctx, &device->plugin);
        break;
      }
      case SDR_TYPE_HACKRF: {
        code = hackrf_device_create(device->server_config, device->hackrf, device->sdr_callback, device->ctx, &device->plugin);
        break;
      }
      default: {
        fprintf(stderr, "<3>unsupported sdr type: %d\n", config->sdr_type);
        code = -1;
        break;
      }
    }
    if (code != 0) {
      fprintf(stderr, "<3>unable to create device\n");
      return 0x04;
    }
  }
  int code = device->start_rx(config->band_freq, device->plugin);
  if (code != 0) {
    fprintf(stderr, "<3>unable to start rx\n");
    return 0x04;
  }
  // reset internal state
  fprintf(stdout, "sdr created\n");
  return 0;
}

void sdr_device_stop(sdr_device *device) {
  // synchronous wait until all threads shutdown
  device->stop_rx(device->plugin);
}

void sdr_device_destroy(sdr_device *device) {
  if (device == NULL) {
    return;
  }
  if (device->plugin != NULL) {
    device->destroy(device->plugin);
  }
  if (device->rtllib != NULL) {
    rtlsdr_lib_destroy(device->rtllib);
  }
  if (device->airspy != NULL) {
    airspy_lib_destroy(device->airspy);
  }
  if (device->hackrf != NULL) {
    hackrf_lib_destroy(device->hackrf);
  }
  fprintf(stdout, "sdr destroyed\n");
  free(device);
}
