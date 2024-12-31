#include "tcp_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "api.h"
#include "dsp_worker.h"
#include "sdr_worker.h"

struct linked_list_tcp_node {
  struct linked_list_tcp_node *next;
  dsp_worker *dsp_worker;
  client_config *config;
  pthread_t client_thread;
  tcp_server *server;
};

struct tcp_server_t {
  int server_socket;
  volatile sig_atomic_t is_running;
  pthread_t acceptor_thread;
  sdr_worker *core;
  struct server_config *server_config;
  uint32_t client_counter;
  uint32_t current_band_freq;

  struct linked_list_tcp_node *tcp_nodes;
  pthread_mutex_t mutex;
  pthread_cond_t terminated_condition;
};

void log_client(struct sockaddr_in *address, uint32_t id) {
  char str[INET_ADDRSTRLEN];
  const char *ptr = inet_ntop(AF_INET, &address->sin_addr, str, sizeof(str));
  printf("[%d] accepted new client from %s:%d\n", id, ptr, ntohs(address->sin_port));
}

int read_struct(int socket, void *result, size_t len) {
  size_t left = len;
  while (left > 0) {
    ssize_t received = recv(socket, (char *)result + (len - left), left, 0);
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
      if (received != -1) {
        perror("unable to read struct");
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

int read_client_config(int client_socket, uint32_t client_id, struct server_config *server_config, client_config **config) {
  client_config *result = malloc(sizeof(client_config));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0
  *result = (client_config){0};
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

int validate_client_config(client_config *config, struct server_config *server_config, uint32_t client_id) {
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

int write_message(int socket, uint8_t status, uint32_t details) {
  struct message_header header;
  header.protocol_version = PROTOCOL_VERSION;
  header.type = TYPE_RESPONSE;
  struct response resp;
  resp.status = status;
  resp.details = htonl(details);

  // it is possible to directly populate *buffer with the fields,
  // however populating structs and then serializing them into byte array
  // is more readable
  size_t total_len = sizeof(struct message_header) + sizeof(struct response);
  char *buffer = malloc(total_len);
  memcpy(buffer, &header, sizeof(struct message_header));
  memcpy(buffer + sizeof(struct message_header), &resp, sizeof(struct response));

  size_t left = total_len;
  while (left > 0) {
    ssize_t written = write(socket, buffer + (total_len - left), left);
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

void respond_failure(int client_socket, uint8_t status, uint32_t details) {
  write_message(client_socket, status, details);  // unable to start device
  close(client_socket);
}

static void tcp_node_destroy(struct linked_list_tcp_node *node) {
  if (node == NULL) {
    return;
  }
  if (node->config != NULL) {
    close(node->config->client_socket);
  }
  if (node->dsp_worker != NULL) {
    dsp_worker_destroy(node->dsp_worker);
  }
}

static void *tcp_worker(void *arg) {
  struct linked_list_tcp_node *node = (struct linked_list_tcp_node *)arg;
  uint32_t node_id = node->config->id;
  fprintf(stdout, "[%d] tcp_worker started. center_freq %d sampling_rate %d destination %d\n", node_id, node->config->center_freq, node->config->sampling_rate, node->config->destination);
  while (node->config->is_running) {
    struct message_header header;
    int code = read_struct(node->config->client_socket, &header, sizeof(struct message_header));
    if (code < -1) {
      // read timeout happened. it's ok.
      // client already sent all information we need
      continue;
    }
    if (code == -1) {
      fprintf(stdout, "[%d] client disconnected\n", node_id);
      break;
    }
    if (header.protocol_version != PROTOCOL_VERSION) {
      fprintf(stderr, "<3>[%d] unsupported protocol: %d\n", node_id, header.protocol_version);
      continue;
    }
    if (header.type != TYPE_SHUTDOWN) {
      fprintf(stderr, "<3>[%d] unsupported request: %d\n", node_id, header.type);
      continue;
    }
    fprintf(stdout, "[%d] client requested disconnect\n", node_id);
    break;
  }
  fprintf(stdout, "[%d] tcp_worker is stopping\n", node_id);
  pthread_mutex_lock(&node->server->mutex);
  struct linked_list_tcp_node *cur_node = node->server->tcp_nodes;
  struct linked_list_tcp_node *previous = NULL;
  node->config->is_running = false;
  close(node->config->client_socket);
  int number_of_running = 0;
  while (cur_node != NULL) {
    if (cur_node->config->is_running) {
      number_of_running++;
      break;
    }
    cur_node = cur_node->next;
  }
  if (number_of_running == 0) {
    sdr_worker_stop(node->server->core);
    node->server->current_band_freq = 0;
  }
  pthread_cond_broadcast(&node->server->terminated_condition);
  pthread_mutex_unlock(&node->server->mutex);
  tcp_node_destroy(node);
  return (void *)0;
}

static void sdr_callback(uint8_t *buf, uint32_t buf_len, void *ctx) {
  tcp_server *server = (tcp_server *)ctx;
  int result = 0;
  pthread_mutex_lock(&server->mutex);
  struct linked_list_tcp_node *current_node = server->tcp_nodes;
  while (current_node != NULL) {
    // copy to client's buffers and notify
    dsp_worker_process(buf, buf_len, current_node->dsp_worker);
    current_node = current_node->next;
  }
  pthread_mutex_unlock(&server->mutex);
}

static void tcp_node_final_cleanup(struct linked_list_tcp_node *cur_node) {
  uint32_t node_id = cur_node->config->id;
  pthread_join(cur_node->client_thread, NULL);
  free(cur_node->config);
  free(cur_node);
  fprintf(stdout, "[%d] tcp_worker stopped\n", node_id);
}

static void cleanup_terminated_threads(tcp_server *server) {
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
    tcp_node_final_cleanup(cur_node);
    cur_node = next;
  }
}

void handle_new_client(int client_socket, tcp_server *server) {
  client_config *config = NULL;
  if (read_client_config(client_socket, server->client_counter, server->server_config, &config) < 0) {
    respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
    return;
  }
  if (validate_client_config(config, server->server_config, server->client_counter) < 0) {
    respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
    free(config);
    return;
  }

  config->is_running = true;
  config->id = server->client_counter;
  config->sdr_type = server->server_config->sdr_type;

  struct linked_list_tcp_node *tcp_node = malloc(sizeof(struct linked_list_tcp_node));
  if (tcp_node == NULL) {
    respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
    free(config);
    return;
  }
  tcp_node->config = config;
  tcp_node->next = NULL;
  tcp_node->server = server;

  int code = pthread_create(&tcp_node->client_thread, NULL, &tcp_worker, tcp_node);
  if (code != 0) {
    respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
    tcp_node_destroy(tcp_node);
    return;
  }

  code = dsp_worker_start(config, server->server_config, &tcp_node->dsp_worker);
  if (code != 0) {
    respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
    tcp_node_destroy(tcp_node);
    return;
  }

  pthread_mutex_lock(&server->mutex);
  cleanup_terminated_threads(server);
  if (server->tcp_nodes == NULL) {
    server->current_band_freq = config->band_freq;
    code = sdr_worker_start(config, server->core);
    if (code == 0) {
      server->tcp_nodes = tcp_node;
    }
  } else {
    if (server->current_band_freq != 0 && server->current_band_freq != config->band_freq) {
      respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_OUT_OF_BAND_FREQ);
      fprintf(stderr, "<3>[%d] requested out of band frequency: %d\n", server->client_counter, config->band_freq);
      pthread_mutex_unlock(&server->mutex);
      // this will shutdown the thread
      close(tcp_node->config->client_socket);
      // this one will close any remaning resources
      tcp_node_final_cleanup(tcp_node);
      return;
    }
    struct linked_list_tcp_node *cur_node = server->tcp_nodes;
    while (cur_node->next != NULL) {
      cur_node = cur_node->next;
    }
    cur_node->next = tcp_node;
  }
  pthread_mutex_unlock(&server->mutex);

  if (code != 0) {
    respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
    close(tcp_node->config->client_socket);
    tcp_node_final_cleanup(tcp_node);
    return;
  }

  write_message(client_socket, RESPONSE_STATUS_SUCCESS, tcp_node->config->id);
}

static void *acceptor_worker(void *arg) {
  tcp_server *server = (tcp_server *)arg;
  struct sockaddr_in address;
  while (server->is_running) {
    int client_socket;
    int addrlen = sizeof(address);
    if ((client_socket = accept(server->server_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
      break;
    }

    struct timeval tv;
    tv.tv_sec = server->server_config->read_timeout_seconds;
    tv.tv_usec = 0;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv)) {
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

  pthread_mutex_lock(&server->mutex);
  struct linked_list_tcp_node *cur_node = server->tcp_nodes;
  while (cur_node != NULL) {
    struct linked_list_tcp_node *next = cur_node->next;
    close(cur_node->config->client_socket);
    uint32_t node_id = cur_node->config->id;
    while (cur_node->config->is_running) {
      pthread_cond_wait(&server->terminated_condition, &server->mutex);
    }
    pthread_join(cur_node->client_thread, NULL);
    free(cur_node->config);
    free(cur_node);
    fprintf(stdout, "[%d] tcp_worker stopped\n", node_id);
    server->tcp_nodes = next;
    cur_node = next;
  }
  pthread_mutex_unlock(&server->mutex);

  if (server->core != NULL) {
    sdr_worker_destroy(server->core);
  }

  printf("tcp server stopped\n");
  return (void *)0;
}

int start_tcp_server(struct server_config *config, tcp_server **server) {
  tcp_server *result = malloc(sizeof(struct tcp_server_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  result->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  result->tcp_nodes = NULL;
  int code = sdr_worker_create(sdr_callback, result, config, &result->core);
  if (code != 0) {
    free(result);
    return -1;
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
  // start counting from 0
  result->client_counter = -1;
  result->current_band_freq = 0;
  result->terminated_condition = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
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

  if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    free(result);
    perror("bind failed");
    return -1;
  }
  if (listen(server_socket, 3) < 0) {
    free(result);
    perror("listen failed");
    return -1;
  }

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
  if (server == NULL) {
    return;
  }
  pthread_join(server->acceptor_thread, NULL);
  free(server);
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
}
