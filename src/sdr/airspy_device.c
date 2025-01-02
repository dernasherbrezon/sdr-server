#include "airspy_device.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define ERROR_CHECK(x, y)                       \
  do {                                          \
    int __err_rc = (x);                         \
    if (__err_rc != 0) {                        \
      fprintf(stderr, "%s: %d\n", y, __err_rc); \
      if (device->dev != NULL) {                \
        device->lib->airspy_close(device->dev); \
        device->dev = NULL;                     \
      }                                         \
      return __err_rc;                          \
    }                                           \
  } while (0)

struct airspy_device_t {
  uint32_t id;
  struct airspy_device *dev;
  void (*sdr_callback)(uint8_t *buf, uint32_t len, void *ctx);
  void *ctx;

  struct server_config *server_config;
  airspy_lib *lib;
};

int airspy_device_create(uint32_t id, struct server_config *server_config, airspy_lib *lib, void (*sdr_callback)(uint8_t *buf, uint32_t len, void *ctx), void *ctx, void **plugin) {
  struct airspy_device_t *device = malloc(sizeof(struct airspy_device_t));
  if (device == NULL) {
    return -ENOMEM;
  }
  *device = (struct airspy_device_t){0};
  device->lib = lib;
  device->sdr_callback = sdr_callback;
  device->ctx = ctx;
  device->server_config = server_config;
  fprintf(stdout, "airspy device created\n");
  *plugin = device;
  return 0;
}

void airspy_device_destroy(void *plugin) {
  if (plugin == NULL) {
    return;
  }
  struct airspy_device_t *device = (struct airspy_device_t *)plugin;
  free(device);
  fprintf(stdout, "airspy device destroyed\n");
}

int airspy_device_callback(airspy_transfer *transfer) {
  struct airspy_device_t *device = (struct airspy_device_t *)transfer->ctx;
  device->sdr_callback(transfer->samples, transfer->sample_count * 2 * sizeof(int16_t), device->ctx);
  return 0;
}

int airspy_device_start_rx(uint32_t band_freq, void *plugin) {
  struct airspy_device_t *device = (struct airspy_device_t *)plugin;
  struct server_config *server_config = device->server_config;
  ERROR_CHECK(device->lib->airspy_open(&device->dev), "<3>unable to init airspy device");
  ERROR_CHECK(device->lib->airspy_set_sample_type(device->dev, AIRSPY_SAMPLE_INT16_IQ), "<3>unable to set sample type int16 iq");
  ERROR_CHECK(device->lib->airspy_set_samplerate(device->dev, server_config->band_sampling_rate), "<3>unable to set sample rate");
  ERROR_CHECK(device->lib->airspy_set_packing(device->dev, 1), "<3>unable to set packing");
  ERROR_CHECK(device->lib->airspy_set_rf_bias(device->dev, server_config->bias_t), "<3>unable to set bias_t");
  switch (server_config->airspy_gain_mode) {
    case AIRSPY_GAIN_SENSITIVITY: {
      ERROR_CHECK(device->lib->airspy_set_sensitivity_gain(device->dev, server_config->airspy_sensitivity_gain), "<3>unable to set sensitivity gain");
      fprintf(stdout, "sensitivity gain is configured: %d\n", server_config->airspy_sensitivity_gain);
      break;
    }
    case AIRSPY_GAIN_LINEARITY: {
      ERROR_CHECK(device->lib->airspy_set_linearity_gain(device->dev, server_config->airspy_linearity_gain), "<3>unable to set linearity gain");
      fprintf(stdout, "linearity gain is configured: %d\n", server_config->airspy_linearity_gain);
      break;
    }
    case AIRSPY_GAIN_AUTO: {
      ERROR_CHECK(device->lib->airspy_set_lna_agc(device->dev, 1), "<3>unable to set lna agc");
      ERROR_CHECK(device->lib->airspy_set_mixer_agc(device->dev, 1), "<3>unable to set mixer agc");
      fprintf(stdout, "auto gain is configured\n");
      break;
    }
    case AIRSPY_GAIN_MANUAL: {
      ERROR_CHECK(device->lib->airspy_set_vga_gain(device->dev, server_config->airspy_vga_gain), "<3>unable to set vga gain");
      ERROR_CHECK(device->lib->airspy_set_mixer_gain(device->dev, server_config->airspy_mixer_gain), "<3>unable to set mixer gain");
      ERROR_CHECK(device->lib->airspy_set_lna_gain(device->dev, server_config->airspy_lna_gain), "<3>unable to set lna gain");
      fprintf(stdout, "manual gain is configured: %d %d %d\n", server_config->airspy_vga_gain, server_config->airspy_mixer_gain, server_config->airspy_lna_gain);
      break;
    }
    default: {
      fprintf(stderr, "unknown airspy gain mode: %d\n", server_config->airspy_gain_mode);
      airspy_device_destroy(device);
      return 1;
    }
  }
  ERROR_CHECK(device->lib->airspy_set_freq(device->dev, band_freq), "<3>unable to set freq");
  return device->lib->airspy_start_rx(device->dev, airspy_device_callback, device);
}

void airspy_device_stop_rx(void *plugin) {
  struct airspy_device_t *device = (struct airspy_device_t *)plugin;
  device->lib->airspy_stop_rx(device->dev);
  if (device->dev != NULL) {
    device->lib->airspy_close(device->dev);
  }
}
