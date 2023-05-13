#ifndef SDR_SERVER_RTLSDR_LIB_MOCK_H
#define SDR_SERVER_RTLSDR_LIB_MOCK_H

void init_mock_librtlsdr();

void wait_for_data_read();

void setup_mock_data(uint8_t *buffer, int len);

void stop_rtlsdr_mock();

#endif //SDR_SERVER_RTLSDR_LIB_MOCK_H
