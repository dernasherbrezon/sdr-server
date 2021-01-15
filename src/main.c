#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <rtl-sdr.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>

#include <complex.h>
#include <volk/volk.h>

#include "config.h"
#include "lpf.h"
#include "xlating.h"

static const uint32_t NUMBER_OF_BUFFERS = 15;
static const uint32_t BUFFER_SIZE = 16 * 32 * 512;

struct message {
	uint8_t command;
	uint32_t argument;
} __attribute__((packed));

struct client_config {
	uint32_t center_freq;
	uint32_t sampling_rate;
	uint32_t band_freq;
	int client_socket;
};

struct server_config *server_config = NULL;
static volatile sig_atomic_t app_running = true;

void plutosdr_stop_async() {
	app_running = false;
}

int read_message(int socket, struct message *result) {
	int left = sizeof(*result);
	while (left > 0) {
		int received = read(socket, (char*) result + (sizeof(*result) - left), left);
		if (received < 0) {
			perror("unable to read the message");
			return -1;
		}
		left -= received;
	}
	result->argument = ntohl(result->argument);
	return 0;
}

int writeMessage(int socket, uint8_t command, uint32_t argument) {
	struct message result;
	result.command = command;
	result.argument = ntohl(argument);
	int left = sizeof(result);
	while (left > 0) {
		int written = write(socket, (char*) &result + (sizeof(result) - left), left);
		if (written < 0) {
			perror("unable to write the message");
			return -1;
		}
		left -= written;
	}
	return 0;
}

// FIXME ok. time to change the style. make underscore everywhere
int read_client_config(int client_socket, struct server_config *server_config, struct client_config **config) {
	struct client_config *result = malloc(sizeof(struct client_config));
	// init all fields with 0
	*result = (struct client_config ) { 0 };
	struct message cmd;
	while (true) {
		if (read_message(client_socket, &cmd) < 0) {
			return -1;
		}
		if (cmd.command == 0x10) {
			break;
		}
		switch (cmd.command) {
		case 0x01:
			result->center_freq = cmd.argument;
			break;
		case 0x02:
			result->sampling_rate = cmd.argument;
			break;
		case 0x11:
			result->band_freq = cmd.argument;
			break;
		default:
			fprintf(stderr, "unknown command: %d\n", cmd.command);
			break;
		}
	}
	result->client_socket = client_socket;
	if (server_config->band_sampling_rate % result->sampling_rate != 0) {
		fprintf(stderr, "sampling frequency is not an integer factor of server sample rate: %u\n", server_config->band_sampling_rate);
		return -1;
	}
	return 0;
}

int validateClientConfig(struct client_config *config) {
	//FIXME
	return 0;
}

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

static void* client_worker(void *arg) {
	printf("started client worker\n");
	struct client_config *config = (struct client_config*) arg;
	float *taps = NULL;
	size_t len;
	// FIXME replace double with floats everywhere
	double sampling_freq = 288000;
	size_t code = create_low_pass_filter(1.0, sampling_freq, config->sampling_rate / 2, 2000, &taps, &len);
	if (code != 0) {
		fprintf(stderr, "unable to setup taps: %zu\n", code);
		close(config->client_socket);
		return ((void*) code);
	}
	xlating *filter = NULL;
	//FIXME should come from the config
//	code = create_frequency_xlating_filter(12, taps, len, config->bandFrequency - config->centerFrequency, config->sampling_freq, BUFFER_SIZE, &filter);
	//FIXME maybe some trick with 32 bit numbers?
	printf("diff: %lld\n", (int64_t) config->center_freq - (int64_t) config->band_freq);
	code = create_frequency_xlating_filter(12, taps, len, (int64_t) config->center_freq - (int64_t) config->band_freq, sampling_freq, BUFFER_SIZE, &filter);
	if (code != 0) {
		fprintf(stderr, "unable to setup filter: %zu\n", code);
		close(config->client_socket);
		free(taps);
		return ((void*) code);
	}

	FILE *file;
	file = fopen("/tmp/file.raw", "wb");

	struct timespec ts;
	struct timeval tp;
	struct llist *curelem, *prev;
	float complex *output;
	size_t output_len = 0;
	printf("getting new data\n");
	while (app_running) {
		pthread_mutex_lock(&ll_mutex);
		//FIXME relative timeout instead of absolute system time?
		gettimeofday(&tp, NULL);
		ts.tv_sec = tp.tv_sec + 5;
		ts.tv_nsec = tp.tv_usec * 1000;
		//FIXME spurious wakeups not handled
		int r = pthread_cond_timedwait(&cond, &ll_mutex, &ts);
		//FIXME check timeout

		curelem = ll_buffers;
		ll_buffers = 0;
		pthread_mutex_unlock(&ll_mutex);

		while (curelem != NULL) {
			printf("processing %zu\n", curelem->len);
			process(curelem->data, curelem->len, &output, &output_len, filter);
			printf("processed %zu\n", curelem->len);
			int n_read = fwrite(output, sizeof(float complex), output_len, file);
//			int n_read = fwrite(output, sizeof(float complex) * output_len, 1, file);
//			int n_read = fwrite(curelem->data, 1, curelem->len, file);
//			fprintf(stderr, "written %d expected to write: %zu\n", n_read, output_len);
			prev = curelem;
			curelem = curelem->next;
			free(prev->data);
			free(prev);
		}
	}
	printf("stopping\n");
	fclose(file);
	//FIXME worker thread
	return (void*) 0;
}

