#include <rtl-sdr.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>

#define rtlsdr_read_async rtlsdr_read_async_mocked
#define rtlsdr_close rtlsdr_close_mocked
#define rtlsdr_open rtlsdr_open_mocked
#define rtlsdr_set_sample_rate rtlsdr_set_sample_rate_mocked
#define rtlsdr_set_center_freq rtlsdr_set_center_freq_mocked
#define rtlsdr_set_tuner_gain_mode rtlsdr_set_tuner_gain_mode_mocked
#define rtlsdr_set_tuner_gain rtlsdr_set_tuner_gain_mocked
#define rtlsdr_set_bias_tee rtlsdr_set_bias_tee_mocked
#define rtlsdr_reset_buffer rtlsdr_reset_buffer_mocked
#define rtlsdr_get_tuner_gains rtlsdr_get_tuner_gains_mocked
#define rtlsdr_cancel_async rtlsdr_cancel_async_mocked

int rtlsdr_cancel_async_mocked(rtlsdr_dev_t *dev);
int rtlsdr_read_async_mocked(rtlsdr_dev_t *dev, rtlsdr_read_async_cb_t cb, void *ctx, uint32_t buf_num, uint32_t buf_len);
int rtlsdr_close_mocked(rtlsdr_dev_t *dev);
int rtlsdr_open_mocked(rtlsdr_dev_t **dev, uint32_t index);
int rtlsdr_set_sample_rate_mocked(rtlsdr_dev_t *dev, uint32_t rate);
int rtlsdr_set_center_freq_mocked(rtlsdr_dev_t *dev, uint32_t freq);
int rtlsdr_set_tuner_gain_mode_mocked(rtlsdr_dev_t *dev, int manual);
int rtlsdr_set_tuner_gain_mocked(rtlsdr_dev_t *dev, int gain);
int rtlsdr_set_bias_tee_mocked(rtlsdr_dev_t *dev, int on);
int rtlsdr_reset_buffer_mocked(rtlsdr_dev_t *dev);
int rtlsdr_get_tuner_gains(rtlsdr_dev_t *dev, int *gains);

#include "../src/core.c"

#undef rtlsdr_cancel_async
#undef rtlsdr_read_async
#undef rtlsdr_close
#undef rtlsdr_open
#undef rtlsdr_set_sample_rate
#undef rtlsdr_set_center_freq
#undef rtlsdr_set_tuner_gain_mode
#undef rtlsdr_set_tuner_gain
#undef rtlsdr_set_bias_tee
#undef rtlsdr_reset_buffer
#undef rtlsdr_get_tuner_gains

struct mock_status {
	uint8_t *buffer;
	int len;

	pthread_mutex_t mutex;
	pthread_cond_t condition;
	int stopped;
	int data_was_read;
};

struct rtlsdr_dev {
	int dummy;
};

struct mock_status mock;

void init_mock_librtlsdr() {
	mock.mutex = (pthread_mutex_t )PTHREAD_MUTEX_INITIALIZER;
	mock.condition = (pthread_cond_t )PTHREAD_COND_INITIALIZER;
	mock.stopped = false;
	mock.data_was_read = false;
	mock.buffer = NULL;
}

// make sure data was read
// the core will be terminated gracefully
// so the data will be processed
void wait_for_data_read() {
	pthread_mutex_lock(&mock.mutex);
	while (!mock.data_was_read) {
		pthread_cond_wait(&mock.condition, &mock.mutex);
	}
	pthread_cond_broadcast(&mock.condition);
	pthread_mutex_unlock(&mock.mutex);
}

void setup_mock_data(uint8_t *buffer, int len) {
	pthread_mutex_lock(&mock.mutex);
	mock.buffer = buffer;
	mock.len = len;
	pthread_cond_broadcast(&mock.condition);
	pthread_mutex_unlock(&mock.mutex);
}

void stop_rtlsdr_mock() {
	pthread_mutex_lock(&mock.mutex);
	mock.stopped = true;
	mock.buffer = NULL;
	pthread_cond_broadcast(&mock.condition);
	pthread_mutex_unlock(&mock.mutex);
}


int rtlsdr_read_async_mocked(rtlsdr_dev_t *dev, rtlsdr_read_async_cb_t cb, void *ctx, uint32_t buf_num, uint32_t buf_len) {
	pthread_mutex_lock(&mock.mutex);
	while (!mock.stopped) {
		if (mock.buffer == NULL) {
			pthread_cond_wait(&mock.condition, &mock.mutex);
		}
		if (mock.buffer != NULL) {
			cb(mock.buffer, mock.len, ctx);
			mock.buffer = NULL;
			mock.data_was_read = true;
			pthread_cond_broadcast(&mock.condition);
		}
	}
	pthread_mutex_unlock(&mock.mutex);
	return 0;
}

int rtlsdr_close_mocked(rtlsdr_dev_t *dev) {
	if (dev != NULL) {
		free(dev);
	}
	return 0;
}

int rtlsdr_cancel_async_mocked(rtlsdr_dev_t *dev) {
	stop_rtlsdr_mock();
	return 0;
}

int rtlsdr_open_mocked(rtlsdr_dev_t **dev, uint32_t index) {
	mock.stopped = false;
	mock.data_was_read = false;
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
int rtlsdr_get_tuner_gains_mocked(rtlsdr_dev_t *dev, int *gains) {
	if (gains == NULL) {
		return 1;
	}
	gains[0] = 43;
	return 1;
}

