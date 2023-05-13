#ifndef SDR_SERVER_AIRSPY_LIB_H
#define SDR_SERVER_AIRSPY_LIB_H

#include <airspy.h>

typedef struct airspy_lib_t airspy_lib;

struct airspy_lib_t {
  void *handle;

  int (*airspy_open)(struct airspy_device **device);

  int (*airspy_set_sample_type)(struct airspy_device *device, enum airspy_sample_type sample_type);

  int (*airspy_close)(struct airspy_device *device);

  int (*airspy_get_samplerates)(struct airspy_device *device, uint32_t *buffer, const uint32_t len);

  int (*airspy_set_samplerate)(struct airspy_device *device, uint32_t samplerate);

  int (*airspy_set_packing)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_rf_bias)(struct airspy_device *dev, uint8_t value);

  int (*airspy_set_lna_gain)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_vga_gain)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_mixer_gain)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_linearity_gain)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_sensitivity_gain)(struct airspy_device *device, uint8_t value);

  int (*airspy_start_rx)(struct airspy_device *device, airspy_sample_block_cb_fn callback, void *rx_ctx);

  int (*airspy_set_freq)(struct airspy_device *device, const uint32_t freq_hz);

  int (*airspy_stop_rx)(struct airspy_device *device);

  void (*airspy_lib_version)(airspy_lib_version_t *lib_version);
};

int airspy_lib_create(airspy_lib **lib);

void airspy_lib_destroy(airspy_lib *lib);

#endif //SDR_SERVER_AIRSPY_LIB_H
