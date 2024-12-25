#ifndef SDR_SERVER_RTLSDR_LIB_H
#define SDR_SERVER_RTLSDR_LIB_H

#include <rtl-sdr.h>

typedef struct rtlsdr_lib_t rtlsdr_lib;

struct rtlsdr_lib_t {
  void *handle;

  int (*rtlsdr_open)(rtlsdr_dev_t **dev, uint32_t index);

  int (*rtlsdr_close)(rtlsdr_dev_t *dev);

  int (*rtlsdr_set_sample_rate)(rtlsdr_dev_t *dev, uint32_t rate);

  int (*rtlsdr_set_center_freq)(rtlsdr_dev_t *dev, uint32_t freq);

  int (*rtlsdr_set_tuner_gain_mode)(rtlsdr_dev_t *dev, int manual);

  int (*rtlsdr_get_tuner_gains)(rtlsdr_dev_t *dev, int *gains);

  int (*rtlsdr_set_tuner_gain)(rtlsdr_dev_t *dev, int gain);

  int (*rtlsdr_set_bias_tee)(rtlsdr_dev_t *dev, int on);

  int (*rtlsdr_reset_buffer)(rtlsdr_dev_t *dev);

  int (*rtlsdr_read_sync)(rtlsdr_dev_t *dev, void *buf, int len, int *n_read);

  int (*rtlsdr_set_freq_correction)(rtlsdr_dev_t *dev, int ppm);

  int (*rtlsdr_get_index_by_serial)(const char *serial);
};

int rtlsdr_lib_create(rtlsdr_lib **lib);

void rtlsdr_lib_destroy(rtlsdr_lib *lib);

#endif //SDR_SERVER_RTLSDR_LIB_H
