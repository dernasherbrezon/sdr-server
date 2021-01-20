#include <rtl-sdr.h>
#include <pthread.h>
#include <stdio.h>

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
	pthread_mutex_t mutex;
	pthread_cond_t condition;
	int stopped;
};

struct rtlsdr_dev {
	int dummy;
};

struct mock_status mock;

void init_mock_librtlsdr() {
	mock.mutex = (pthread_mutex_t )PTHREAD_MUTEX_INITIALIZER;
	mock.condition = (pthread_cond_t )PTHREAD_COND_INITIALIZER;
	mock.stopped = 0;
}

int rtlsdr_read_sync_mocked(rtlsdr_dev_t *dev, void *buf, int len, int *n_read) {
	int result;
	pthread_mutex_lock(&mock.mutex);
	if (mock.stopped == 1) {
		return -1;
	}
	if (mock.buffer != NULL) {
		buf = mock.buffer;
		*n_read = mock.len;
		mock.buffer = NULL;
		result = 0;
	} else {
		*n_read = 0;
		printf("mock is waiting for data\n");
		pthread_cond_wait(&mock.condition, &mock.mutex);
		result = -1;
	}
	pthread_mutex_unlock(&mock.mutex);
	return result;
}

int rtlsdr_close_mocked(rtlsdr_dev_t *dev) {
	if (dev != NULL) {
		free(dev);
	}
	pthread_mutex_lock(&mock.mutex);
	mock.stopped = 1;
	pthread_cond_broadcast(&mock.condition);
	pthread_mutex_unlock(&mock.mutex);
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

