#include "core.h"

#include <complex.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "api.h"
#include "lpf.h"
#include "queue.h"
#include "sdr/airspy_device.h"
#include "sdr/rtlsdr_device.h"
#include "sdr/sdr_device.h"
#include "xlating.h"

struct linked_list_node {
  struct linked_list_node *next;
  struct client_config *config;
  queue *queue;
  xlating *filter;

  pthread_t *dsp_thread;
  FILE *file;
  gzFile gz;
};

struct core_t {
  struct server_config *server_config;
  pthread_mutex_t mutex;
  pthread_cond_t sdr_stopped_condition;
  bool sdr_stop_requested;
  bool sdr_stopped;

  struct linked_list_node *client_configs;
  sdr_device *dev;
  rtlsdr_lib *rtllib;
  airspy_lib *airspy;
};

static int sdr_callback(uint8_t *buf, uint32_t buf_len, void *ctx) {
  core *core = (struct core_t *)ctx;
  int result = 0;
  pthread_mutex_lock(&core->mutex);
  struct linked_list_node *current_node = core->client_configs;
  while (current_node != NULL) {
    // copy to client's buffers and notify
    queue_put(buf, buf_len, current_node->queue);
    current_node = current_node->next;
  }
  if (core->sdr_stop_requested) {
    result = 1;
  }
  pthread_mutex_unlock(&core->mutex);
  return result;
}

int core_create_sdr_device(struct server_config *server_config, core *core) {
  switch (server_config->sdr_type) {
    case SDR_TYPE_RTL: {
      return rtlsdr_device_create(1, server_config, core->rtllib, sdr_callback, core, &core->dev);
    }
    case SDR_TYPE_AIRSPY: {
      return airspy_device_create(1, server_config, core->airspy, sdr_callback, core, &core->dev);
    }
    default: {
      fprintf(stderr, "<3>unsupported sdr type: %d\n", server_config->sdr_type);
      return -1;
    }
  }
}

