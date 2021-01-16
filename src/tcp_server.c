#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

#include "tcp_server.h"

struct tcp_server_t {
	int server_socket;
	pthread_attr_t attr;
	volatile sig_atomic_t is_running;
	pthread_t acceptor_thread;
	core *core;
	struct server_config *server_config;
};

struct message {
	uint8_t command;
	uint32_t argument;
} __attribute__((packed));

void log_client(struct sockaddr_in *address, uint32_t id) {
	char str[INET_ADDRSTRLEN];
	const char *ptr = inet_ntop(AF_INET, &address->sin_addr, str, sizeof(str));
	printf("accepted client from %s:%d (id: %u)\n", ptr, ntohs(address->sin_port), id);
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

int read_client_config(int client_socket, struct server_config *server_config, struct client_config **config) {
	struct client_config *result = malloc(sizeof(struct client_config));
	// init all fields with 0
	*result = (struct client_config ) { 0 };
	struct message cmd;
	//FIXME true?
	while (true) {
		if (read_message(client_socket, &cmd) < 0) {
			return -1;
		}
		if (cmd.command == 0x10) {
			break;
		}
		//FIXME add api.h + add protocol version command
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

int write_message(int socket, uint8_t command, uint32_t argument) {
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

void respond_failure(tcp_server *server, int client_socket, int response, int status) {
	if (status != 0) {
		fprintf(stderr, "unable to perform operation. status: %d\n", status);
	}
	write_message(client_socket, response, status); // unable to start device
	close(client_socket);
	stop_rtlsdr(server->core);
}

static void* client_worker(void *arg) {
	struct client_config *config = (struct client_config*) arg;
	while (config->is_running) {
		struct message cmd;
		if (read_message(config->client_socket, &cmd) < 0) {
			fprintf(stdout, "client disconnected: %u\n", config->id);
			break;
		}
		if (cmd.command == 0x12) {
			fprintf(stdout, "client is disconnecting: %u\n", config->id);
			break;
		}
		// just read any data from client
		// if connection closed from the client side, then shutdown calculation
		fprintf(stderr, "unexpected command: %d\n", cmd.command);
	}
	remove_client(config);
	close(config->client_socket);
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

		struct client_config *config = NULL;
		if (read_client_config(client_socket, server->server_config, &config) < 0) {
			// close silently
			close(client_socket);
			continue;
		}
		if (validate_client_config(config, server->server_config) < 0) {
			// invalid request
			respond_failure(NULL, client_socket, 0x01, 0x01);
			continue;
		}
		config->is_running = true;
		config->core = server->core;
		config->id = client_counter;
		client_counter++;

		if (current_band_freq != 0 && current_band_freq != config->band_freq) {
			// out of band frequency
			respond_failure(NULL, client_socket, 0x01, 0x02);
			continue;
		}

		// init rtl-sdr only for the first client
		if (current_band_freq == 0) {
			int code = start_rtlsdr(config);
			if (code != 0) {
				respond_failure(server, client_socket, 0x01, code);
				continue;
			}

			current_band_freq = config->band_freq;
		}

		int code = add_client(config);
		if (code != 0) {
			respond_failure(server, client_socket, 0x01, code);
			continue;
		}

		pthread_t client_thread;
		code = pthread_create(&client_thread, &server->attr, &client_worker, config);
		if (code != 0) {
			respond_failure(NULL, client_socket, 0x01, 0x04);
			remove_client(config);
			continue;
		}

		write_message(client_socket, 0x01, 0x00); // success
	}

	int code = pthread_attr_destroy(&server->attr);
	if (code != 0) {
		perror("unable to destroy attribute");
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
		perror("socket creation failed");
		return -1;
	}
	result->server_socket = server_socket;
	result->is_running = true;
	result->server_config = config;
	int opt = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt - SO_REUSEADDR");
		return -1;
	}
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
		perror("setsockopt - SO_REUSEPORT");
		return -1;
	}
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	if (inet_pton(AF_INET, config->bind_address, &address.sin_addr) <= 0) {
		perror("invalid address");
		return -1;
	}
	address.sin_port = htons(config->port);

	if (bind(server_socket, (struct sockaddr*) &address, sizeof(address)) < 0) {
		perror("bind failed");
		return -1;
	}
	if (listen(server_socket, 3) < 0) {
		perror("listen failed");
		return -1;
	}

	pthread_attr_t attr;
	int code = pthread_attr_init(&attr);
	if (code != 0) {
		perror("unable to init attributes");
		return -1;
	}
	code = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (code != 0) {
		perror("unable to set attribute");
		return -1;
	}
	result->attr = attr;
	result->core = core;

	pthread_t acceptor_thread;
	code = pthread_create(&acceptor_thread, NULL, &acceptor_worker, result);
	if (code != 0) {
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
	close(server->server_socket);
	pthread_join(server->acceptor_thread, NULL);
	// do not free tcp_server here
	// it should be destroyed on the thread during shutdown
}
