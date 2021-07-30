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

struct linked_list_tcp_node {
	struct client_config *config;
	struct linked_list_tcp_node *next;
	pthread_t client_thread;
	tcp_server *server;
};

struct tcp_server_t {
	int server_socket;
	volatile sig_atomic_t is_running;
	pthread_t acceptor_thread;
	core *core;
	struct server_config *server_config;
    uint32_t client_counter;
	uint32_t current_band_freq;

	struct linked_list_tcp_node *tcp_nodes;
	pthread_mutex_t mutex;
};

void log_client(struct sockaddr_in *address, uint32_t id) {
	char str[INET_ADDRSTRLEN];
	const char *ptr = inet_ntop(AF_INET, &address->sin_addr, str, sizeof(str));
	printf("[%d] accepted new client from %s:%d\n", id, ptr, ntohs(address->sin_port));
}

int read_struct(int socket, void *result, size_t len) {
	size_t left = len;
	while (left > 0) {
		int received = recv(socket, (char*) result + (len - left), left, 0);
		if (received < 0) {
			// will happen on timeout
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				return -errno;
			}
			// SIGINT and SIGTERM handled in main
			// all other signals should be ignored and syscall should be retried
			// For example, EINTR happens when gdb disconnects
			if (errno == EINTR) {
				continue;
			}
			// other types of errors
			// log and disconnect
			perror("unable to read struct");
			return -1;
		}
		// client has closed the socket
		if (received == 0) {
			return -1;
		}
		left -= received;
	}
	return 0;
}

int read_client_config(int client_socket, uint32_t client_id, struct server_config *server_config, struct client_config **config) {
	struct client_config *result = malloc(sizeof(struct client_config));
	if (result == NULL) {
		return -ENOMEM;
	}
	// init all fields with 0
	*result = (struct client_config ) { 0 };
	struct request req;
	if (read_struct(client_socket, &req, sizeof(struct request)) < 0) {
		fprintf(stderr, "<3>[%d] unable to read request fully\n", client_id);
		free(result);
		return -1;
	}
	result->center_freq = ntohl(req.center_freq);
	result->sampling_rate = ntohl(req.sampling_rate);
	result->band_freq = ntohl(req.band_freq);
	result->client_socket = client_socket;
	result->destination = req.destination;
	if (result->sampling_rate > 0 && server_config->band_sampling_rate % result->sampling_rate != 0) {
		fprintf(stderr, "<3>[%d] sampling frequency is not an integer factor of server sample rate: %u\n", client_id, server_config->band_sampling_rate);
		free(result);
		return -1;
	}

	*config = result;
	return 0;
}

