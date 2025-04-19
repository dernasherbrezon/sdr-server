#ifndef SDR_SERVER_RTLSDR_LIB_MOCK_H
#define SDR_SERVER_RTLSDR_LIB_MOCK_H

#include <stdint.h>

void rtlsdr_wait_for_data_read();

void rtlsdr_setup_mock_data(uint8_t *buffer, int len);

void rtlsdr_stop_mock();

#endif //SDR_SERVER_RTLSDR_LIB_MOCK_H
