#include "config.h"

#include <errno.h>
#include <libconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AIRSPY_BUFFER_SIZE 262144

char *read_and_copy_str(const config_setting_t *setting, const char *default_value) {
  const char *value;
  if (setting == NULL) {
    if (default_value == NULL) {
      return NULL;
    }
    value = default_value;
  } else {
    value = config_setting_get_string(setting);
  }
  char *bind_address;
  size_t length = strlen(value);
  char *str_bind_address = malloc(sizeof(char) * length + 1);
  if (str_bind_address == NULL) {
    return NULL;
  }
  strncpy(str_bind_address, value, length);
  str_bind_address[length] = '\0';
  bind_address = str_bind_address;
  return bind_address;
}

int config_read_int(config_t *libconfig, const char *config_name, int default_value) {
  const config_setting_t *setting = config_lookup(libconfig, config_name);
  int result;
  if (setting == NULL) {
    result = default_value;
  } else {
    result = config_setting_get_int(setting);
  }
  fprintf(stdout, "%s: %d\n", config_name, result);
  return result;
}

float config_read_float(config_t *libconfig, const char *config_name, float default_value) {
  const config_setting_t *setting = config_lookup(libconfig, config_name);
  float result;
  if (setting == NULL) {
    result = default_value;
  } else {
    result = (float) config_setting_get_float(setting);
  }
  fprintf(stdout, "%s: %f\n", config_name, result);
  return result;
}

bool config_read_bool(config_t *libconfig, const char *config_name, bool default_value) {
  const config_setting_t *setting = config_lookup(libconfig, config_name);
  bool result;
  if (setting == NULL) {
    result = default_value;
  } else {
    result = config_setting_get_bool(setting);
  }
  fprintf(stdout, "%s: %d\n", config_name, result);
  return result;
}

uint32_t config_read_uint32_t(config_t *libconfig, const char *config_name, uint32_t default_value) {
  const config_setting_t *setting = config_lookup(libconfig, config_name);
  uint32_t result;
  if (setting == NULL) {
    result = default_value;
  } else {
    result = (uint32_t) config_setting_get_int(setting);
  }
  fprintf(stdout, "%s: %d\n", config_name, result);
  return result;
}

static cpu_optimization config_parse_cpu_optimization(const char *str) {
  if (strcmp(str, "NATIVE_CF32") == 0) return NATIVE_CF32;
  if (strcmp(str, "OPTIMIZED_CF32") == 0) return OPTIMIZED_CF32;
  return -1;
}

static const char *config_format_cpu_optimization(cpu_optimization value) {
  switch (value) {
    case NATIVE_CF32:
      return "NATIVE_CF32";
    case OPTIMIZED_CF32:
      return "OPTIMIZED_CF32";
    default:
      return "UNKNOWN";
  }
}

