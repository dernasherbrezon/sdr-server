#include <stdio.h>
#include <rtl-sdr.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <complex.h>
#include <unistd.h>
#include <zlib.h>

#include "lpf.h"
#include "xlating.h"
#include "queue.h"
#include "core.h"
#include "api.h"

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
    pthread_cond_t rtl_thread_stopped_condition;

    struct linked_list_node *client_configs;
    rtlsdr_dev_t *dev;
    pthread_t *rtlsdr_worker_thread;
    uint8_t *buffer;
};

int find_nearest_gain(rtlsdr_dev_t *dev, int target_gain, int *nearest) {
    int count = rtlsdr_get_tuner_gains(dev, NULL);
    if (count <= 0) {
        return -1;
    }
    int *gains = malloc(sizeof(int) * count);
    if (gains == NULL) {
        return -ENOMEM;
    }
    int code = rtlsdr_get_tuner_gains(dev, gains);
    if (code <= 0) {
        free(gains);
        return -1;
    }
    *nearest = gains[0];
    for (int i = 0; i < count; i++) {
        int err1 = abs(target_gain - *nearest);
        int err2 = abs(target_gain - gains[i]);
        if (err2 < err1) {
            *nearest = gains[i];
        }
    }
    free(gains);
    return 0;
}

int create_core(struct server_config *server_config, core **result) {
    core *core = malloc(sizeof(struct core_t));
    if (core == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *core = (struct core_t) {0};

    core->server_config = server_config;
    uint8_t *buffer = malloc(server_config->buffer_size * sizeof(uint8_t));
    if (buffer == NULL) {
        destroy_core(core);
        return -ENOMEM;
    }
    memset(buffer, 0, server_config->buffer_size);
    core->buffer = buffer;
    core->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    core->rtl_thread_stopped_condition = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
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
        int written = write(config_node->config->client_socket, (char *) filter_output + (total_len - left), left);
        if (written < 0) {
            return -1;
        }
        left -= written;
    }
    return 0;
}

static void *dsp_worker(void *arg) {
    struct linked_list_node *config_node = (struct linked_list_node *) arg;
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
        process(input, input_len, &filter_output, &filter_output_len, config_node->filter);
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
            // this would trigger client disconnect
            // I could use "break" here, but "continue" is a bit better:
            //   - single route for abnormal termination (i.e. disk space issue) and normal (i.e. client disconnected)
            //   - all shutdown sequences have: stop tcp thread, then dsp thread, then rtlsdr thread
            //   - processing the queue and writing to the already full disk is OK (I hope)
            //   - calling "close" socket multiple times is OK (I hope)
            close(config_node->config->client_socket);
        }

    }
    destroy_queue(config_node->queue);
    printf("[%d] dsp_worker stopped\n", config_node->config->id);
    return (void *) 0;
}

static void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx) {
    core *core = (struct core_t *) ctx;
    pthread_mutex_lock(&core->mutex);
    struct linked_list_node *current_node = core->client_configs;
    while (current_node != NULL) {
        // copy to client's buffers and notify
        queue_put(buf, len, current_node->queue);
        current_node = current_node->next;
    }
    pthread_mutex_unlock(&core->mutex);
}

static void *rtlsdr_worker(void *arg) {
    core *core = (struct core_t *) arg;
    uint32_t buffer_size = core->server_config->buffer_size;
    rtlsdr_read_async(core->dev, rtlsdr_callback, core, 0, buffer_size);
    pthread_mutex_lock(&core->mutex);
    // run close on the same thread
    // otherwise it might produce race condition somewhere in libusb code
    rtlsdr_close(core->dev);
    core->dev = NULL;
    pthread_cond_broadcast(&core->rtl_thread_stopped_condition);
    pthread_mutex_unlock(&core->mutex);
    printf("rtl-sdr stopped\n");
    return (void *) 0;
}

