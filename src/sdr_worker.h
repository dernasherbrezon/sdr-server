#ifndef SDR_WORKER_H_
#define SDR_WORKER_H_

#include "config.h"
#include "dsp_worker.h"

typedef struct sdr_worker_t sdr_worker;

int sdr_worker_create(void (*sdr_callback)(uint8_t *buf, uint32_t buf_len, void *ctx), void *ctx, struct server_config *server_config, sdr_worker **result);

int sdr_worker_start(client_config *config, sdr_worker *sdr_worker);

void sdr_worker_stop(sdr_worker *sdr_worker);

void sdr_worker_destroy(sdr_worker *sdr_worker);

#endif /* SDR_WORKER_H_ */
