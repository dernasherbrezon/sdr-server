#include <stdlib.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "api.h"
#include "tcp_server.h"

struct tcp_server_t {
	int server_socket;
	pthread_attr_t attr;
	volatile sig_atomic_t is_running;
	pthread_t acceptor_thread;
	core *core;
	struct server_config *server_config;
};

void log_client(struct sockaddr_in *address, uint32_t id) {
	char str[INET_ADDRSTRLEN];
	const char *ptr = inet_ntop(AF_INET, &address->sin_addr, str, sizeof(str));
	printf("accepted client from %s:%d (id: %u)\n", ptr, ntohs(address->sin_port), id);
}

int read_struct(int socket, void *result, size_t len) {
	size_t left = len;
	while (left > 0) {
		int received = recv(socket, (char*) result + (len - left), left, 0);
		if (received < 0) {
			return -1;
		}
		left -= received;
	}
	return 0;
}

int read_client_config(int client_socket, struct server_config *server_config, struct client_config **config) {
	struct client_config *result = malloc(sizeof(struct client_config));
	if (result == NULL) {
		return -ENOMEM;
	}
	// init all fields with 0
	*result = (struct client_config ) { 0 };
	struct message_header header;
	if (read_struct(client_socket, &header, sizeof(struct message_header)) < 0) {
		fprintf(stderr, "unable to read header fully\n");
		free(result);
		return -1;
	}
	if (header.protocol_version != PROTOCOL_VERSION) {
		fprintf(stderr, "unsupported protocol: %d\n", header.protocol_version);
		free(result);
		return -1;
	}
	if (header.type != TYPE_REQUEST) {
		fprintf(stderr, "unsupported request: %d\n", header.type);
		free(result);
		return -1;
	}
	struct request req;
	if (read_struct(client_socket, &req, sizeof(struct request)) < 0) {
		fprintf(stderr, "unable to read request fully\n");
		free(result);
		return -1;
	}
	result->center_freq = req.center_freq;
	result->sampling_rate = req.sampling_rate;
	result->band_freq = req.band_freq;
	result->client_socket = client_socket;
	if (result->sampling_rate > 0 && server_config->band_sampling_rate % result->sampling_rate != 0) {
		fprintf(stderr, "sampling frequency is not an integer factor of server sample rate: %u\n", server_config->band_sampling_rate);
		free(result);
		return -1;
	}

	*config = result;
	return 0;
}

int validate_client_config(struct client_config *config, struct server_config *server_config) {
	if (config->center_freq == 0) {
		fprintf(stderr, "missing center_freq parameter\n");
		return -1;
	}
	if (config->sampling_rate == 0) {
		fprintf(stderr, "missing sampling_rate parameter\n");
		return -1;
	}
	if (config->band_freq == 0) {
		fprintf(stderr, "missing band_freq parameter\n");
		return -1;
	}
	if (config->sampling_rate > server_config->band_sampling_rate) {
		fprintf(stderr, "requested sampling rate is less than on server: %u\n", server_config->band_sampling_rate);
		return -1;
	}
	uint32_t requested_min_freq = config->center_freq - config->sampling_rate / 2;
	uint32_t server_min_freq = config->band_freq - server_config->band_sampling_rate / 2;
	if (requested_min_freq < server_min_freq) {
		fprintf(stderr, "requested center freq is out of the band: %u\n", config->center_freq);
		return -1;
	}
	uint32_t requested_max_freq = config->center_freq + config->sampling_rate / 2;
	uint32_t server_max_freq = config->band_freq + server_config->band_sampling_rate / 2;
	if (requested_max_freq > server_max_freq) {
		fprintf(stderr, "requested center freq is out of the band: %u\n", config->center_freq);
		return -1;
	}
	return 0;
}

int write_message(int socket, uint8_t status, uint8_t details) {
	struct message_header header;
	header.protocol_version = PROTOCOL_VERSION;
	header.type = TYPE_RESPONSE;
	struct response resp;
	resp.status = status;
	resp.details = details;

	// it is possible to directly populate *buffer with the fields,
	// however populating structs and then serializing them into byte array
	// is more readable
	size_t total_len = sizeof(struct message_header) + sizeof(struct response);
	char *buffer = malloc(total_len);
	memcpy(buffer, &header, sizeof(struct message_header));
	memcpy(buffer + sizeof(struct message_header), &resp, sizeof(struct response));

	size_t left = total_len;
	while (left > 0) {
		int written = write(socket, buffer + (total_len - left), left);
		if (written < 0) {
			perror("unable to write the message");
			free(buffer);
			return -1;
		}
		left -= written;
	}
	free(buffer);
	return 0;
}

void respond_failure(int client_socket, uint8_t status, uint8_t details) {
	write_message(client_socket, status, details); // unable to start device
	close(client_socket);
}

