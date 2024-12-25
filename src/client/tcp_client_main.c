#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "tcp_client.h"

#define ERROR_CHECK(x)           \
  do {                           \
    int __err_rc = (x);          \
    if (__err_rc != 0) {         \
      return __err_rc;           \
    }                            \
  } while (0)

volatile sig_atomic_t do_exit = 0;

static void sighandler(int signum) {
  fprintf(stderr, "Signal caught, exiting!\n");
  do_exit = 1;
}

void usage() {
  printf("Usage:\n");
  printf("  -h  print help\n");
  printf("  -k <hostname> sdr_server hostname (default: localhost)\n");
  printf("  -p <port> sdr_server port (default: 8090)\n");
  printf("  -s <sampling_rate> sampling rate (default: 48000)\n");
  printf("  -f <center_freq> Center frequency. Frequency where signal of interest is\n");
  printf("  -b <band_freq> Band frequency. Multiple clients have to specify same frequency band\n");
  printf("  <filename> Filename to output (a '-' dumps samples to stdout)\n");
}

int main(int argc, char **argv) {
  int port = 8090;
  uint32_t center_freq = 0;
  uint32_t band_freq = 0;
  uint32_t sampling_rate = 48000;
  char *filename = NULL;
  char *hostname = "127.0.0.1";

  int dopt;
  while ((dopt = getopt(argc, argv, "hr:k:m:b:s:p:n:f:")) != EOF) {
    switch (dopt) {
      case 'h':
        usage();
        return EXIT_SUCCESS;
      case 'k':
        hostname = optarg;
        break;
      case 'b':
        band_freq = (uint32_t) atof(optarg);
        break;
      case 's':
        sampling_rate = (uint32_t) atof(optarg);
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'f':
        center_freq = (uint32_t) atof(optarg);
        break;
      default:
        exit(EXIT_FAILURE);
    }
  }

  if (argc <= optind || center_freq == 0) {
    usage();
    return EXIT_SUCCESS;
  } else {
    filename = argv[optind];
  }

  if (band_freq == 0) {
    band_freq = center_freq;
  }

  struct tcp_client *client = NULL;
  ERROR_CHECK(create_client(hostname, port, &client));
  ERROR_CHECK(send_message(client, PROTOCOL_VERSION, TYPE_REQUEST, center_freq, sampling_rate, band_freq, REQUEST_DESTINATION_SOCKET));
  struct message_header *response_header = NULL;
  struct response *resp = NULL;
  ERROR_CHECK(read_response(&response_header, &resp, client));
  if (response_header->type != TYPE_RESPONSE) {
    fprintf(stderr, "invalid response type received: %d\n", response_header->type);
    destroy_client(client);
    return EXIT_FAILURE;
  }
  if (resp->status != RESPONSE_STATUS_SUCCESS) {
    fprintf(stderr, "invalid request: %d\n", resp->details);
    destroy_client(client);
    return EXIT_FAILURE;
  }
  FILE *output;
  if (strcmp(filename, "-") == 0) {
    output = stdout;
  } else {
    output = fopen(filename, "wb");
    if (!output) {
      fprintf(stderr, "failed to open %s\n", filename);
      destroy_client(client);
      return EXIT_FAILURE;
    }
  }
  size_t buffer_length = 262144;
  uint8_t *buffer = malloc(buffer_length);
  if (buffer == NULL) {
    destroy_client(client);
    return -ENOMEM;
  }
  while (!do_exit) {
    int code = read_data(buffer, buffer_length, client);
    if (code != 0) {
      fprintf(stderr, "unable to read data. shutdown\n");
      destroy_client(client);
      return EXIT_FAILURE;
    }

    size_t left = buffer_length;
    while (left > 0) {
      size_t written = fwrite(buffer + (buffer_length - left), sizeof(uint8_t), left, output);
      if (written == 0) {
        perror("unable to write the message");
        destroy_client(client);
        return EXIT_FAILURE;
      }
      left -= written;
    }
  }

  return EXIT_SUCCESS;
}