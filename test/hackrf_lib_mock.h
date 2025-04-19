#ifndef hackrf_LIB_MOCK_H
#define hackrf_LIB_MOCK_H

#include <stdint.h>

void hackrf_wait_for_data_read();

void hackrf_setup_mock_data(int8_t *buffer, int len);

void hackrf_stop_mock();

#endif  // hackrf_LIB_MOCK_H
