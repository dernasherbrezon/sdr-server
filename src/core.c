#include <stdio.h>
#include <rtl-sdr.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <complex.h>

#include "queue.h"
#include "core.h"

struct linked_list_node {
	struct linked_list_node *next;
	struct client_config *config;
	queue *queue;
	float *taps;
	xlating *filter;
	volatile sig_atomic_t is_running;
	pthread_t dsp_thread;
};

struct core_t {
	struct server_config *server_config;
	pthread_mutex_t mutex;

	struct linked_list_node *client_configs;
	rtlsdr_dev_t *dev;
	volatile sig_atomic_t is_rtlsdr_running;
	pthread_t rtlsdr_worker_thread;
	uint8_t *buffer;
};

int create_core(struct server_config *server_config, core **result) {
	core *core = malloc(sizeof(struct core_t));
	if (core == NULL) {
		return -ENOMEM;
	}
	// init all fields with 0 so that destroy_* method would work
	*core = (struct core_t ) { 0 };

	core->server_config = server_config;
	uint8_t *buffer = malloc(server_config->buffer_size * sizeof(uint8_t));
	if (buffer == NULL) {
		destroy_core(core);
		return -ENOMEM;
	}
	core->buffer = buffer;
	core->mutex = (pthread_mutex_t )PTHREAD_MUTEX_INITIALIZER;
	*result = core;
	return 0;
}

// FIXME replace double with floats everywhere
static void* dsp_worker(void *arg) {
	struct linked_list_node *config_node = (struct linked_list_node*) arg;
	struct client_config *config = config_node->config;

	FILE *file;
	//FIXME
	file = fopen("/tmp/file.raw", "wb");

	uint8_t *input = NULL;
	int input_len = 0;
	float complex *filter_output;
	size_t filter_output_len = 0;
	while (config_node->is_running) {
		take_buffer_for_processing(&input, &input_len, config_node->queue);
		// poison pill received
		if (input == NULL) {
			break;
		}
		process(input, input_len, &filter_output, &filter_output_len, config_node->filter);

		int n_read = fwrite(filter_output, sizeof(float complex), filter_output_len, file);
		//FIXME check how n_read can be less than buffer's expected

		complete_buffer_processing(config_node->queue);
	}
	fclose(file);
	printf("dsp_worker stopped\n");
	return (void*) 0;
}

static void* rtlsdr_worker(void *arg) {
	core *core = (struct core_t*) arg;
	uint32_t buffer_size = core->server_config->buffer_size;
	int n_read;
	while (core->is_rtlsdr_running) {
		int code = rtlsdr_read_sync(core->dev, core->buffer, buffer_size, &n_read);
		if (code < 0) {
			fprintf(stderr, "rtl-sdr read failure. shutdown\n");
			core->is_rtlsdr_running = false;
			break;
		}
		pthread_mutex_lock(&core->mutex);
		struct linked_list_node *current_node = core->client_configs;
		while (current_node != NULL) {
			// copy to client's buffers and notify
			put(core->buffer, n_read, current_node->queue);
			current_node = current_node->next;
		}
		pthread_mutex_unlock(&core->mutex);
	}
	rtlsdr_close(core->dev);
	core->dev = NULL;
	printf("rtl-sdr stopped\n");
	return (void*) 0;
}

