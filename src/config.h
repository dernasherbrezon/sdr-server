#ifndef CONFIG_H_
#define CONFIG_H_

#include <unistd.h>

struct server_config {
	int gain_mode;
	int gain;
	int ppm;
	int bias_t;
	uint32_t band_sampling_freq;
};

int create_server_config(struct server_config **config, const char *path);

#endif /* CONFIG_H_ */
