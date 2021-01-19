#include <rtl-sdr.h>

#define rtlsdr_read_sync rtlsdr_read_sync_mocked
#define rtlsdr_close rtlsdr_close_mocked
#define rtlsdr_open rtlsdr_open_mocked
#define rtlsdr_set_sample_rate rtlsdr_set_sample_rate_mocked
#define rtlsdr_set_center_freq rtlsdr_set_center_freq_mocked
#define rtlsdr_set_tuner_gain_mode rtlsdr_set_tuner_gain_mode_mocked
#define rtlsdr_set_tuner_gain rtlsdr_set_tuner_gain_mocked
#define rtlsdr_set_bias_tee rtlsdr_set_bias_tee_mocked
#define rtlsdr_reset_buffer rtlsdr_reset_buffer_mocked

int rtlsdr_read_sync_mocked(rtlsdr_dev_t *dev, void *buf, int len, int *n_read);
int rtlsdr_close_mocked(rtlsdr_dev_t *dev);
int rtlsdr_open_mocked(rtlsdr_dev_t **dev, uint32_t index);
int rtlsdr_set_sample_rate_mocked(rtlsdr_dev_t *dev, uint32_t rate);
int rtlsdr_set_center_freq_mocked(rtlsdr_dev_t *dev, uint32_t freq);
int rtlsdr_set_tuner_gain_mode_mocked(rtlsdr_dev_t *dev, int manual);
int rtlsdr_set_tuner_gain_mocked(rtlsdr_dev_t *dev, int gain);
int rtlsdr_set_bias_tee_mocked(rtlsdr_dev_t *dev, int on);
int rtlsdr_reset_buffer_mocked(rtlsdr_dev_t *dev);

#include "../src/core.c"

#undef rtlsdr_read_sync
#undef rtlsdr_close
#undef rtlsdr_open
#undef rtlsdr_set_sample_rate
#undef rtlsdr_set_center_freq
#undef rtlsdr_set_tuner_gain_mode
#undef rtlsdr_set_tuner_gain
#undef rtlsdr_set_bias_tee
#undef rtlsdr_reset_buffer

struct mock_status {
	uint8_t *buffer;
	int len;
};

struct rtlsdr_dev {
	int dummy;
};

struct mock_status mock;

int rtlsdr_read_sync_mocked(rtlsdr_dev_t *dev, void *buf, int len, int *n_read) {
	if (mock.buffer != NULL) {
		buf = mock.buffer;
		*n_read = mock.len;
	} else {
		*n_read = 0;
	}
	return 0;
}

int rtlsdr_close_mocked(rtlsdr_dev_t *dev) {
	if (dev != NULL) {
		free(dev);
	}
	return 0;
}

int rtlsdr_open_mocked(rtlsdr_dev_t **dev, uint32_t index) {
	rtlsdr_dev_t *result = malloc(sizeof(rtlsdr_dev_t));
	*dev = result;
	return 0;
}
int rtlsdr_set_sample_rate_mocked(rtlsdr_dev_t *dev, uint32_t rate) {
	return 0;
}
int rtlsdr_set_center_freq_mocked(rtlsdr_dev_t *dev, uint32_t freq) {
	return 0;
}
int rtlsdr_set_tuner_gain_mode_mocked(rtlsdr_dev_t *dev, int manual) {
	return 0;
}
int rtlsdr_set_tuner_gain_mocked(rtlsdr_dev_t *dev, int gain) {
	return 0;
}
int rtlsdr_set_bias_tee_mocked(rtlsdr_dev_t *dev, int on) {
	return 0;
}
int rtlsdr_reset_buffer_mocked(rtlsdr_dev_t *dev) {
	return 0;
}

