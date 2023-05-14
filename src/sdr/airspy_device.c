#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "airspy_device.h"

#define ERROR_CHECK(x)           \
  do {                           \
    int __err_rc = (x);          \
    if (__err_rc != 0) {         \
      airspy_device_destroy(device);                           \
      return __err_rc;           \
    }                            \
  } while (0)

struct airspy_device_t {
  uint32_t id;
  struct airspy_device *dev;
  void (*sdr_callback)(float *buf, uint32_t len, void *ctx);
  void *ctx;

  airspy_lib *lib;
};

int airspy_device_create(uint32_t id,  struct server_config *server_config,  airspy_lib *lib, void (*sdr_callback)(float *buf, uint32_t len, void *ctx), void *ctx, sdr_device **output) {
  fprintf(stdout, "airspy is starting\n");
  struct airspy_device_t *device = malloc(sizeof(struct airspy_device_t));
  if (device == NULL) {
    return -ENOMEM;
  }
  *device = (struct airspy_device_t) {0};
  device->lib = lib;
  device->sdr_callback = sdr_callback;
  device->ctx = ctx;
  ERROR_CHECK(lib->airspy_open(&device->dev));
  ERROR_CHECK(lib->airspy_set_sample_type(device->dev, AIRSPY_SAMPLE_FLOAT32_IQ));
  ERROR_CHECK(lib->airspy_set_samplerate(device->dev, server_config->band_sampling_rate));
  ERROR_CHECK(lib->airspy_set_packing(device->dev, 1));
  ERROR_CHECK(lib->airspy_set_rf_bias(device->dev, server_config->bias_t));
  //FIXME configure output buffer size
  switch (server_config->airspy_gain_mode) {
    case AIRSPY_GAIN_SENSITIVITY: {
      ERROR_CHECK(lib->airspy_set_sensitivity_gain(device->dev, server_config->airspy_sensitivity_gain));
      fprintf(stdout, "sensitivity gain is configured: %d\n", server_config->airspy_sensitivity_gain);
      break;
    }
    case AIRSPY_GAIN_LINEARITY: {
      ERROR_CHECK(lib->airspy_set_linearity_gain(device->dev, server_config->airspy_linearity_gain));
      fprintf(stdout, "linearity gain is configured: %d\n", server_config->airspy_linearity_gain);
      break;
    }
    case AIRSPY_GAIN_AUTO: {
      ERROR_CHECK(lib->airspy_set_lna_agc(device->dev, 1));
      ERROR_CHECK(lib->airspy_set_mixer_agc(device->dev, 1));
      fprintf(stdout, "auto gain is configured\n");
      break;
    }
    case AIRSPY_GAIN_MANUAL: {
      ERROR_CHECK(lib->airspy_set_vga_gain(device->dev, server_config->airspy_vga_gain));
      ERROR_CHECK(lib->airspy_set_mixer_gain(device->dev, server_config->airspy_mixer_gain));
      ERROR_CHECK(lib->airspy_set_lna_gain(device->dev, server_config->airspy_lna_gain));
      fprintf(stdout, "manual gain is configured: %d %d %d\n", server_config->airspy_vga_gain, server_config->airspy_mixer_gain, server_config->airspy_lna_gain);
      break;
    }
    default: {
      fprintf(stderr, "unknown airspy gain mode: %d\n", server_config->airspy_gain_mode);
      airspy_device_destroy(device);
      return 1;
    }
  }

  struct sdr_device_t *result = malloc(sizeof(struct sdr_device_t));
  if (result == NULL) {
    airspy_device_destroy(device);
    return -ENOMEM;
  }
  result->plugin = device;
  result->destroy = airspy_device_destroy;
  result->start_rx = airspy_device_start_rx;
  result->stop_rx = airspy_device_stop_rx;

  *output = result;
  return 0;
}

void airspy_device_destroy(void *plugin) {
  if( plugin == NULL ) {
    return;
  }
  struct airspy_device_t *device = (struct airspy_device_t *) plugin;
  if (device->dev != NULL) {
    device->lib->airspy_close(device->dev);
  }
  free(device);
}

int airspy_device_callback(airspy_transfer* transfer) {
  struct airspy_device_t *device = (struct airspy_device_t *) transfer->ctx;
  device->sdr_callback(transfer->samples, transfer->sample_count * 2, device->ctx);
  return 0;
}

int airspy_device_start_rx(uint32_t band_freq, void *plugin) {
  struct airspy_device_t *device = (struct airspy_device_t *) plugin;
  ERROR_CHECK(device->lib->airspy_set_freq(device->dev, band_freq));
  return device->lib->airspy_start_rx(device->dev, airspy_device_callback, device);
}

void airspy_device_stop_rx(void *plugin) {
  struct airspy_device_t *device = (struct airspy_device_t *) plugin;
  device->lib->airspy_stop_rx(device->dev);
}