#include "dsp_worker.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "api.h"
#include "lpf.h"

static int write_to_file(dsp_worker *config, float complex *filter_output, size_t filter_output_len) {
  size_t n_written;
  if (config->file != NULL) {
    n_written = fwrite(filter_output, sizeof(float complex), filter_output_len, config->file);
  } else if (config->gz != NULL) {
    n_written = gzwrite(config->gz, filter_output, sizeof(float complex) * filter_output_len);
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

static int write_to_socket(int client_socket, float complex *filter_output, size_t filter_output_len) {
  size_t total_len = filter_output_len * sizeof(float complex);
  size_t left = total_len;
  while (left > 0) {
    ssize_t written = write(client_socket, (char *)filter_output + (total_len - left), left);
    if (written < 0) {
      return -1;
    }
    left -= written;
  }
  return 0;
}

static void *callback(void *arg) {
  dsp_worker *worker = (dsp_worker *)arg;
  client_config *config = worker->config;
  fprintf(stdout, "[%d] dsp_worker started\n", config->id);
  uint8_t *input = NULL;
  size_t input_len = 0;
  float complex *filter_output = NULL;
  size_t filter_output_len = 0;
  while (true) {
    take_buffer_for_processing(&input, &input_len, worker->queue);
    // poison pill received
    if (input == NULL) {
      break;
    }
    switch (config->sdr_type) {
      case SDR_TYPE_HACKRF: {
        process_cs8((const int8_t *)input, input_len, &filter_output, &filter_output_len, worker->filter);
        break;
      }
      case SDR_TYPE_RTL: {
        process_cu8(input, input_len, &filter_output, &filter_output_len, worker->filter);
        break;
      }
      case SDR_TYPE_AIRSPY: {
        process_cs16((const int16_t *)input, input_len / sizeof(int16_t), &filter_output, &filter_output_len, worker->filter);
        break;
      }
      default: {
        fprintf(stderr, "<3>unsupported sdr type: %d\n", config->sdr_type);
        break;
      }
    }
    int code;
    if (config->destination == REQUEST_DESTINATION_FILE) {
      code = write_to_file(worker, filter_output, filter_output_len);
    } else if (config->destination == REQUEST_DESTINATION_SOCKET) {
      code = write_to_socket(config->client_socket, filter_output, filter_output_len);
    } else {
      fprintf(stderr, "<3>unknown destination: %d\n", config->destination);
      code = -1;
    }
    complete_buffer_processing(worker->queue);
    if (code != 0) {
      close(config->client_socket);
    }
  }
  return (void *)0;
}

int dsp_worker_start(client_config *config, struct server_config *server_config, dsp_worker **worker) {
  dsp_worker *result = malloc(sizeof(dsp_worker));
  *result = (dsp_worker){0};
  result->config = config;

  // setup taps
  float *taps = NULL;
  size_t len;
  int code = create_low_pass_filter(1.0F, server_config->band_sampling_rate, config->sampling_rate / 2, config->sampling_rate / server_config->lpf_cutoff_rate, &taps, &len);
  if (code != 0) {
    dsp_worker_destroy(result);
    return code;
  }
  // setup xlating frequency filter
  code = create_frequency_xlating_filter(server_config->band_sampling_rate / config->sampling_rate, taps, len, (int64_t)config->center_freq - (int64_t)config->band_freq, server_config->band_sampling_rate, server_config->buffer_size, &result->filter);
  if (code != 0) {
    dsp_worker_destroy(result);
    return code;
  }

  if (server_config->use_gzip) {
    char file_path[4096];
    snprintf(file_path, sizeof(file_path), "%s/%d.cf32.gz", server_config->base_path, config->id);
    result->gz = gzopen(file_path, "wb");
    if (result->gz == NULL) {
      fprintf(stderr, "<3>unable to open gz file for output: %s\n", file_path);
      dsp_worker_destroy(result);
      return -1;
    }
  } else {
    char file_path[4096];
    snprintf(file_path, sizeof(file_path), "%s/%d.cf32", server_config->base_path, config->id);
    result->file = fopen(file_path, "wb");
    if (result->file == NULL) {
      fprintf(stderr, "<3>unable to open file for output: %s\n", file_path);
      dsp_worker_destroy(result);
      return -1;
    }
  }

  // setup queue
  code = create_queue(server_config->buffer_size, server_config->queue_size, &result->queue);
  if (code != 0) {
    dsp_worker_destroy(result);
    return -1;
  }

  // start processing
  pthread_t *dsp_thread = malloc(sizeof(pthread_t));
  if (dsp_thread == NULL) {
    dsp_worker_destroy(result);
    return -ENOMEM;
  }
  code = pthread_create(dsp_thread, NULL, &callback, result);
  if (code != 0) {
    // destroy_node does pthread_join which might fail on undefined pthread_t
    // release memory explicitly
    free(dsp_thread);
    dsp_worker_destroy(result);
    return -1;
  }
  result->dsp_thread = dsp_thread;
  *worker = result;
  return 0;
}

void dsp_worker_destroy(dsp_worker *node) {
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
  if (node->queue != NULL) {
    destroy_queue(node->queue);
  }
  if (node->file != NULL) {
    fclose(node->file);
  }
  if (node->gz != NULL) {
    gzclose(node->gz);
  }
  if (node->filter != NULL) {
    destroy_xlating(node->filter);
  }
  printf("[%d] dsp_worker stopped\n", node->config->id);
  free(node);
}

void dsp_worker_process(uint8_t *buf, uint32_t buf_len, dsp_worker *config) {
  queue_put(buf, buf_len, config->queue);
}