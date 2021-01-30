#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

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

int write_data(uint8_t *buffer, size_t total_len, struct tcp_client *tcp_client) {
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

int write_client_message(struct message_header header, struct request req, struct tcp_client *tcp_client) {
	// it is possible to directly populate *buffer with the fields,
	// however populating structs and then serializing them into byte array
	// is more readable
	size_t total_len = sizeof(struct message_header) + sizeof(struct request);
	uint8_t *buffer = malloc(total_len);
	memcpy(buffer, &header, sizeof(struct message_header));
	memcpy(buffer + sizeof(struct message_header), &req, sizeof(struct request));
	int code = write_data(buffer, total_len, tcp_client);
	free(buffer);
	return code;
}

int read_response_struct(int socket, void *result, size_t len) {
	size_t left = len;
	while (left > 0) {
		int received = recv(socket, (char*) result + (len - left), left, 0);
		if (received < 0) {
			perror("unable to read the message");
			return -1;
		}
		left -= received;
	}
	return 0;
}

int read_data(struct message_header **response_header, struct response **resp, struct tcp_client *tcp_client) {
	struct message_header *header = malloc(sizeof(struct message_header));
	if (header == NULL) {
		return -ENOMEM;
	}
	int code = read_response_struct(tcp_client->client_socket, header, sizeof(struct message_header));
	if (code != 0) {
		free(header);
		return code;
	}
	struct response *result = malloc(sizeof(struct response));
	if (result == NULL) {
		free(header);
		return -ENOMEM;
	}
	code = read_response_struct(tcp_client->client_socket, result, sizeof(struct response));
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

