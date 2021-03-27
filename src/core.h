
#ifndef CORE_H_
#define CORE_H_

#include <stdint.h>
#include <signal.h>
#include <stdatomic.h>

#include "config.h"

typedef struct core_t core;

struct client_config {
	uint32_t center_freq;
	uint32_t sampling_rate;
	uint32_t band_freq;
	uint8_t destination;
	int client_socket;
	uint8_t id;
	atomic_bool is_running;
	core *core;
};

int create_core(struct server_config *server_config, core **result);

// client_config contains core to modify
int add_client(struct client_config *config);
void remove_client(struct client_config *config);

void destroy_core(core *core);


#endif /* CORE_H_ */
