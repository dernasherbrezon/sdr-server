#ifndef AIRSPY_LIB_MOCK_H
#define AIRSPY_LIB_MOCK_H

#include <stdint.h>

void airspy_wait_for_data_read();

void airspy_setup_mock_data(int16_t *buffer, int len);

void airspy_stop_mock();

#endif //AIRSPY_LIB_MOCK_H
