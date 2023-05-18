#ifndef TCP_CLIENT_H_
#define TCP_CLIENT_H_

#include <stdint.h>
#include "../api.h"

struct tcp_client {
	int client_socket;
};

int create_client(const char *addr, int port, struct tcp_client **tcp_client);

int write_data(void *buffer, size_t len, struct tcp_client *tcp_client);
int read_data(void *buffer, size_t len, struct tcp_client *tcp_client);

int write_request(struct message_header header, struct request req, struct tcp_client *tcp_client);
int send_message(struct tcp_client *client, uint8_t protocol, uint8_t type, uint32_t center_freq, uint32_t sampling_rate, uint32_t band_freq, uint8_t destination);
int read_response(struct message_header **header, struct response **resp, struct tcp_client *tcp_client);

void destroy_client(struct tcp_client *tcp_client);
// this will wait until server release all resources, stop all threads and closes connection
void gracefully_destroy_client(struct tcp_client *tcp_client);

#endif /* TCP_CLIENT_H_ */
