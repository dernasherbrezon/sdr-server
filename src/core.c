#include <stdio.h>
#include <rtl-sdr.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

#include "core.h"

struct core_t {
	rtlsdr_dev_t *dev;
	struct server_config *server_config;
	volatile sig_atomic_t is_rtlsdr_running;
	pthread_t rtlsdr_worker_thread;
};

static const uint32_t BUFFER_SIZE = 16 * 32 * 512;

static pthread_mutex_t ll_mutex;
static pthread_cond_t cond;

static struct llist *ll_buffers = 0;
static int llbuf_num = 500;
static int global_numq = 0;

struct llist {
	int8_t *data;
//	char *data;
	size_t len;
	struct llist *next;
};

int create_core(struct server_config *server_config, core **result) {
	core *core = malloc(sizeof(struct core_t));
	if (core == NULL) {
		return -ENOMEM;
	}
	core->server_config = server_config;

	*result = core;
	return 0;
}

static void* rtlsdr_worker(void *arg) {
	core *core = (struct core_t *) arg;
	uint8_t *buffer;
	uint32_t out_block_size = BUFFER_SIZE;
	int n_read;
	// FIXME tune the buffer size
	buffer = malloc(out_block_size * sizeof(uint8_t));
	while (core->is_rtlsdr_running) {
		int result = rtlsdr_read_sync(core->dev, buffer, out_block_size, &n_read);
		//TODO temporary linked list. allocate memory in advance
		struct llist *rpt = (struct llist*) malloc(sizeof(struct llist));
		rpt->data = (int8_t*) malloc(n_read);
//		rpt->data = (char*) malloc(n_read);
		memcpy(rpt->data, buffer, n_read);
		rpt->len = n_read;
		rpt->next = NULL;

		pthread_mutex_lock(&ll_mutex);
		if (ll_buffers == NULL) {
			ll_buffers = rpt;
		} else {
			struct llist *cur = ll_buffers;
			int num_queued = 0;

			while (cur->next != NULL) {
				cur = cur->next;
				num_queued++;
			}

			if (llbuf_num && llbuf_num == num_queued - 2) {
				struct llist *curelem;

				free(ll_buffers->data);
				curelem = ll_buffers->next;
				free(ll_buffers);
				ll_buffers = curelem;
			}

			cur->next = rpt;

			if (num_queued > global_numq)
				printf("ll+, now %d\n", num_queued);
			else if (num_queued < global_numq)
				printf("ll-, now %d\n", num_queued);

			global_numq = num_queued;
		}
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&ll_mutex);
	}
	printf("stopping\n");
	free(buffer);
	rtlsdr_close(core->dev);
	return (void*) 0;
}

int start_rtlsdr(struct client_config *config, core *core) {
	fprintf(stdout, "starting rtl-sdr\n");
	rtlsdr_dev_t *dev = NULL;
	rtlsdr_open(&dev, 0);
	if (dev == NULL) {
		return 0x04;
	}

	int code = rtlsdr_set_sample_rate(dev, core->server_config->band_sampling_rate);
	if (code < 0) {
		return 0x04;
	}
	code = rtlsdr_set_center_freq(dev, config->band_freq);
	if (code < 0) {
		return 0x04;
	}
	code = rtlsdr_set_tuner_gain_mode(dev, core->server_config->gain_mode);
	if (code < 0) {
		return 0x04;
	}
	if (core->server_config->gain_mode == 1) {
		code = rtlsdr_set_tuner_gain(dev, core->server_config->gain);
		if (code < 0) {
			return 0x04;
		}
	}
	code = rtlsdr_set_bias_tee(dev, core->server_config->bias_t);
	if (code < 0) {
		return 0x04;
	}
	code = rtlsdr_reset_buffer(dev);
	if (code < 0) {
		return 0x04;
	}
	core->dev = dev;

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
	pthread_join(core->rtlsdr_worker_thread, NULL);
}

int add_client(struct client_config *config, core *core) {
	//FIXME
	return 0;
}
void remove_client(struct client_config *config, core *core) {
	//FIXME
}

void destroy_core(core *core) {
	if (core == NULL) {
		return;
	}
	free(core);
}

