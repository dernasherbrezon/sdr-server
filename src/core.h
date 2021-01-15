
#ifndef CORE_H_
#define CORE_H_

#include "config.h"

typedef struct core_t core;

struct client_config {
	uint32_t center_freq;
	uint32_t sampling_rate;
	uint32_t band_freq;
	int client_socket;
	uint32_t id;
};

int create_core(struct server_config *server_config, core **result);

int start_rtlsdr(struct client_config *config, core *core);
void stop_rtlsdr(core *core);

int add_client(struct client_config *config, core *core);
void remove_client(struct client_config *config, core *core);

void destroy_core(core *core);


#endif /* CORE_H_ */
