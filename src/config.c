#include <libconfig.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

char* read_and_copy_str(const config_setting_t *setting, const char *default_value) {
	const char *value;
	if (setting == NULL) {
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

int create_server_config(struct server_config **config, const char *path) {
	fprintf(stdout, "loading configuration from: %s\n", path);
	struct server_config *result = malloc(sizeof(struct server_config));
	if (result == NULL) {
		return -ENOMEM;
	}

	config_t libconfig;
	config_init(&libconfig);

	int code = config_read_file(&libconfig, path);
	if (code == CONFIG_FALSE) {
		fprintf(stderr, "unable to read configuration: %s\n", config_error_text(&libconfig));
		config_destroy(&libconfig);
		free(result);
		return -1;
	}

	const config_setting_t *setting = config_lookup(&libconfig, "bias_t");
	int bias_t;
	if (setting == NULL) {
		bias_t = 0;
	} else {
		bias_t = config_setting_get_int(setting);
	}
	fprintf(stdout, "bias-t: %d\n", bias_t);
	result->bias_t = bias_t;

	setting = config_lookup(&libconfig, "gain_mode");
	int gain_mode;
	if (setting == NULL) {
		gain_mode = 0;
	} else {
		gain_mode = config_setting_get_int(setting);
	}
	fprintf(stdout, "gain mode: %d\n", gain_mode);
	result->gain_mode = gain_mode;

	setting = config_lookup(&libconfig, "gain");
	float gain;
	if (setting == NULL) {
		gain = 0;
	} else {
		gain = (float) config_setting_get_float(setting);
	}
	fprintf(stdout, "gain: %f\n", gain);
	result->gain = (int) (gain * 10);

	setting = config_lookup(&libconfig, "ppm");
	int ppm;
	if (setting == NULL) {
		ppm = 0;
	} else {
		ppm = config_setting_get_int(setting);
	}
	fprintf(stdout, "ppm: %d\n", ppm);
	result->ppm = ppm;

	setting = config_lookup(&libconfig, "band_sampling_rate");
	if (setting == NULL) {
		fprintf(stderr, "missing required configuration: band_sampling_rate\n");
		config_destroy(&libconfig);
		free(result);
		return -1;
	}
	uint32_t band_sampling_rate = (uint32_t) config_setting_get_int(setting);
	fprintf(stdout, "band sampling rate: %d\n", band_sampling_rate);
	result->band_sampling_rate = band_sampling_rate;

	setting = config_lookup(&libconfig, "buffer_size");
	uint32_t buffer_size;
	if (setting == NULL) {
		buffer_size = 262144;
	} else {
		buffer_size = (uint32_t) config_setting_get_int(setting);
	}
	fprintf(stdout, "buffer size: %d\n", buffer_size);
	result->buffer_size = buffer_size;

	setting = config_lookup(&libconfig, "bind_address");
	char *bind_address = read_and_copy_str(setting, "127.0.0.1");
	if (bind_address == NULL) {
		config_destroy(&libconfig);
		free(result);
		return -ENOMEM;
	}
	result->bind_address = bind_address;
	setting = config_lookup(&libconfig, "port");
	int port;
	if (setting == NULL) {
		port = 8081;
	} else {
		port = config_setting_get_int(setting);
	}
	result->port = port;
	fprintf(stdout, "start listening on %s:%d\n", result->bind_address, result->port);

	setting = config_lookup(&libconfig, "read_timeout_seconds");
	int read_timeout_seconds;
	if (setting == NULL) {
		read_timeout_seconds = 5;
	} else {
		read_timeout_seconds = config_setting_get_int(setting);
		if (read_timeout_seconds <= 0) {
			config_destroy(&libconfig);
			free(result);
			fprintf(stderr, "read timeout should be positive: %d\n", read_timeout_seconds);
			return -1;
		}
	}
	result->read_timeout_seconds = read_timeout_seconds;
	fprintf(stdout, "read timeout %ds\n", result->read_timeout_seconds);

	const char *default_folder = getenv("TMPDIR");
	if (default_folder == NULL) {
		default_folder = "/tmp";
	}

	setting = config_lookup(&libconfig, "base_path");
	char *base_path = read_and_copy_str(setting, default_folder);
	if (base_path == NULL) {
		config_destroy(&libconfig);
		free(result);
		return -ENOMEM;
	}
	result->base_path = base_path;
	fprintf(stdout, "base path for storing results: %s\n", result->base_path);

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
	free(config);
}
