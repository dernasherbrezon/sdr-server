#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SDR_TYPE_RTL = 0,
  SDR_TYPE_AIRSPY = 1,
  SDR_TYPE_HACKRF = 2
} sdr_type_t;

typedef enum {
  AIRSPY_GAIN_AUTO = 0,
  AIRSPY_GAIN_SENSITIVITY = 1,
  AIRSPY_GAIN_LINEARITY = 2,
  AIRSPY_GAIN_MANUAL = 3
} airspy_gain_mode_t;

typedef enum {
  NATIVE_CF32,
  OPTIMIZED_CF32
} cpu_optimization;

struct server_config {
  // socket settings
  char *bind_address;
  int port;
  int read_timeout_seconds;
  char *device_serial;
  cpu_optimization optimization;

  sdr_type_t sdr_type;

  // rtl-sdr settings
  int device_index;
  int gain_mode;
  int gain;
  int ppm;
  int bias_t;
  uint32_t buffer_size;
  // 4GHz max
  uint32_t band_sampling_rate;
  int queue_size;
  int lpf_cutoff_rate;

  // airspy settings
  airspy_gain_mode_t airspy_gain_mode;
  int airspy_vga_gain;
  int airspy_mixer_gain;
  int airspy_lna_gain;
  int airspy_linearity_gain;
  int airspy_sensitivity_gain;

  // hackrf settings
  uint8_t hackrf_bias_t;
  int hackrf_amp;
  int hackrf_lna_gain;
  int hackrf_vga_gain;

  // output settings
  char *base_path;
  bool use_gzip;
};

int create_server_config(struct server_config **config, const char *path);

void destroy_server_config(struct server_config *config);

#endif /* CONFIG_H_ */
