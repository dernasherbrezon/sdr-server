#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdint.h>

struct server_config {
	char* bind_address;
	int port;
	int gain_mode;
	int gain;
	int ppm;
	int bias_t;
	uint32_t buffer_size;
	uint32_t band_sampling_rate;
	char *base_path;
};

int create_server_config(struct server_config **config, const char *path);

void destroy_server_config(struct server_config *config);

#endif /* CONFIG_H_ */
