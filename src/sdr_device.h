#ifndef SDR_DEVICE_H_
#define SDR_DEVICE_H_

#include "config.h"
#include "dsp_worker.h"

typedef struct sdr_device_t sdr_device;

int sdr_device_create(void (*sdr_callback)(uint8_t *buf, uint32_t buf_len, void *ctx), void *ctx, struct server_config *server_config, sdr_device **result);

int sdr_device_start(client_config *config, sdr_device *sdr_device);

void sdr_device_stop(sdr_device *sdr_device);

void sdr_device_destroy(sdr_device *sdr_device);

#endif /* SDR_DEVICE_H_ */