int validate_client_config(struct client_config *config, struct server_config *server_config, uint32_t client_id) {
	if (config->center_freq == 0) {
		fprintf(stderr, "<3>[%d] missing center_freq parameter\n", client_id);
		return -1;
	}
	if (config->sampling_rate == 0) {
		fprintf(stderr, "<3>[%d] missing sampling_rate parameter\n", client_id);
		return -1;
	}
	if (config->band_freq == 0) {
		fprintf(stderr, "<3>[%d] missing band_freq parameter\n", client_id);
		return -1;
	}
	if (config->destination != REQUEST_DESTINATION_FILE && config->destination != REQUEST_DESTINATION_SOCKET) {
		fprintf(stderr, "<3>[%d] unknown destination: %d\n", client_id, config->destination);
		return -1;
	}
	uint32_t requested_min_freq = config->center_freq - config->sampling_rate / 2;
	uint32_t server_min_freq = config->band_freq - server_config->band_sampling_rate / 2;
	if (requested_min_freq < server_min_freq) {
		fprintf(stderr, "<3>[%d] requested center freq is out of the band: %u\n", client_id, config->center_freq);
		return -1;
	}
	uint32_t requested_max_freq = config->center_freq + config->sampling_rate / 2;
	uint32_t server_max_freq = config->band_freq + server_config->band_sampling_rate / 2;
	if (requested_max_freq > server_max_freq) {
		fprintf(stderr, "<3>[%d] requested center freq is out of the band: %u\n", client_id, config->center_freq);
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

static void* tcp_worker(void *arg) {
	struct linked_list_tcp_node *node = (struct linked_list_tcp_node*) arg;
	fprintf(stdout, "[%d] tcp_worker is starting\n", node->config->id);
	int code = add_client(node->config);
	if (code != 0) {
		respond_failure(node->config->client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
	} else {
		write_message(node->config->client_socket, RESPONSE_STATUS_SUCCESS, node->config->id);
		fprintf(stdout, "[%d] center_freq %d sampling_rate %d destination %d\n", node->config->id, node->config->center_freq, node->config->sampling_rate, node->config->destination);
		while (node->config->is_running) {
			struct message_header header;
			code = read_struct(node->config->client_socket, &header, sizeof(struct message_header));
			if (code < -1) {
				// read timeout happened. it's ok.
				// client already sent all information we need
				continue;
			}
			if (code == -1) {
				fprintf(stdout, "[%d] client disconnected\n", node->config->id);
				break;
			}
			if (header.protocol_version != PROTOCOL_VERSION) {
				fprintf(stderr, "<3>[%d] unsupported protocol: %d\n", node->config->id, header.protocol_version);
				continue;
			}
			if (header.type != TYPE_SHUTDOWN) {
				fprintf(stderr, "<3>[%d] unsupported request: %d\n", node->config->id, header.type);
				continue;
			}
			fprintf(stdout, "[%d] client requested disconnect\n", node->config->id);
			break;
		}
		remove_client(node->config);
	}
	node->config->is_running = false;
	close(node->config->client_socket);
	return (void*) 0;
}

void cleanup_terminated_threads(tcp_server *server) {
	pthread_mutex_lock(&server->mutex);
	struct linked_list_tcp_node *cur_node = server->tcp_nodes;
	struct linked_list_tcp_node *previous = NULL;
	while (cur_node != NULL) {
		struct linked_list_tcp_node *next = cur_node->next;
		if (cur_node->config->is_running) {
			previous = cur_node;
			cur_node = next;
			continue;
		}
		if (previous == NULL) {
			server->tcp_nodes = next;
		} else {
			previous->next = next;
		}

		fprintf(stdout, "[%d] tcp_worker is stopping\n", cur_node->config->id);
		pthread_join(cur_node->client_thread, NULL);
		free(cur_node->config);
		free(cur_node);

		cur_node = next;
	}
	if (server->tcp_nodes == NULL) {
		server->current_band_freq = 0;
	}
	pthread_mutex_unlock(&server->mutex);
}

void add_tcp_node(struct linked_list_tcp_node *node) {
	pthread_mutex_lock(&node->server->mutex);
	if (node->server->tcp_nodes == NULL) {
		node->server->tcp_nodes = node;
	} else {
		struct linked_list_tcp_node *cur_node = node->server->tcp_nodes;
		while (cur_node->next != NULL) {
			cur_node = cur_node->next;
		}
		cur_node->next = node;
	}
	pthread_mutex_unlock(&node->server->mutex);
}

void remove_all_tcp_nodes(tcp_server *server) {
	pthread_mutex_lock(&server->mutex);
	// get number of threads and signal them to terminate
	struct linked_list_tcp_node *cur_node = server->tcp_nodes;
	while (cur_node != NULL) {
		struct linked_list_tcp_node *next = cur_node->next;
		cur_node->config->is_running = false;
		close(cur_node->config->client_socket);

		fprintf(stdout, "[%d] tcp_worker is stopping\n", cur_node->config->id);
		pthread_join(cur_node->client_thread, NULL);
		free(cur_node->config);
		free(cur_node);

		cur_node = next;
	}
	pthread_mutex_unlock(&server->mutex);
}

void handle_new_client(int client_socket, tcp_server *server) {
	struct client_config *config = NULL;
	if (read_client_config(client_socket, server->client_counter, server->server_config, &config) < 0) {
		respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
		return;
	}
	if (validate_client_config(config, server->server_config, server->client_counter) < 0) {
		respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
		free(config);
		return;
	}

	cleanup_terminated_threads(server);

	if (server->current_band_freq != 0 && server->current_band_freq != config->band_freq) {
		respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_OUT_OF_BAND_FREQ);
		fprintf(stderr, "<3>[%d] requested out of band frequency: %d\n", server->client_counter, config->band_freq);
		free(config);
		return;
	}

	if (server->current_band_freq == 0) {
		server->current_band_freq = config->band_freq;
	}

	config->is_running = true;
	config->core = server->core;
	config->id = server->client_counter;

	struct linked_list_tcp_node *tcp_node = malloc(sizeof(struct linked_list_tcp_node));
	if (tcp_node == NULL) {
		respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
		free(config);
		return;
	}
	tcp_node->config = config;
	tcp_node->next = NULL;
	tcp_node->server = server;

	pthread_t client_thread;
	int code = pthread_create(&client_thread, NULL, &tcp_worker, tcp_node);
	if (code != 0) {
		respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
		free(config);
		free(tcp_node);
		return;
	}
	tcp_node->client_thread = client_thread;

	add_tcp_node(tcp_node);
}

static void* acceptor_worker(void *arg) {
	tcp_server *server = (tcp_server*) arg;
	struct sockaddr_in address;
	while (server->is_running) {
		int client_socket;
		int addrlen = sizeof(address);
		if ((client_socket = accept(server->server_socket, (struct sockaddr*) &address, (socklen_t*) &addrlen)) < 0) {
			break;
		}

		struct timeval tv;
		tv.tv_sec = server->server_config->read_timeout_seconds;
		tv.tv_usec = 0;
		if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof tv)) {
			close(client_socket);
			perror("setsockopt - SO_RCVTIMEO");
			continue;
		}

		// always increment counter to make even error messages traceable
		server->client_counter++;

		struct message_header header;
		if (read_struct(client_socket, &header, sizeof(struct message_header)) < 0) {
			fprintf(stderr, "<3>[%d] unable to read request header fully\n", server->client_counter);
			respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
			continue;
		}
		if (header.protocol_version != PROTOCOL_VERSION) {
			fprintf(stderr, "<3>[%d] unsupported protocol: %d\n", server->client_counter, header.protocol_version);
			respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
			continue;
		}

		switch (header.type) {
			case TYPE_REQUEST:
				log_client(&address, server->client_counter);
				handle_new_client(client_socket, server);
				break;
			case TYPE_PING:
				write_message(client_socket, RESPONSE_STATUS_SUCCESS, 0);
				close(client_socket);
				break;
			default:
				fprintf(stderr, "<3>[%d] unsupported request: %d\n", server->client_counter, header.type);
				respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
				break;
		}

	}

	remove_all_tcp_nodes(server);

	printf("tcp server stopped\n");
	return (void*) 0;
}