static void* rtlsdr_worker(void *arg) {
	rtlsdr_dev_t *dev = (rtlsdr_dev_t*) arg;
	uint8_t *buffer;
	uint32_t out_block_size = BUFFER_SIZE;
	int n_read;
	// FIXME tune the buffer size
	buffer = malloc(out_block_size * sizeof(uint8_t));
	while (app_running) {
		int result = rtlsdr_read_sync(dev, buffer, out_block_size, &n_read);
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
	free(buffer);
	printf("stopping\n");
	rtlsdr_close(dev);
	return (void*) 0;
}

void respond_failure(rtlsdr_dev_t *dev, int client_socket, int response, int status) {
	if (status != 0) {
		// FIXME use stderr for all errors
		printf("unable to perform operation. status: %d\n", status);
	}
	writeMessage(client_socket, response, status); // unable to start device
	close(client_socket);
	if (dev != NULL) {
		rtlsdr_close(dev);
	}
}

int init_rtl_sdr(rtlsdr_dev_t **dev, struct client_config *config, struct server_config *server_config) {
	printf("starting rtl-sdr\n");
	rtlsdr_open(dev, 0);
	if (dev == NULL) {
		return 0x04;
	}

	int code = rtlsdr_set_sample_rate(*dev, server_config->band_sampling_rate);
	if (code < 0) {
		return 0x04;
	}
	code = rtlsdr_set_center_freq(*dev, config->band_freq);
	if (code < 0) {
		return 0x04;
	}
	code = rtlsdr_set_tuner_gain_mode(*dev, server_config->gain_mode);
	if (code < 0) {
		return 0x04;
	}
	if (server_config->gain_mode == 1) {
		code = rtlsdr_set_tuner_gain(*dev, server_config->gain);
		if (code < 0) {
			return 0x04;
		}
	}
	code = rtlsdr_set_bias_tee(*dev, server_config->bias_t);
	if (code < 0) {
		return 0x04;
	}
	code = rtlsdr_reset_buffer(*dev);
	if (code < 0) {
		return 0x04;
	}
	return 0x0;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "parameter missing: configuration file\n");
		exit(EXIT_FAILURE);
	}
	int code = create_server_config(&server_config, argv[1]);
	if (code != 0) {
		exit(EXIT_FAILURE);
	}
//	size_t align = volk_get_alignment();
//	printf("%zu %zu %f\n", align, sizeof(float), (float)align / sizeof(float complex));
	//FIXME properly handle shutdown
//	signal(SIGINT, plutosdr_stop_async);
//	signal(SIGHUP, plutosdr_stop_async);
//	signal(SIGSEGV, plutosdr_stop_async);
//	signal(SIGTERM, plutosdr_stop_async);

	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	int opt = 1;
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt - SO_REUSEADDR");
		exit(EXIT_FAILURE);
	}
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
		perror("setsockopt - SO_REUSEPORT");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	if (inet_pton(AF_INET, server_config->bind_address, &address.sin_addr) <= 0) {
		perror("invalid address");
		exit(EXIT_FAILURE);
	}
	address.sin_port = htons(server_config->port);

	if (bind(serverSocket, (struct sockaddr*) &address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(serverSocket, 3) < 0) {
		perror("listen failed");
		exit(EXIT_FAILURE);
	}

	// use detached threads
	pthread_attr_t attr;
	code = pthread_attr_init(&attr);
	if (code != 0) {
		perror("unable to init attributes");
		exit(EXIT_FAILURE);
	}
	code = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (code != 0) {
		perror("unable to set attribute");
		exit(EXIT_FAILURE);
	}

	uint32_t currentBandFrequency = 0;
	while (true) {
		int clientSocket;
		int addrlen = sizeof(address);
		if ((clientSocket = accept(serverSocket, (struct sockaddr*) &address, (socklen_t*) &addrlen)) < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		char str[INET_ADDRSTRLEN];
		const char *ptr = inet_ntop(AF_INET, &address.sin_addr, str, sizeof(str));
		printf("accepted client from %s:%d\n", ptr, ntohs(address.sin_port));

		struct client_config *config = NULL;
		if (read_client_config(clientSocket, server_config, &config) < 0) {
			// close silently
			close(clientSocket);
			continue;
		}
		if (validateClientConfig(config) < 0) {
			// invalid request
			respond_failure(NULL, clientSocket, 0x01, 0x01);
			continue;
		}

		if (currentBandFrequency != 0 && currentBandFrequency != config->band_freq) {
			//FIXME more validations
			// out of band frequency
			respond_failure(NULL, clientSocket, 0x01, 0x02);
			continue;
		}

		if (currentBandFrequency == 0) {
			rtlsdr_dev_t *dev = NULL;
			code = init_rtl_sdr(&dev, config, server_config);
			if (code != 0) {
				respond_failure(dev, clientSocket, 0x01, code);
				continue;
			}

			pthread_t rtlsdr_worker_thread;
			code = pthread_create(&rtlsdr_worker_thread, &attr, &rtlsdr_worker, dev);
			if (code != 0) {
				respond_failure(dev, clientSocket, 0x01, 0x04);
				continue;
			}
			currentBandFrequency = config->band_freq;
		}

		pthread_t worker_thread;
		code = pthread_create(&worker_thread, &attr, &client_worker, &config);
		if (code != 0) {
			respond_failure(NULL, clientSocket, 0x01, 0x04);
			continue;
		}

		writeMessage(clientSocket, 0x01, 0x00); // success
	}

	//FIXME cleanup after shutdown

	code = pthread_attr_destroy(&attr);
	if (code != 0) {
		perror("unable to destroy attribute");
		exit(EXIT_FAILURE);
	}

	close(serverSocket);
}

