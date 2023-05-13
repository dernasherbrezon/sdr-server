#include <stdlib.h>
#include "airspy_device.h"

struct airspy_device_t {
  uint32_t id;
  struct airspy_device *dev;

  float complex *output;
  size_t output_len;

  airspy_lib *lib;
};

int airspy_device_create(uint32_t id, uint32_t max_input_buffer_length, airspy_lib *lib, sdr_device **result) {
  return 0;
}

int airspy_device_process_rx(float complex **output, size_t *output_len, void *plugin) {
  return 0;
}

void airspy_device_destroy(void *plugin) {
  if( plugin == NULL ) {
    return;
  }

}