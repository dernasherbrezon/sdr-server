#include <libconfig.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

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
		gain = config_setting_get_float(setting);
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
	uint32_t band_sampling_rate;
	if (setting == NULL) {
		fprintf(stderr, "missing required configuration: band_sampling_rate\n");
		config_destroy(&libconfig);
		free(result);
		return -1;
	}
	band_sampling_rate = (uint32_t) config_setting_get_int(setting);
	fprintf(stdout, "band sampling rate: %d\n", band_sampling_rate);
	result->band_sampling_rate = band_sampling_rate;

	setting = config_lookup(&libconfig, "bind_address");
	const char *value;
	if (setting == NULL) {
		value = "127.0.0.1";
	} else {
		value = config_setting_get_string(setting);
	}
	char *bind_address;
	size_t length = strlen(value);
	char *str_bind_address = malloc(sizeof(char) * length + 1);
	strncpy(str_bind_address, value, length);
	str_bind_address[length] = '\0';
	bind_address = str_bind_address;
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
	free(config);
}
