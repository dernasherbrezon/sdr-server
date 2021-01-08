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

struct Message {
	uint8_t command;
	uint32_t argument;
} __attribute__((packed));

struct ClientConfig {
	uint32_t centerFrequency;
	uint32_t sampleRate;
	int gainMode;
	int gain;
	int ppm;
	int ifStage;
	int ifGain;
	int agc;
	int offsetTuning;
	int biasT;
	uint32_t bandFrequency;
};

int readMessage(int socket, struct Message *result) {
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
	struct Message result;
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

int readClientConfig(int clientSocket, struct ClientConfig *config) {
	struct Message cmd;
	while (true) {
		if (readMessage(clientSocket, &cmd) < 0) {
			return -1;
		}
		if (cmd.command == 0x10) {
			break;
		}
		switch (cmd.command) {
		case 0x01:
			config->centerFrequency = cmd.argument;
			break;
		case 0x02:
			config->sampleRate = cmd.argument;
			break;
		case 0x03:
			config->gainMode = cmd.argument;
			break;
		case 0x04:
			config->gain = cmd.argument;
			break;
		case 0x05:
			config->ppm = cmd.argument;
			break;
		case 0x06:
			config->ifStage = cmd.argument >> 16;
			config->ifGain = (short) (cmd.argument & 0xffff);
			break;
		case 0x08:
			config->agc = cmd.argument;
			break;
		case 0x0a:
			config->offsetTuning = cmd.argument;
			break;
		case 0x0e:
			config->biasT = cmd.argument;
			break;
		case 0x11:
			config->bandFrequency = cmd.argument;
			break;
		default:
			fprintf(stderr, "unknown command: %d\n", cmd.command);
			break;
		}
	}
	return 0;
}

int validateClientConfig(struct ClientConfig *config) {
	//FIXME
	return 0;
}

static pthread_mutex_t ll_mutex;
static pthread_cond_t cond;

static struct llist *ll_buffers = 0;
static int llbuf_num = 500;
static int global_numq = 0;

struct llist {
	char *data;
	size_t len;
	struct llist *next;
};

void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx) {
	//TODO temporary linked list
	struct llist *rpt = (struct llist*) malloc(sizeof(struct llist));
	rpt->data = (char*) malloc(len);
	memcpy(rpt->data, buf, len);
	rpt->len = len;
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

static void* client_worker(void *arg) {



	struct timespec ts;
	struct timeval tp;
	struct llist *curelem, *prev;
	while (1) {
		pthread_mutex_lock(&ll_mutex);
		//FIXME relative timeout instead of absolute system time?
		gettimeofday(&tp, NULL);
		ts.tv_sec = tp.tv_sec + 50000;
		ts.tv_nsec = tp.tv_usec * 1000;
		//FIXME spurious wakeups not handled
		int r = pthread_cond_timedwait(&cond, &ll_mutex, &ts);
		//FIXME check timeout

		curelem = ll_buffers;
		ll_buffers = 0;
		pthread_mutex_unlock(&ll_mutex);

		while (curelem != NULL) {

			//FIXME DSP
			printf("processed %zu\n", curelem->len);

			prev = curelem;
			curelem = curelem->next;
			free(prev->data);
			free(prev);
		}
	}
	//FIXME worker thread
	return (void*) 0;
}

static void* rtlsdr_worker(void *arg) {
	rtlsdr_dev_t *dev = (rtlsdr_dev_t*) arg;
	//FIXME tune buffers here
	int result = rtlsdr_read_async(dev, rtlsdr_callback, NULL, 0, 0);
	if (result != 0) {
		//FIXME cancel all workers threads
	}
	rtlsdr_close(dev);
	return (void*) 0;
}

int main(int argc, char **argv) {
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
	//FIXME interface
	if (inet_pton(AF_INET, "0.0.0.0", &address.sin_addr) <= 0) {
		perror("invalid address");
		exit(EXIT_FAILURE);
	}
	//FIXME port
	address.sin_port = htons(8081);

	if (bind(serverSocket, (struct sockaddr*) &address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(serverSocket, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	// use detached threads
	pthread_attr_t attr;
	int code = pthread_attr_init(&attr);
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

		struct ClientConfig config;
		if (readClientConfig(clientSocket, &config) < 0) {
			// close silently
			close(clientSocket);
			continue;
		}
		if (validateClientConfig(&config) < 0) {
			writeMessage(clientSocket, 0x01, 0x01); // invalid response
			close(clientSocket);
			continue;
		}

		if (currentBandFrequency != 0 && currentBandFrequency != config.bandFrequency) {
			//FIXME more validations
			writeMessage(clientSocket, 0x01, 0x02);  // out of band frequency
			close(clientSocket);
			continue;
		}

		if (currentBandFrequency == 0) {
			rtlsdr_dev_t *dev = NULL;
			printf("starting rtl-sdr\n");
			rtlsdr_open(&dev, 0);
			if (dev == NULL) {
				writeMessage(clientSocket, 0x01, 0x03); // no suitable device found
				close(clientSocket);
				continue;
			}
			//FIXME from config 1400000
			code = rtlsdr_set_sample_rate(dev, config.sampleRate);
			if (code < 0) {
				writeMessage(clientSocket, 0x01, 0x04); // unable to start device
				close(clientSocket);
				rtlsdr_close(dev);
				continue;
			}
			printf("sample rate: %u\n", config.sampleRate);
			code = rtlsdr_set_center_freq(dev, config.bandFrequency);
			if (code < 0) {
				writeMessage(clientSocket, 0x01, 0x04); // unable to start device
				close(clientSocket);
				rtlsdr_close(dev);
				continue;
			}
			printf("band frequency: %u\n", config.bandFrequency);
			code = rtlsdr_set_tuner_gain_mode(dev, config.gainMode);
			if (code < 0) {
				writeMessage(clientSocket, 0x01, 0x04); // unable to start device
				close(clientSocket);
				rtlsdr_close(dev);
				continue;
			}
			printf("gain mode: %d\n", config.gainMode);
			if (config.gainMode == 1) {
				code = rtlsdr_set_tuner_gain(dev, config.gain);
				if (code < 0) {
					writeMessage(clientSocket, 0x01, 0x04); // unable to start device
					close(clientSocket);
					rtlsdr_close(dev);
					continue;
				}
				printf("gain: %d\n", config.gain);
			}
			code = rtlsdr_set_bias_tee(dev, config.biasT);
			if (code < 0) {
				writeMessage(clientSocket, 0x01, 0x04); // unable to start device
				close(clientSocket);
				rtlsdr_close(dev);
				continue;
			}
			printf("biast: %d\n", config.biasT);
			code = rtlsdr_reset_buffer(dev);
			if (code < 0) {
				writeMessage(clientSocket, 0x01, 0x04); // unable to start device
				close(clientSocket);
				rtlsdr_close(dev);
				continue;
			}

			pthread_t rtlsdr_worker_thread;
			code = pthread_create(&rtlsdr_worker_thread, &attr, &rtlsdr_worker, dev);
			if (code != 0) {
				writeMessage(clientSocket, 0x01, 0x04); // unable to start device
				close(clientSocket);
				rtlsdr_close(dev);
				continue;
			}
			currentBandFrequency = config.bandFrequency;
		}

		pthread_t worker_thread;
		code = pthread_create(&worker_thread, &attr, &client_worker, &config);
		if (code != 0) {
			writeMessage(clientSocket, 0x01, 0x04); // unable to start device
			close(clientSocket);
			continue;
		}

		writeMessage(clientSocket, 0x01, 0x00); // success
	}

	code = pthread_attr_destroy(&attr);
	if (code != 0) {
		perror("unable to destroy attribute");
		exit(EXIT_FAILURE);
	}

	close(serverSocket);
}

