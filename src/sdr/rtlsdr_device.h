#ifndef SDR_SERVER_RTLSDR_DEVICE_H
#define SDR_SERVER_RTLSDR_DEVICE_H

#include "rtlsdr_lib.h"
#include "sdr_device.h"
#include "../config.h"

int rtlsdr_device_create(uint32_t id, struct server_config *server_config, rtlsdr_lib *lib, int (*sdr_callback)(uint8_t *buf, uint32_t len, void *ctx), void *ctx, sdr_device **result);

void rtlsdr_device_destroy(void *plugin);

int rtlsdr_device_start_rx(uint32_t band_freq, void *plugin);

void rtlsdr_device_stop_rx(void *plugin);

#endif //SDR_SERVER_RTLSDR_DEVICE_H
