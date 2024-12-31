#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "tcp_server.h"

static tcp_server *server = NULL;

extern const char *SIMD_STATUS;

void sdrserver_stop_async(int signum) {
  stop_tcp_server(server);
  server = NULL;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "<3>parameter missing: configuration file\n");
    exit(EXIT_FAILURE);
  }
  setvbuf(stdout, NULL, _IOLBF, 0);
  printf("SIMD optimization: %s\n", SIMD_STATUS);
  struct server_config *server_config = NULL;
  int code = create_server_config(&server_config, argv[1]);
  if (code != 0) {
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, sdrserver_stop_async);
  signal(SIGHUP, sdrserver_stop_async);
  signal(SIGTERM, sdrserver_stop_async);

  code = start_tcp_server(server_config, &server);
  if (code != 0) {
    destroy_server_config(server_config);
    exit(EXIT_FAILURE);
  }

  // wait here until server terminates
  join_tcp_server_thread(server);

  // server will be freed on its own thread
  destroy_server_config(server_config);
}
