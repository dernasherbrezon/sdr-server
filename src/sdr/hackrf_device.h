#ifndef SDR_SERVER_HACKRF_DEVICE_H
#define SDR_SERVER_HACKRF_DEVICE_H

#include "../config.h"
#include "hackrf_lib.h"

int hackrf_device_create(uint32_t id, struct server_config *server_config, hackrf_lib *lib, void (*sdr_callback)(uint8_t *buf, uint32_t len, void *ctx), void *ctx, void **plugin);

void hackrf_device_destroy(void *plugin);

int hackrf_device_start_rx(uint32_t band_freq, void *plugin);

void hackrf_device_stop_rx(void *plugin);

#endif