static void* client_worker(void *arg) {
	struct client_config *config = (struct client_config*) arg;
	while (config->is_running) {
		struct message_header header;
		if (read_struct(config->client_socket, &header, sizeof(struct message_header)) < 0) {
			fprintf(stdout, "client disconnected: %u\n", config->id);
			break;
		}
		if (header.protocol_version != PROTOCOL_VERSION) {
			fprintf(stderr, "unsupported protocol: %d\n", header.protocol_version);
			continue;
		}
		if (header.type != TYPE_SHUTDOWN) {
			fprintf(stderr, "unsupported request: %d\n", header.type);
			continue;
		}
		fprintf(stdout, "client is disconnecting: %u\n", config->id);
		break;
	}
	remove_client(config);
	close(config->client_socket);
	free(config);
	return (void*) 0;
}

static void* acceptor_worker(void *arg) {
	tcp_server *server = (tcp_server*) arg;
	uint32_t current_band_freq = 0;
	struct sockaddr_in address;
	uint32_t client_counter = 0;
	while (server->is_running) {
		int client_socket;
		int addrlen = sizeof(address);
		if ((client_socket = accept(server->server_socket, (struct sockaddr*) &address, (socklen_t*) &addrlen)) < 0) {
			break;
		}

		log_client(&address, client_counter);

		struct timeval tv;
		tv.tv_sec = server->server_config->read_timeout_seconds;
		tv.tv_usec = 0;
		if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof tv)) {
			close(client_socket);
			perror("setsockopt - SO_RCVTIMEO");
			continue;
		}

		struct client_config *config = NULL;
		if (read_client_config(client_socket, server->server_config, &config) < 0) {
			// close silently
			close(client_socket);
			continue;
		}
		if (validate_client_config(config, server->server_config) < 0) {
			respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
			free(config);
			continue;
		}
		if (current_band_freq != 0 && current_band_freq != config->band_freq) {
			respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_OUT_OF_BAND_FREQ);
			free(config);
			continue;
		}

		if (current_band_freq == 0) {
			current_band_freq = config->band_freq;
		}

		config->is_running = true;
		config->core = server->core;
		config->id = client_counter;
		client_counter++;

		int code = add_client(config);
		if (code != 0) {
			respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
			free(config);
			continue;
		}

		pthread_t client_thread;
		code = pthread_create(&client_thread, &server->attr, &client_worker, config);
		if (code != 0) {
			respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
			remove_client(config);
			free(config);
			continue;
		}

		write_message(client_socket, RESPONSE_STATUS_SUCCESS, RESPONSE_DETAILS_SUCCESS);
	}

	int code = pthread_attr_destroy(&server->attr);
	if (code != 0) {
		perror("unable to destroy attribute");
		free(server);
		return (void*) -1;
	}

	free(server);
	return (void*) 0;
}

int start_tcp_server(struct server_config *config, core *core, tcp_server **server) {
	tcp_server *result = malloc(sizeof(struct tcp_server_t));
	if (result == NULL) {
		return -ENOMEM;
	}
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == 0) {
		free(result);
		perror("socket creation failed");
		return -1;
	}
	result->server_socket = server_socket;
	result->is_running = true;
	result->server_config = config;
	int opt = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		free(result);
		perror("setsockopt - SO_REUSEADDR");
		return -1;
	}
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
		free(result);
		perror("setsockopt - SO_REUSEPORT");
		return -1;
	}

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	if (inet_pton(AF_INET, config->bind_address, &address.sin_addr) <= 0) {
		free(result);
		perror("invalid address");
		return -1;
	}
	address.sin_port = htons(config->port);

	if (bind(server_socket, (struct sockaddr*) &address, sizeof(address)) < 0) {
		free(result);
		perror("bind failed");
		return -1;
	}
	if (listen(server_socket, 3) < 0) {
		free(result);
		perror("listen failed");
		return -1;
	}

	pthread_attr_t attr;
	int code = pthread_attr_init(&attr);
	if (code != 0) {
		free(result);
		perror("unable to init attributes");
		return -1;
	}
	code = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (code != 0) {
		free(result);
		perror("unable to set attribute");
		return -1;
	}
	result->attr = attr;
	result->core = core;

	pthread_t acceptor_thread;
	code = pthread_create(&acceptor_thread, NULL, &acceptor_worker, result);
	if (code != 0) {
		free(result);
		return -1;
	}
	result->acceptor_thread = acceptor_thread;

	*server = result;
	return 0;
}

void join_tcp_server_thread(tcp_server *server) {
	pthread_join(server->acceptor_thread, NULL);
}

void stop_tcp_server(tcp_server *server) {
	if (server == NULL) {
		return;
	}
	fprintf(stdout, "stopping tcp server\n");
	server->is_running = false;
	// close is not enough to exit from the blocking "accept" method
	// execute shutdown first
	int code = shutdown(server->server_socket, SHUT_RDWR);
	if (code != 0) {
		close(server->server_socket);
	}
	pthread_join(server->acceptor_thread, NULL);
	// do not free tcp_server here
	// it should be destroyed on the thread during shutdown
}
