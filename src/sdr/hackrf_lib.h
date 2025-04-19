#ifndef SDR_SERVER_HACKRF_LIB_H
#define SDR_SERVER_HACKRF_LIB_H

#include <hackrf.h>

typedef struct hackrf_lib_t hackrf_lib;

struct hackrf_lib_t {
  void* handle;

  int (*hackrf_init)();
  const char* (*hackrf_error_name)(enum hackrf_error errcode);
  int (*hackrf_version_string_read)(hackrf_device* device, char* version, uint8_t length);
  int (*hackrf_usb_api_version_read)(hackrf_device* device, uint16_t* version);
  int (*hackrf_set_vga_gain)(hackrf_device* device, uint32_t value);
  int (*hackrf_set_lna_gain)(hackrf_device* device, uint32_t value);
  int (*hackrf_set_amp_enable)(hackrf_device* device, const uint8_t value);
  int (*hackrf_set_freq)(hackrf_device* device, const uint64_t freq_hz);
  int (*hackrf_set_baseband_filter_bandwidth)(hackrf_device* device, const uint32_t bandwidth_hz);
  int (*hackrf_set_sample_rate)(hackrf_device* device, const double freq_hz);
  int (*hackrf_start_rx)(hackrf_device* device, hackrf_sample_block_cb_fn callback, void* rx_ctx);
  int (*hackrf_stop_rx)(hackrf_device* device);
  int (*hackrf_close)(hackrf_device* device);
  int (*hackrf_exit)();
  int (*hackrf_open)(hackrf_device** device);
  int (*hackrf_open_by_serial)(const char* const desired_serial_number, hackrf_device** device);
  int (*hackrf_set_antenna_enable)(hackrf_device* device, const uint8_t value);
};

int hackrf_lib_create(hackrf_lib** lib);

void hackrf_lib_destroy(hackrf_lib* lib);

#endif