int create_server_config(struct server_config **config, const char *path) {
  fprintf(stdout, "loading configuration from: %s\n", path);
  struct server_config *result = malloc(sizeof(struct server_config));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct server_config) {0};

  config_t libconfig;
  config_init(&libconfig);

  int code = config_read_file(&libconfig, path);
  if (code == CONFIG_FALSE) {
    fprintf(stderr, "<3>unable to read configuration: %s\n", config_error_text(&libconfig));
    config_destroy(&libconfig);
    free(result);
    return -1;
  }

  result->sdr_type = config_read_int(&libconfig, "sdr_type", 0);
  result->bias_t = config_read_int(&libconfig, "bias_t", 0);
  result->gain_mode = config_read_int(&libconfig, "gain_mode", 0);
  result->gain = (int) (config_read_float(&libconfig, "gain", 0) * 10);
  result->ppm = config_read_int(&libconfig, "ppm", 0);

  result->airspy_gain_mode = config_read_int(&libconfig, "airspy_gain_mode", AIRSPY_GAIN_MANUAL);
  result->airspy_vga_gain = config_read_int(&libconfig, "airspy_vga_gain", 5);
  if (result->airspy_vga_gain > 15 || result->airspy_vga_gain < 0) {
    fprintf(stderr, "<3>invalid airspy_vga_gain configuration\n");
    config_destroy(&libconfig);
    free(result);
    return -1;
  }
  result->airspy_mixer_gain = config_read_int(&libconfig, "airspy_mixer_gain", 0);
  if (result->airspy_mixer_gain > 15 || result->airspy_mixer_gain < 0) {
    fprintf(stderr, "<3>invalid airspy_mixer_gain configuration\n");
    config_destroy(&libconfig);
    free(result);
    return -1;
  }
  result->airspy_lna_gain = config_read_int(&libconfig, "airspy_lna_gain", 1);
  if (result->airspy_lna_gain > 14 || result->airspy_lna_gain < 0) {
    fprintf(stderr, "<3>invalid airspy_lna_gain configuration\n");
    config_destroy(&libconfig);
    free(result);
    return -1;
  }
  result->airspy_linearity_gain = config_read_int(&libconfig, "airspy_linearity_gain", 0);
  if (result->airspy_linearity_gain > 21 || result->airspy_linearity_gain < 0) {
    fprintf(stderr, "<3>invalid airspy_linearity_gain configuration\n");
    config_destroy(&libconfig);
    free(result);
    return -1;
  }
  result->airspy_sensitivity_gain = config_read_int(&libconfig, "airspy_sensitivity_gain", 0);
  if (result->airspy_sensitivity_gain > 21 || result->airspy_sensitivity_gain < 0) {
    fprintf(stderr, "<3>invalid airspy_sensitivity_gain configuration\n");
    config_destroy(&libconfig);
    free(result);
    return -1;
  }

  result->hackrf_bias_t = (uint8_t) config_read_int(&libconfig, "hackrf_bias_t", 0);
  result->hackrf_amp = config_read_int(&libconfig, "hackrf_amp", 0);
  if (result->hackrf_amp > 1) {
    fprintf(stderr, "<3>hackrf_amp is either turned on (1) or off (0)\n");
    config_destroy(&libconfig);
    free(result);
    return -1;
  }
  result->hackrf_lna_gain = config_read_int(&libconfig, "hackrf_lna_gain", 16);
  if (result->hackrf_lna_gain < 0 || result->hackrf_lna_gain > 40) {
    fprintf(stderr, "<3>invalid hackrf_lna_gain configuration\n");
    config_destroy(&libconfig);
    free(result);
    return -1;
  }
  result->hackrf_vga_gain = config_read_int(&libconfig, "hackrf_vga_gain", 16);
  if (result->hackrf_vga_gain < 0 || result->hackrf_vga_gain > 62) {
    fprintf(stderr, "<3>invalid hackrf_vga_gain configuration\n");
    config_destroy(&libconfig);
    free(result);
    return -1;
  }

  result->queue_size = config_read_int(&libconfig, "queue_size", 64);
  if (result->queue_size <= 0) {
    fprintf(stderr, "<3>queue size should be positive: %d\n", result->queue_size);
    config_destroy(&libconfig);
    free(result);
    return -1;
  }

  const config_setting_t *setting = config_lookup(&libconfig, "band_sampling_rate");
  if (setting == NULL) {
    fprintf(stderr, "<3>missing required configuration: band_sampling_rate\n");
    config_destroy(&libconfig);
    free(result);
    return -1;
  }
  uint32_t band_sampling_rate = (uint32_t) config_setting_get_int(setting);
  fprintf(stdout, "band sampling rate: %d\n", band_sampling_rate);
  result->band_sampling_rate = band_sampling_rate;

  result->device_index = config_read_int(&libconfig, "device_index", 0);
  result->device_serial = read_and_copy_str(config_lookup(&libconfig, "device_serial"), NULL);
  if (result->device_serial != NULL) {
    fprintf(stdout, "device_serial: %s\n", result->device_serial);
  }

  result->buffer_size = config_read_uint32_t(&libconfig, "buffer_size", 262144);
  if (result->sdr_type == SDR_TYPE_AIRSPY && result->buffer_size != AIRSPY_BUFFER_SIZE) {
    result->buffer_size = AIRSPY_BUFFER_SIZE;
    fprintf(stdout, "force airspy buffer_size to: %d\n", result->buffer_size);
  }
  result->lpf_cutoff_rate = config_read_int(&libconfig, "lpf_cutoff_rate", 5);

  setting = config_lookup(&libconfig, "bind_address");
  char *bind_address = read_and_copy_str(setting, "127.0.0.1");
  if (bind_address == NULL) {
    config_destroy(&libconfig);
    free(result);
    return -ENOMEM;
  }
  result->bind_address = bind_address;
  result->port = config_read_int(&libconfig, "port", 8090);
  fprintf(stdout, "start listening on %s:%d\n", result->bind_address, result->port);

  int read_timeout_seconds = config_read_int(&libconfig, "read_timeout_seconds", 5);
  if (read_timeout_seconds <= 0) {
    config_destroy(&libconfig);
    destroy_server_config(result);
    fprintf(stderr, "<3>read timeout should be positive: %d\n", read_timeout_seconds);
    return -1;
  }
  result->read_timeout_seconds = read_timeout_seconds;

  const char *default_folder = getenv("TMPDIR");
  if (default_folder == NULL) {
    default_folder = "/tmp";
  }

  setting = config_lookup(&libconfig, "base_path");
  char *base_path = read_and_copy_str(setting, default_folder);
  if (base_path == NULL) {
    config_destroy(&libconfig);
    destroy_server_config(result);
    return -ENOMEM;
  }
  result->base_path = base_path;
  fprintf(stdout, "base path for storing results: %s\n", result->base_path);

  result->use_gzip = config_read_bool(&libconfig, "use_gzip", true);

  setting = config_lookup(&libconfig, "cpu_optimization");
  if (setting != NULL) {
    const char *cpu_optimization_str = config_setting_get_string(setting);
    result->optimization = config_parse_cpu_optimization(cpu_optimization_str);
    if (result->optimization == -1) {
      fprintf(stderr, "<3>invalid cpu_optimization: %s\n", cpu_optimization_str);
      config_destroy(&libconfig);
      destroy_server_config(result);
      return -1;
    }
  } else {
    result->optimization = NATIVE_CF32;
  }
  fprintf(stdout, "cpu_optimization: %s\n", config_format_cpu_optimization(result->optimization));

  config_destroy(&libconfig);

  *config = result;
  return 0;
}

void destroy_server_config(struct server_config *config) {
  if (config == NULL) {
    return;
  }
  if (config->bind_address != NULL) {
    free(config->bind_address);
  }
  if (config->base_path != NULL) {
    free(config->base_path);
  }
  if (config->device_serial != NULL) {
    free(config->device_serial);
  }
  free(config);
}
