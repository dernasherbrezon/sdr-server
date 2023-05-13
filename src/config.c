#include <libconfig.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

char *read_and_copy_str(const config_setting_t *setting, const char *default_value) {
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

    result->buffer_size = config_read_uint32_t(&libconfig, "buffer_size", 262144);
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
        free(result);
        return -ENOMEM;
    }
    result->base_path = base_path;
    fprintf(stdout, "base path for storing results: %s\n", result->base_path);

    result->use_gzip = config_read_bool(&libconfig, "use_gzip", true);

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
