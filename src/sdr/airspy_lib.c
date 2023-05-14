#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>
#include <inttypes.h>
#include "airspy_lib.h"

#define SETUP_FUNCTION(result, name)           \
  do {                           \
    result->name = airspy_lib_dlsym(result->handle, #name);          \
    if (result->name == NULL) {                \
      airspy_lib_destroy(result);                                         \
      return -1;           \
    }                            \
  } while (0)

void *airspy_lib_dlsym(void *handle, const char *symbol) {
  void *result = dlsym(handle, symbol);
  if (result == NULL) {
    fprintf(stderr, "unable to load function %s: %s\n", symbol, dlerror());
  }
  return result;
}

int airspy_lib_create(airspy_lib **lib) {
  struct airspy_lib_t *result = malloc(sizeof(struct airspy_lib_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct airspy_lib_t) {0};

#if defined(__APPLE__)
  result->handle = dlopen("libairspy", RTLD_LAZY);
#else
  result->handle = dlopen("libairspy.so", RTLD_LAZY);
#endif
  if (!result->handle) {
    fprintf(stderr, "unable to load arispy lib: %s\n", dlerror());
    airspy_lib_destroy(result);
    return -1;
  }
  SETUP_FUNCTION(result, airspy_lib_version);
  SETUP_FUNCTION(result, airspy_open);
  SETUP_FUNCTION(result, airspy_set_sample_type);
  SETUP_FUNCTION(result, airspy_close);
  SETUP_FUNCTION(result, airspy_set_lna_agc);
  SETUP_FUNCTION(result, airspy_set_samplerate);
  SETUP_FUNCTION(result, airspy_set_packing);
  SETUP_FUNCTION(result, airspy_set_rf_bias);
  SETUP_FUNCTION(result, airspy_set_lna_gain);
  SETUP_FUNCTION(result, airspy_set_mixer_agc);
  SETUP_FUNCTION(result, airspy_set_vga_gain);
  SETUP_FUNCTION(result, airspy_set_mixer_gain);
  SETUP_FUNCTION(result, airspy_set_linearity_gain);
  SETUP_FUNCTION(result, airspy_set_sensitivity_gain);
  SETUP_FUNCTION(result, airspy_start_rx);
  SETUP_FUNCTION(result, airspy_set_freq);
  SETUP_FUNCTION(result, airspy_stop_rx);

  airspy_lib_version_t version;
  result->airspy_lib_version(&version);
  fprintf(stdout, "airspy library initialized: %"PRIu32" %"PRIu32, version.major_version, version.minor_version);

  *lib = result;
  return 0;
}

void airspy_lib_destroy(airspy_lib *lib) {
  if (lib->handle != NULL) {
    dlclose(lib->handle);
  }
  free(lib);
}
