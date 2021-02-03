#include <check.h>
#include <stdlib.h>
#include "utils.h"

void setup_input_data(uint8_t **input, size_t input_offset, size_t len) {
	uint8_t *result = malloc(sizeof(uint8_t) * len);
	ck_assert(result != NULL);
	for (size_t i = 0; i < len; i++) {
		// don't care about the loss of data
		result[i] = (uint8_t) (input_offset + i);
	}
	*input = result;
}
