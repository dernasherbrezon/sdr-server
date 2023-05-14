#ifndef SDR_SERVER_SDR_DEVICE_H
#define SDR_SERVER_SDR_DEVICE_H

#include <complex.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct sdr_device_t sdr_device;

struct sdr_device_t {
    void *plugin;

    void (*destroy)(void *plugin);
    int (*start_rx)(uint32_t band_freq, void *plugin);
    void (*stop_rx)(void *plugin);
};

#endif //SDR_SERVER_SDR_DEVICE_H
