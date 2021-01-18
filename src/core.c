#include <stdio.h>
#include <rtl-sdr.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

#include "queue.h"
#include "core.h"

struct linked_list_node {
	struct linked_list_node *next;
	struct client_config *config;
	queue *queue;
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
	core->server_config = server_config;
	uint8_t *buffer = malloc(server_config->buffer_size * sizeof(uint8_t));
	if (buffer == NULL) {
		return -ENOMEM;
	}
	core->buffer = buffer;
	core->mutex = (pthread_mutex_t )PTHREAD_MUTEX_INITIALIZER;
	*result = core;
	return 0;
}

static void* dsp_worker(void *arg) {
//	printf("started client worker\n");
//	struct client_config *config = (struct client_config*) arg;
//	float *taps = NULL;
//	size_t len;
//	// FIXME replace double with floats everywhere
//	double sampling_freq = 288000;
//	size_t code = create_low_pass_filter(1.0, sampling_freq, config->sampling_rate / 2, 2000, &taps, &len);
//	if (code != 0) {
//		fprintf(stderr, "unable to setup taps: %zu\n", code);
//		close(config->client_socket);
//		return ((void*) code);
//	}
//	xlating *filter = NULL;
//	//FIXME should come from the config
////	code = create_frequency_xlating_filter(12, taps, len, config->bandFrequency - config->centerFrequency, config->sampling_freq, BUFFER_SIZE, &filter);
//	//FIXME maybe some trick with 32 bit numbers?
//	printf("diff: %lld\n", (int64_t) config->center_freq - (int64_t) config->band_freq);
//	code = create_frequency_xlating_filter(12, taps, len, (int64_t) config->center_freq - (int64_t) config->band_freq, sampling_freq, BUFFER_SIZE, &filter);
//	if (code != 0) {
//		fprintf(stderr, "unable to setup filter: %zu\n", code);
//		close(config->client_socket);
//		free(taps);
//		return ((void*) code);
//	}
//
//	FILE *file;
//	file = fopen("/tmp/file.raw", "wb");
//
//	struct timespec ts;
//	struct timeval tp;
//	struct llist *curelem, *prev;
//	float complex
//	*output;
//	size_t output_len = 0;
//	printf("getting new data\n");
//	while (app_running) {
//		pthread_mutex_lock(&ll_mutex);
//		//FIXME relative timeout instead of absolute system time?
//		gettimeofday(&tp, NULL);
//		ts.tv_sec = tp.tv_sec + 5;
//		ts.tv_nsec = tp.tv_usec * 1000;
//		//FIXME spurious wakeups not handled
//		int r = pthread_cond_timedwait(&cond, &ll_mutex, &ts);
//		//FIXME check timeout
//
//		curelem = ll_buffers;
//		ll_buffers = 0;
//		pthread_mutex_unlock(&ll_mutex);
//
//		while (curelem != NULL) {
//			printf("processing %zu\n", curelem->len);
//			process(curelem->data, curelem->len, &output, &output_len, filter);
//			printf("processed %zu\n", curelem->len);
//			int n_read = fwrite(output, sizeof(float complex), output_len, file);
////			int n_read = fwrite(output, sizeof(float complex) * output_len, 1, file);
////			int n_read = fwrite(curelem->data, 1, curelem->len, file);
////			fprintf(stderr, "written %d expected to write: %zu\n", n_read, output_len);
//			prev = curelem;
//			curelem = curelem->next;
//			free(prev->data);
//			free(prev);
//		}
//	}
	printf("stopping dsp worker\n");
//	fclose(file);
	//FIXME worker thread
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
	printf("stopping rtl-sdr\n");
	rtlsdr_close(core->dev);
	core->dev = NULL;
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

int add_client(struct client_config *config) {
	struct linked_list_node *config_node = malloc(sizeof(struct linked_list_node));
	if (config_node == NULL) {
		return -ENOMEM;
	}
	config_node->config = config;
	queue *client_queue = NULL;
	int code = create_queue(config->core->server_config->buffer_size, 16, &client_queue);
	if (code != 0) {
		free(config_node);
		return -1;
	}
	config_node->queue = client_queue;

	pthread_t dsp_thread;
	code = pthread_create(&dsp_thread, NULL, &dsp_worker, config_node);
	if (code != 0) {
		free(config_node);
		destroy_queue(client_queue);
		return -1;
	}

	int result;
	pthread_mutex_lock(&config->core->mutex);
	if (config->core->client_configs == NULL) {
		config->core->client_configs = config_node;
		// init rtl-sdr only for the first client
		result = start_rtlsdr(config);
	} else {
		struct linked_list_node *last_config = config->core->client_configs;
		while (last_config->next != NULL) {
			last_config = last_config->next;
		}
		last_config->next = config_node;
		result = 0;
	}
	pthread_mutex_unlock(&config->core->mutex);
	return result;
}
void remove_client(struct client_config *config) {
	config->is_running = false;
	//FIXME
}

void destroy_core(core *core) {
	if (core == NULL) {
		return;
	}
	free(core);
}

