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
  int code = -1;
  switch (server_config->sdr_type) {
    case SDR_TYPE_RTL: {
      code = rtlsdr_lib_create(&result->rtllib);
      break;
    }
    case SDR_TYPE_AIRSPY: {
      code = airspy_lib_create(&result->airspy);
      break;
    }
    default: {
      fprintf(stderr, "<3>unsupported sdr type: %d\n", server_config->sdr_type);
      code = -1;
      break;
    }
  }
  if (code != 0) {
    sdr_worker_destroy(result);
    return code;
  }
  *worker = result;
  return 0;
}

int sdr_worker_start(client_config *config, sdr_worker *worker) {
  pthread_mutex_lock(&worker->mutex);
  while (!worker->sdr_stopped) {
    pthread_cond_wait(&worker->sdr_stopped_condition, &worker->mutex);
  }
  if (worker->dev == NULL) {
    int code = -1;
    switch (config->sdr_type) {
      case SDR_TYPE_RTL: {
        code = rtlsdr_device_create(1, worker->server_config, worker->rtllib, worker->sdr_callback, worker->ctx, &worker->dev);
        break;
      }
      case SDR_TYPE_AIRSPY: {
        code = airspy_device_create(1, worker->server_config, worker->airspy, worker->sdr_callback, worker->ctx, &worker->dev);
        break;
      }
      default: {
        fprintf(stderr, "<3>unsupported sdr type: %d\n", config->sdr_type);
        code = -1;
        break;
      }
    }
    if (code != 0) {
      fprintf(stderr, "<3>unable to create device\n");
      pthread_mutex_unlock(&worker->mutex);
      return 0x04;
    }
  }
  int code = worker->dev->start_rx(config->band_freq, worker->dev->plugin);
  if (code != 0) {
    fprintf(stderr, "<3>unable to start rx\n");
    pthread_mutex_unlock(&worker->mutex);
    return 0x04;
  }
  // reset internal state
  worker->sdr_stopped = false;
  fprintf(stdout, "sdr started\n");
  pthread_mutex_unlock(&worker->mutex);
  return 0;
}

void sdr_worker_stop(sdr_worker *worker) {
  fprintf(stdout, "sdr is stopping\n");
  pthread_mutex_lock(&worker->mutex);
  // synchronous wait until all threads shutdown
  worker->dev->stop_rx(worker->dev->plugin);
  worker->sdr_stopped = true;
  pthread_cond_broadcast(&worker->sdr_stopped_condition);
  fprintf(stdout, "sdr stopped\n");
  pthread_mutex_unlock(&worker->mutex);
}

void sdr_worker_destroy(sdr_worker *worker) {
  if (worker == NULL) {
    return;
  }
  pthread_mutex_lock(&worker->mutex);
  if (!worker->sdr_stopped) {
    sdr_worker_stop(worker);
  }
  if (worker->dev != NULL) {
    worker->dev->destroy(worker->dev->plugin);
    free(worker->dev);
  }
  if (worker->rtllib != NULL) {
    rtlsdr_lib_destroy(worker->rtllib);
  }
  if (worker->airspy != NULL) {
    airspy_lib_destroy(worker->airspy);
  }
  pthread_mutex_unlock(&worker->mutex);
  free(worker);
}