int core_create_sdr_library(struct server_config *server_config, core *core) {
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

int create_core(struct server_config *server_config, core **result) {
  core *core = malloc(sizeof(struct core_t));
  if (core == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *core = (struct core_t){0};
  core->server_config = server_config;
  core->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  core->sdr_stopped_condition = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
  core->sdr_stopped = true;
  core->sdr_stop_requested = false;
  int code = core_create_sdr_library(server_config, core);
  if (code != 0) {
    destroy_core(core);
    return code;
  }
  *result = core;
  return 0;
}

int write_to_file(struct linked_list_node *config_node, float complex *filter_output, size_t filter_output_len) {
  size_t n_written;
  if (config_node->file != NULL) {
    n_written = fwrite(filter_output, sizeof(float complex), filter_output_len, config_node->file);
  } else if (config_node->gz != NULL) {
    n_written = gzwrite(config_node->gz, filter_output, sizeof(float complex) * filter_output_len);
  } else {
    fprintf(stderr, "<3>unknown file output\n");
    return -1;
  }
  // if disk is full, then terminate the client
  if (n_written < filter_output_len) {
    return -1;
  } else {
    return 0;
  }
}

int write_to_socket(struct linked_list_node *config_node, float complex *filter_output, size_t filter_output_len) {
  size_t total_len = filter_output_len * sizeof(float complex);
  size_t left = total_len;
  while (left > 0) {
    ssize_t written = write(config_node->config->client_socket, (char *)filter_output + (total_len - left), left);
    if (written < 0) {
      return -1;
    }
    left -= written;
  }
  return 0;
}

static void *dsp_worker(void *arg) {
  struct linked_list_node *config_node = (struct linked_list_node *)arg;
  fprintf(stdout, "[%d] dsp_worker is starting\n", config_node->config->id);
  uint8_t *input = NULL;
  size_t input_len = 0;
  float complex *filter_output = NULL;
  size_t filter_output_len = 0;
  while (true) {
    take_buffer_for_processing(&input, &input_len, config_node->queue);
    // poison pill received
    if (input == NULL) {
      break;
    }
    switch (config_node->config->sdr_type) {
      case SDR_TYPE_RTL: {
        process_cu8(input, input_len, &filter_output, &filter_output_len, config_node->filter);
        break;
      }
      case SDR_TYPE_AIRSPY: {
        process_cs16((const int16_t *)input, input_len / sizeof(int16_t), &filter_output, &filter_output_len, config_node->filter);
        break;
      }
      default: {
        fprintf(stderr, "<3>unsupported sdr type: %d\n", config_node->config->sdr_type);
        break;
      }
    }
    int code;
    if (config_node->config->destination == REQUEST_DESTINATION_FILE) {
      code = write_to_file(config_node, filter_output, filter_output_len);
    } else if (config_node->config->destination == REQUEST_DESTINATION_SOCKET) {
      code = write_to_socket(config_node, filter_output, filter_output_len);
    } else {
      fprintf(stderr, "<3>unknown destination: %d\n", config_node->config->destination);
      code = -1;
    }
    complete_buffer_processing(config_node->queue);
    if (code != 0) {
      close(config_node->config->client_socket);
    }
  }

  destroy_queue(config_node->queue);
  printf("[%d] dsp_worker stopped\n", config_node->config->id);
  return (void *)0;
}

int start_sdr(struct client_config *config) {
  core *core = config->core;
  if (core->dev == NULL) {
    int code = core_create_sdr_device(core->server_config, core);
    if (code != 0) {
      fprintf(stderr, "<3>unable to create device\n");
      return 0x04;
    }
  }
  int code = core->dev->start_rx(config->band_freq, core->dev->plugin);
  if (code != 0) {
    fprintf(stderr, "<3>unable to start rx\n");
    return 0x04;
  }
  // reset internal state
  core->sdr_stopped = false;
  core->sdr_stop_requested = false;
  return 0;
}

void stop_sdr(core *core) {
  fprintf(stdout, "sdr is stopping\n");
  // async shutdown request
  core->sdr_stop_requested = true;
  // synchronous wait until all threads shutdown
  core->dev->stop_rx(core->dev->plugin);
  fprintf(stdout, "sdr stopped\n");
  core->sdr_stopped = true;
  pthread_cond_broadcast(&core->sdr_stopped_condition);
}

void destroy_node(struct linked_list_node *node) {
  if (node == NULL) {
    return;
  }
  fprintf(stdout, "[%d] dsp_worker is stopping\n", node->config->id);
  if (node->queue != NULL) {
    interrupt_waiting_the_data(node->queue);
  }
  // wait until thread terminates and only then destroy the node
  if (node->dsp_thread != NULL) {
    pthread_join(*node->dsp_thread, NULL);
    free(node->dsp_thread);
  }
  // cleanup everything only when thread terminates
  if (node->file != NULL) {
    fclose(node->file);
  }
  if (node->gz != NULL) {
    gzclose(node->gz);
  }
  if (node->filter != NULL) {
    destroy_xlating(node->filter);
  }
  free(node);
}

int add_client(struct client_config *config) {
  if (config == NULL) {
    return -1;
  }
  struct linked_list_node *config_node = malloc(sizeof(struct linked_list_node));
  if (config_node == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *config_node = (struct linked_list_node){0};
  config_node->config = config;

  // setup taps
  float *taps = NULL;
  size_t len;
  int code = create_low_pass_filter(1.0F, config->core->server_config->band_sampling_rate, config->sampling_rate / 2, config->sampling_rate / config->core->server_config->lpf_cutoff_rate, &taps, &len);
  if (code != 0) {
    destroy_node(config_node);
    return code;
  }
  // setup xlating frequency filter
  xlating *filter = NULL;
  code = create_frequency_xlating_filter(config->core->server_config->band_sampling_rate / config->sampling_rate, taps, len, (int64_t)config->center_freq - (int64_t)config->band_freq, config->core->server_config->band_sampling_rate, config->core->server_config->buffer_size, &filter);
  if (code != 0) {
    destroy_node(config_node);
    return code;
  }
  config_node->filter = filter;

  if (config->core->server_config->use_gzip) {
    char file_path[4096];
    snprintf(file_path, sizeof(file_path), "%s/%d.cf32.gz", config->core->server_config->base_path, config->id);
    config_node->gz = gzopen(file_path, "wb");
    if (config_node->gz == NULL) {
      fprintf(stderr, "<3>unable to open gz file for output: %s\n", file_path);
      destroy_node(config_node);
      return -1;
    }
  } else {
    char file_path[4096];
    snprintf(file_path, sizeof(file_path), "%s/%d.cf32", config->core->server_config->base_path, config->id);
    config_node->file = fopen(file_path, "wb");
    if (config_node->file == NULL) {
      fprintf(stderr, "<3>unable to open file for output: %s\n", file_path);
      destroy_node(config_node);
      return -1;
    }
  }

  // setup queue
  queue *client_queue = NULL;
  code = create_queue(config->core->server_config->buffer_size, config->core->server_config->queue_size, &client_queue);
  if (code != 0) {
    destroy_node(config_node);
    return -1;
  }
  config_node->queue = client_queue;

  // start processing
  pthread_t *dsp_thread = malloc(sizeof(pthread_t));
  if (dsp_thread == NULL) {
    destroy_node(config_node);
    return -ENOMEM;
  }
  code = pthread_create(dsp_thread, NULL, &dsp_worker, config_node);
  if (code != 0) {
    // destroy_node does pthread_join which might fail on undefined pthread_t
    // release memory explicitly
    free(dsp_thread);
    destroy_node(config_node);
    return -1;
  }
  config_node->dsp_thread = dsp_thread;

  int result;
  pthread_mutex_lock(&config->core->mutex);
  if (config->core->client_configs == NULL) {
    // init rtl-sdr only for the first client
    // wait until sdr fully stop
    // release mutex for stop to perform some cleanups
    while (!config->core->sdr_stopped) {
      pthread_cond_wait(&config->core->sdr_stopped_condition, &config->core->mutex);
    }
    result = start_sdr(config);
    if (result == 0) {
      config->core->client_configs = config_node;
    }
  } else {
    struct linked_list_node *last_config = config->core->client_configs;
    while (last_config->next != NULL) {
      last_config = last_config->next;
    }
    last_config->next = config_node;
    result = 0;
  }
  pthread_mutex_unlock(&config->core->mutex);

  if (result != 0) {
    destroy_node(config_node);
  }
  return result;
}

void remove_client(struct client_config *config) {
  if (config == NULL) {
    return;
  }
  struct linked_list_node *node_to_destroy = NULL;
  bool should_stop_sdr = false;
  pthread_mutex_lock(&config->core->mutex);
  struct linked_list_node *cur_node = config->core->client_configs;
  struct linked_list_node *previous_node = NULL;
  while (cur_node != NULL) {
    if (cur_node->config->id == config->id) {
      if (previous_node == NULL) {
        if (cur_node->next == NULL) {
          // this is the first and the last node
          // shutdown rtlsdr
          should_stop_sdr = true;
        }
        // update pointer to the first node
        config->core->client_configs = cur_node->next;
      } else {
        previous_node->next = cur_node->next;
      }
      node_to_destroy = cur_node;
      break;
    }
    previous_node = cur_node;
    cur_node = cur_node->next;
  }
  if (should_stop_sdr) {
    stop_sdr(config->core);
  }
  pthread_mutex_unlock(&config->core->mutex);

  // stopping the thread can take some time
  // do it outside of synch block
  if (node_to_destroy != NULL) {
    destroy_node(node_to_destroy);
  }
}

void destroy_core(core *core) {
  if (core == NULL) {
    return;
  }
  pthread_mutex_lock(&core->mutex);
  if (!core->sdr_stopped) {
    stop_sdr(core);
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
  struct linked_list_node *cur_node = core->client_configs;
  while (cur_node != NULL) {
    struct linked_list_node *next = cur_node->next;
    // destroy all nodes in synch block, because
    // destory_core should be atomic operation for external code,
    // i.e. destroy all client_configs
    destroy_node(cur_node);
    cur_node = next;
  }
  core->client_configs = NULL;
  pthread_mutex_unlock(&core->mutex);
  free(core);
}
