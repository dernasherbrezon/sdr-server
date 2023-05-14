#ifndef SDR_SERVER_AIRSPY_DEVICE_H
#define SDR_SERVER_AIRSPY_DEVICE_H

#include "../config.h"
#include "airspy_lib.h"
#include "sdr_device.h"

int airspy_device_create(uint32_t id, struct server_config *server_config, airspy_lib *lib, sdr_device **result);

void airspy_device_destroy(void *plugin);

int airspy_device_start_rx(uint32_t band_freq, void (*sdr_callback)(uint8_t *buf, uint32_t len, void *ctx), void *ctx, void *plugin);

void airspy_device_stop_rx(void *plugin);

#endif //SDR_SERVER_AIRSPY_DEVICE_H