int start_rtlsdr(struct client_config *config) {
	core *core = config->core;
	if (core->is_rtlsdr_running) {
		return 0;
	}
	fprintf(stdout, "starting rtl-sdr\n");
	rtlsdr_dev_t *dev = NULL;
	rtlsdr_open(&dev, 0);
	if (dev == NULL) {
		return 0x04;
	}

	int code = rtlsdr_set_sample_rate(dev, core->server_config->band_sampling_rate);
	if (code < 0) {
		fprintf(stderr, "unable to set sampling rate: %d\n", code);
	}
	code = rtlsdr_set_center_freq(dev, config->band_freq);
	if (code < 0) {
		fprintf(stderr, "unable to set center freq: %d\n", code);
	}
	code = rtlsdr_set_tuner_gain_mode(dev, core->server_config->gain_mode);
	if (code < 0) {
		fprintf(stderr, "unable to set gain mode: %d\n", code);
	}
	if (core->server_config->gain_mode == 1) {
		code = rtlsdr_set_tuner_gain(dev, core->server_config->gain);
		if (code < 0) {
			fprintf(stderr, "unable to set tuner gain: %d\n", code);
		}
	}
	code = rtlsdr_set_bias_tee(dev, core->server_config->bias_t);
	if (code < 0) {
		fprintf(stderr, "unable to set bias_t: %d\n", code);
	}
	code = rtlsdr_reset_buffer(dev);
	if (code < 0) {
		fprintf(stderr, "unable to reset buffer: %d\n", code);
	}
	core->dev = dev;
	core->is_rtlsdr_running = true;

	pthread_t rtlsdr_worker_thread;
	code = pthread_create(&rtlsdr_worker_thread, NULL, &rtlsdr_worker, core);
	if (code != 0) {
		return 0x04;
	}
	core->rtlsdr_worker_thread = rtlsdr_worker_thread;
	return 0x0;
}

void stop_rtlsdr(core *core) {
	fprintf(stdout, "stopping rtl-sdr\n");
	core->is_rtlsdr_running = false;

	// block access to core until rtlsdr fully stops and cleans the resources
	pthread_mutex_lock(&core->mutex);
	pthread_join(core->rtlsdr_worker_thread, NULL);
	pthread_mutex_unlock(&core->mutex);
}

void destroy_node(struct linked_list_node *node) {
	if (node == NULL) {
		return;
	}
	if (node->taps != NULL) {
		free(node->taps);
	}
	if (node->filter != NULL) {
		destroy_xlating(node->filter);
	}
	node->is_running = false;
	if (node->queue != NULL) {
		destroy_queue(node->queue);
	}
	// wait until thread terminates and only then destroy the node
	pthread_join(node->dsp_thread, NULL);
	free(node);
}

int add_client(struct client_config *config) {
	struct linked_list_node *config_node = malloc(sizeof(struct linked_list_node));
	if (config_node == NULL) {
		return -ENOMEM;
	}
	// init all fields with 0 so that destroy_* method would work
	*config_node = (struct linked_list_node ) { 0 };

	// setup taps
	float *taps = NULL;
	size_t len;
	int code = create_low_pass_filter(1.0, config->core->server_config->band_sampling_rate, config->sampling_rate / 2, 2000, &taps, &len);
	if (code != 0) {
		destroy_node(config_node);
		return code;
	}
	config_node->taps = taps;

	// setup xlating frequency filter
	xlating *filter = NULL;
	//FIXME maybe some trick with 32 bit numbers?
	code = create_frequency_xlating_filter(12, taps, len, (int64_t) config->center_freq - (int64_t) config->band_freq, config->core->server_config->band_sampling_rate, config->core->server_config->buffer_size, &filter);
	if (code != 0) {
		destroy_node(config_node);
		return code;
	}
	config_node->filter = filter;
	config_node->config = config;

	// setup queue
	queue *client_queue = NULL;
	code = create_queue(config->core->server_config->buffer_size, 16, &client_queue);
	if (code != 0) {
		destroy_node(config_node);
		return -1;
	}
	config_node->queue = client_queue;
	config_node->is_running = true;

	// start processing
	pthread_t dsp_thread;
	code = pthread_create(&dsp_thread, NULL, &dsp_worker, config_node);
	if (code != 0) {
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
	//FIXME
//	config->is_running = false;
}

void destroy_core(core *core) {
	if (core == NULL) {
		return;
	}
	if (core->dev != NULL) {
		stop_rtlsdr(core);
	}
	if (core->buffer != NULL) {
		free(core->buffer);
	}
	pthread_mutex_lock(&core->mutex);
	struct linked_list_node *cur_node = core->client_configs;
	while (cur_node != NULL) {
		struct linked_list_node *next = cur_node->next;
		destroy_node(cur_node);
		cur_node = next;
	}
	pthread_mutex_unlock(&core->mutex);
	free(core);
}

