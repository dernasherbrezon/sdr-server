#ifndef TCP_SERVER_H_
#define TCP_SERVER_H_

#include "config.h"

typedef struct tcp_server_t tcp_server;

int start_tcp_server(struct server_config *config, tcp_server **server);

void join_tcp_server_thread(tcp_server *server);

void stop_tcp_server(tcp_server *server);


#endif /* TCP_SERVER_H_ */
