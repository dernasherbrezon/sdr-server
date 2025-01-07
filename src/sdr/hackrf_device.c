#include "hackrf_device.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define ERROR_CHECK(x, y)                                                              \
  do {                                                                                 \
    int __err_rc = (x);                                                                \
    if (__err_rc != HACKRF_SUCCESS) {                                                  \
      fprintf(stderr, "%s: %s (%d)\n", y, lib->hackrf_error_name(__err_rc), __err_rc); \
      hackrf_device_stop_rx(wrapper);                                                  \
      return __err_rc;                                                                 \
    }                                                                                  \
  } while (0)

typedef struct {
  uint32_t id;
  hackrf_device *dev;
  void (*sdr_callback)(uint8_t *buf, uint32_t len, void *ctx);
  void *ctx;

  struct server_config *server_config;
  hackrf_lib *lib;
} hackrf_wrapper;

int hackrf_device_create(uint32_t id, struct server_config *server_config, hackrf_lib *lib, void (*sdr_callback)(uint8_t *buf, uint32_t len, void *ctx), void *ctx, void **plugin) {
  hackrf_wrapper *wrapper = malloc(sizeof(hackrf_wrapper));
  if (wrapper == NULL) {
    return -ENOMEM;
  }
  *wrapper = (hackrf_wrapper){0};
  wrapper->lib = lib;
  wrapper->sdr_callback = sdr_callback;
  wrapper->ctx = ctx;
  wrapper->server_config = server_config;
  ERROR_CHECK(lib->hackrf_init(), "<3>unable to initialize hackrf library");
  fprintf(stdout, "hackrf device created\n");
  *plugin = wrapper;
  return 0;
}

void hackrf_device_destroy(void *plugin) {
  if (plugin == NULL) {
    return;
  }
  hackrf_wrapper *device = (hackrf_wrapper *)plugin;
  if (device->lib != NULL) {
    device->lib->hackrf_exit();
  }
  free(device);
  fprintf(stdout, "hackrf device destroyed\n");
}

static int hackrf_callback(hackrf_transfer *transfer) {
  hackrf_wrapper *device = (hackrf_wrapper *)transfer->rx_ctx;
  device->sdr_callback(transfer->buffer, transfer->buffer_length, device->ctx);
  return 0;
}

int hackrf_device_start_rx(uint32_t band_freq, void *plugin) {
  hackrf_wrapper *wrapper = (hackrf_wrapper *)plugin;
  hackrf_lib *lib = wrapper->lib;
  if (wrapper->server_config->device_serial != NULL) {
    ERROR_CHECK(lib->hackrf_open_by_serial(wrapper->server_config->device_serial, &wrapper->dev), "<3>unable to open device by serial number");
  } else {
    ERROR_CHECK(lib->hackrf_open(&wrapper->dev), "<3>unable to open device");
  }
  char version[255 + 1];
  ERROR_CHECK(lib->hackrf_version_string_read(wrapper->dev, &version[0], 255), "<3>unable to get version string");
  uint16_t usb_version;
  ERROR_CHECK(lib->hackrf_usb_api_version_read(wrapper->dev, &usb_version), "<3>unable to read usb api version");
  // similar to hackrf_info
  printf("Firmware Version: %s (API:%x.%02x)\n", version, (usb_version >> 8) & 0xFF, usb_version & 0xFF);
  ERROR_CHECK(lib->hackrf_set_freq(wrapper->dev, band_freq), "<3>unable to setup frequency");
  ERROR_CHECK(lib->hackrf_set_sample_rate(wrapper->dev, wrapper->server_config->band_sampling_rate), "<3>unable to setup sample rate");
  // should be the same as sampling rate otherwise clients might get nothing when tuned to >75% range
  ERROR_CHECK(lib->hackrf_set_baseband_filter_bandwidth(wrapper->dev, wrapper->server_config->band_sampling_rate), "<3>unable to setup filter bandwidth");
  ERROR_CHECK(lib->hackrf_set_amp_enable(wrapper->dev, wrapper->server_config->hackrf_amp), "<3>unable to enable amplifier");
  ERROR_CHECK(lib->hackrf_set_lna_gain(wrapper->dev, wrapper->server_config->hackrf_lna_gain), "<3>unable to setup lna gain");
  ERROR_CHECK(lib->hackrf_set_vga_gain(wrapper->dev, wrapper->server_config->hackrf_vga_gain), "<3>unable to setup vga gain");
  ERROR_CHECK(lib->hackrf_set_antenna_enable(wrapper->dev, wrapper->server_config->hackrf_bias_t), "<3>unable to setup bias-t");
  ERROR_CHECK(lib->hackrf_start_rx(wrapper->dev, hackrf_callback, wrapper), "<3>unable to start rx");
  return 0;
}

void hackrf_device_stop_rx(void *plugin) {
  hackrf_wrapper *wrapper = (hackrf_wrapper *)plugin;
  if (wrapper->dev != NULL) {
    wrapper->lib->hackrf_stop_rx(wrapper->dev);
    wrapper->lib->hackrf_close(wrapper->dev);
    wrapper->dev = NULL;
  }
}