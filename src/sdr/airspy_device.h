#ifndef SDR_SERVER_AIRSPY_DEVICE_H
#define SDR_SERVER_AIRSPY_DEVICE_H

#include "airspy_lib.h"
#include "sdr_device.h"

int airspy_device_create(uint32_t id, uint32_t max_input_buffer_length, airspy_lib *lib, sdr_device **result);

int airspy_device_process_rx(float complex **output, size_t *output_len, void *plugin);

void airspy_device_destroy(void *plugin);

#endif //SDR_SERVER_AIRSPY_DEVICE_H
