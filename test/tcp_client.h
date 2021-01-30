#ifndef TCP_CLIENT_H_
#define TCP_CLIENT_H_

#include <stdint.h>
#include "../src/api.h"

struct tcp_client {
	int client_socket;
};

int create_client(const char *addr, int port, struct tcp_client** tcp_client);

int write_data(uint8_t *buffer, size_t total_len, struct tcp_client *tcp_client);
int write_client_message(struct message_header header, struct request req, struct tcp_client *tcp_client);

int read_data(struct response **resp, struct tcp_client* tcp_client);

void destroy_client(struct tcp_client* tcp_client);


#endif /* TCP_CLIENT_H_ */
