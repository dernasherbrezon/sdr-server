#include "sdr_worker.h"

#include <complex.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "sdr/airspy_device.h"
#include "sdr/rtlsdr_device.h"
#include "sdr/sdr_device.h"

struct sdr_worker_t {
  struct server_config *server_config;
  pthread_mutex_t mutex;
  pthread_cond_t sdr_stopped_condition;
  bool sdr_stopped;

  sdr_device *dev;
  rtlsdr_lib *rtllib;
  airspy_lib *airspy;
  void (*sdr_callback)(uint8_t *buf, uint32_t buf_len, void *ctx);
  void *ctx;
};

int core_create_sdr_device(struct server_config *server_config, sdr_worker *core) {
  switch (server_config->sdr_type) {
    case SDR_TYPE_RTL: {
      return rtlsdr_device_create(1, server_config, core->rtllib, core->sdr_callback, core->ctx, &core->dev);
    }
    case SDR_TYPE_AIRSPY: {
      return airspy_device_create(1, server_config, core->airspy, core->sdr_callback, core->ctx, &core->dev);
    }
    default: {
      fprintf(stderr, "<3>unsupported sdr type: %d\n", server_config->sdr_type);
      return -1;
    }
  }
}

int core_create_sdr_library(struct server_config *server_config, sdr_worker *core) {
  switch (server_config->sdr_type) {
    case SDR_TYPE_RTL: {
      return rtlsdr_lib_create(&core->rtllib);
    }
    case SDR_TYPE_AIRSPY: {
      return airspy_lib_create(&core->airspy);
    }
    default: {
      fprintf(stderr, "<3>unsupported sdr type: %d\n", server_config->sdr_type);
      return -1;
    }
  }
}

int sdr_worker_create(void (*sdr_callback)(uint8_t *buf, uint32_t buf_len, void *ctx), void *ctx, struct server_config *server_config, sdr_worker **worker) {
  struct sdr_worker_t *result = malloc(sizeof(struct sdr_worker_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct sdr_worker_t){0};
  result->server_config = server_config;
  result->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  result->sdr_stopped_condition = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
  result->sdr_stopped = true;
  result->sdr_callback = sdr_callback;
  result->ctx = ctx;
  int code = core_create_sdr_library(server_config, result);
  if (code != 0) {
    sdr_worker_destroy(result);
    return code;
  }
  *worker = result;
  return 0;
}

int sdr_worker_start(client_config *config, sdr_worker *sdr_worker) {
  pthread_mutex_lock(&sdr_worker->mutex);
  while (!sdr_worker->sdr_stopped) {
    pthread_cond_wait(&sdr_worker->sdr_stopped_condition, &sdr_worker->mutex);
  }
  if (sdr_worker->dev == NULL) {
    int code = core_create_sdr_device(sdr_worker->server_config, sdr_worker);
    if (code != 0) {
      fprintf(stderr, "<3>unable to create device\n");
      pthread_mutex_unlock(&sdr_worker->mutex);
      return 0x04;
    }
  }
  int code = sdr_worker->dev->start_rx(config->band_freq, sdr_worker->dev->plugin);
  if (code != 0) {
    fprintf(stderr, "<3>unable to start rx\n");
    pthread_mutex_unlock(&sdr_worker->mutex);
    return 0x04;
  }
  // reset internal state
  sdr_worker->sdr_stopped = false;
  fprintf(stdout, "sdr started\n");
  pthread_mutex_unlock(&sdr_worker->mutex);
  return 0;
}

void sdr_worker_stop(sdr_worker *sdr_worker) {
  fprintf(stdout, "sdr is stopping\n");
  pthread_mutex_lock(&sdr_worker->mutex);
  // synchronous wait until all threads shutdown
  sdr_worker->dev->stop_rx(sdr_worker->dev->plugin);
  sdr_worker->sdr_stopped = true;
  pthread_cond_broadcast(&sdr_worker->sdr_stopped_condition);
  fprintf(stdout, "sdr stopped\n");
  pthread_mutex_unlock(&sdr_worker->mutex);
}

void sdr_worker_destroy(sdr_worker *core) {
  if (core == NULL) {
    return;
  }
  pthread_mutex_lock(&core->mutex);
  if (!core->sdr_stopped) {
    sdr_worker_stop(core);
  }
  if (core->dev != NULL) {
    core->dev->destroy(core->dev->plugin);
    free(core->dev);
  }
  if (core->rtllib != NULL) {
    rtlsdr_lib_destroy(core->rtllib);
  }
  if (core->airspy != NULL) {
    airspy_lib_destroy(core->airspy);
  }
  pthread_mutex_unlock(&core->mutex);
  free(core);
}
