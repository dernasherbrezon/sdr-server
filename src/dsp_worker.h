#ifndef DSP_WORKER_H_
#define DSP_WORKER_H_

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <zlib.h>

#include "config.h"
#include "queue.h"
#include "xlating.h"

typedef struct {
  uint32_t center_freq;
  uint32_t sampling_rate;
  uint32_t band_freq;
  uint8_t destination;
  int client_socket;
  uint32_t id;
  sdr_type_t sdr_type;
  bool is_running;
} client_config;

typedef struct {
  client_config *config;

  void (*xlating_process_cu8)(const uint8_t *, size_t, float complex **, size_t *, xlating *);

  void (*xlating_process_cs8)(const int8_t *, size_t, float complex **, size_t *, xlating *);

  void (*xlating_process_cs16)(const int16_t *, size_t, float complex **, size_t *, xlating *);

  queue *queue;
  xlating *filter;
  pthread_t *dsp_thread;
  FILE *file;
  gzFile gz;
} dsp_worker;

int dsp_worker_start(client_config *config, struct server_config *server_config, dsp_worker **worker);

void dsp_worker_process(uint8_t *buf, uint32_t buf_len, dsp_worker *worker);

void dsp_worker_destroy(dsp_worker *worker);

#endif