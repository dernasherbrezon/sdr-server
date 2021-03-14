#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "tcp_client.h"

int create_client(const char *addr, int port, struct tcp_client **tcp_client) {
	struct tcp_client *result = malloc(sizeof(struct tcp_client));
	if (result == NULL) {
		return -ENOMEM;
	}
	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket == -1) {
		free(result);
		fprintf(stderr, "socket creation failed: %d\n", client_socket);
		return -1;
	}
	result->client_socket = client_socket;

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(addr);
	address.sin_port = htons(port);
	int code = connect(client_socket, (struct sockaddr*) &address, sizeof(address));
	if (code != 0) {
		free(result);
		fprintf(stderr, "connection with the server failed: %d\n", code);
		return -1;
	}
	fprintf(stdout, "connected to the server..\n");

	*tcp_client = result;
	return 0;
}

int write_data(void *buffer, size_t total_len, struct tcp_client *tcp_client) {
	size_t left = total_len;
	while (left > 0) {
		int written = write(tcp_client->client_socket, buffer + (total_len - left), left);
		if (written < 0) {
			perror("unable to write the message");
			return -1;
		}
		left -= written;
	}
	return 0;
}

int write_request(struct message_header header, struct request req, struct tcp_client *tcp_client) {
	req.band_freq = htonl(req.band_freq);
	req.center_freq = htonl(req.center_freq);
	req.sampling_rate = htonl(req.sampling_rate);
	// it is possible to directly populate *buffer with the fields,
	// however populating structs and then serializing them into byte array
	// is more readable
	size_t total_len = sizeof(struct message_header) + sizeof(struct request);
	uint8_t *buffer = malloc(total_len);
	if (buffer == NULL) {
		return -ENOMEM;
	}
	memcpy(buffer, &header, sizeof(struct message_header));
	memcpy(buffer + sizeof(struct message_header), &req, sizeof(struct request));
	int code = write_data(buffer, total_len, tcp_client);
	free(buffer);
	return code;
}

int send_message(struct tcp_client *client, uint8_t protocol, uint8_t type, uint32_t center_freq, uint32_t sampling_rate, uint32_t band_freq, uint8_t destination) {
	struct message_header header;
	header.protocol_version = protocol;
	header.type = type;
	struct request req;
	req.band_freq = band_freq;
	req.center_freq = center_freq;
	req.sampling_rate = sampling_rate;
	req.destination = destination;
	return write_request(header, req, client);
}

int read_data(void *result, size_t len, struct tcp_client *tcp_client) {
	size_t left = len;
	while (left > 0) {
		int received = recv(tcp_client->client_socket, (char*) result + (len - left), left, 0);
		if (received < 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				return -errno;
			}
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

int read_response(struct message_header **response_header, struct response **resp, struct tcp_client *tcp_client) {
	struct message_header *header = malloc(sizeof(struct message_header));
	if (header == NULL) {
		return -ENOMEM;
	}
	int code = read_data(header, sizeof(struct message_header), tcp_client);
	if (code != 0) {
		free(header);
		return code;
	}
	struct response *result = malloc(sizeof(struct response));
	if (result == NULL) {
		free(header);
		return -ENOMEM;
	}
	code = read_data(result, sizeof(struct response), tcp_client);
	if (code != 0) {
		free(header);
		free(result);
		return code;
	}
	*response_header = header;
	*resp = result;
	return 0;
}

void destroy_client(struct tcp_client *tcp_client) {
	if (tcp_client == NULL) {
		return;
	}
	close(tcp_client->client_socket);
	free(tcp_client);
}

void gracefully_destroy_client(struct tcp_client *tcp_client) {
	while (true) {
		struct message_header header;
		int code = read_data(&header, sizeof(struct message_header), tcp_client);
		if (code < -1) {
			// read timeout happened. it's ok.
			// client already sent all information we need
			continue;
		}
		if (code == -1) {
			break;
		}
	}
	fprintf(stdout, "disconnected from the server..\n");
	destroy_client(tcp_client);
}