int start_rtlsdr(struct client_config *config) {
    core *core = config->core;
    fprintf(stdout, "rtl-sdr is starting\n");
    rtlsdr_dev_t *dev = NULL;
    rtlsdr_open(&dev, 0);
    if (dev == NULL) {
        fprintf(stderr, "<3>unable to open rtl-sdr device\n");
        return 0x04;
    }

    int code = rtlsdr_set_sample_rate(dev, core->server_config->band_sampling_rate);
    if (code < 0) {
        fprintf(stderr, "<3>unable to set sampling rate: %d\n", code);
    }
    code = rtlsdr_set_center_freq(dev, config->band_freq);
    if (code < 0) {
        fprintf(stderr, "<3>unable to set center freq: %d\n", code);
    }
    code = rtlsdr_set_tuner_gain_mode(dev, core->server_config->gain_mode);
    if (code < 0) {
        fprintf(stderr, "<3>unable to set gain mode: %d\n", code);
    }
    if (core->server_config->gain_mode == 1) {
        int nearest_gain = 0;
        code = find_nearest_gain(dev, core->server_config->gain, &nearest_gain);
        if (code < 0) {
            fprintf(stderr, "<3>unable to find nearest gain for: %d\n", core->server_config->gain);
        } else {
            if (nearest_gain != core->server_config->gain) {
                fprintf(stdout, "the actual nearest supported gain is: %f\n", (float) nearest_gain / 10);
            }
            code = rtlsdr_set_tuner_gain(dev, nearest_gain);
            if (code < 0) {
                fprintf(stderr, "<3>unable to set tuner gain: %d\n", code);
            }
        }
    }
    code = rtlsdr_set_bias_tee(dev, core->server_config->bias_t);
    if (code < 0) {
        fprintf(stderr, "<3>unable to set bias_t: %d\n", code);
    }
    code = rtlsdr_reset_buffer(dev);
    if (code < 0) {
        fprintf(stderr, "<3>unable to reset buffer: %d\n", code);
    }
    core->dev = dev;

    pthread_t *rtlsdr_worker_thread = malloc(sizeof(pthread_t));
    if (rtlsdr_worker_thread == NULL) {
        return 0x04;
    }
    code = pthread_create(rtlsdr_worker_thread, NULL, &rtlsdr_worker, core);
    if (code != 0) {
        free(rtlsdr_worker_thread);
        return 0x04;
    }
    core->rtlsdr_worker_thread = rtlsdr_worker_thread;
    return 0x0;
}

void stop_rtlsdr(core *core) {
    fprintf(stdout, "rtl-sdr is stopping\n");

    rtlsdr_cancel_async(core->dev);

    // release the mutex here for rtlsdr thread to send last updates
    // and cleans up
    while (core->dev != NULL) {
        pthread_cond_wait(&core->rtl_thread_stopped_condition, &core->mutex);
    }

    // additionally wait until rtlsdr thread terminates by OS
    if (core->rtlsdr_worker_thread != NULL) {
        pthread_join(*core->rtlsdr_worker_thread, NULL);
        free(core->rtlsdr_worker_thread);
    }
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
    *config_node = (struct linked_list_node) {0};
    config_node->config = config;

    // setup taps
    float *taps = NULL;
    size_t len;
    int code = create_low_pass_filter(1.0, config->core->server_config->band_sampling_rate, config->sampling_rate / 2, config->sampling_rate / config->core->server_config->lpf_cutoff_rate, &taps, &len);
    if (code != 0) {
        destroy_node(config_node);
        return code;
    }
    // setup xlating frequency filter
    xlating *filter = NULL;
    code = create_frequency_xlating_filter(config->core->server_config->band_sampling_rate / config->sampling_rate, taps, len, (int64_t) config->center_freq - (int64_t) config->band_freq, config->core->server_config->band_sampling_rate, config->core->server_config->buffer_size, &filter);
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
        result = start_rtlsdr(config);
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
    bool should_stop_rtlsdr = false;
    pthread_mutex_lock(&config->core->mutex);
    struct linked_list_node *cur_node = config->core->client_configs;
    struct linked_list_node *previous_node = NULL;
    while (cur_node != NULL) {
        if (cur_node->config->id == config->id) {
            if (previous_node == NULL) {
                if (cur_node->next == NULL) {
                    // this is the first and the last node
                    // shutdown rtlsdr
                    should_stop_rtlsdr = true;
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
    if (should_stop_rtlsdr) {
        stop_rtlsdr(config->core);
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
    if (core->dev != NULL) {
        stop_rtlsdr(core);
    }
    if (core->buffer != NULL) {
        free(core->buffer);
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