int start_tcp_server(struct server_config *config, core *core, tcp_server **server) {
	tcp_server *result = malloc(sizeof(struct tcp_server_t));
	if (result == NULL) {
		return -ENOMEM;
	}
	result->mutex = (pthread_mutex_t )PTHREAD_MUTEX_INITIALIZER;
	result->tcp_nodes = NULL;

	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == 0) {
		free(result);
		perror("socket creation failed");
		return -1;
	}
	result->server_socket = server_socket;
	result->is_running = true;
	result->server_config = config;
	// start counting from 0
	result->client_counter = -1;
	result->current_band_freq = 0;
	int opt = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		free(result);
		perror("setsockopt - SO_REUSEADDR");
		return -1;
	}

#ifdef SO_REUSEPORT
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
		free(result);
		perror("setsockopt - SO_REUSEPORT");
		return -1;
	}
#endif

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
	result->core = core;

	pthread_t acceptor_thread;
	int code = pthread_create(&acceptor_thread, NULL, &acceptor_worker, result);
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
	fprintf(stdout, "tcp server is stopping\n");
	server->is_running = false;
	// close is not enough to exit from the blocking "accept" method
	// execute shutdown first
	int code = shutdown(server->server_socket, SHUT_RDWR);
	if (code != 0) {
		close(server->server_socket);
	}
	pthread_join(server->acceptor_thread, NULL);

	free(server);
}
