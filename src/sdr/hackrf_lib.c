#include "hackrf_lib.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define SETUP_FUNCTION(result, name)                        \
  do {                                                      \
    result->name = hackrf_lib_dlsym(result->handle, #name); \
    if (result->name == NULL) {                             \
      hackrf_lib_destroy(result);                           \
      return -1;                                            \
    }                                                       \
  } while (0)

void *hackrf_lib_dlsym(void *handle, const char *symbol) {
  void *result = dlsym(handle, symbol);
  if (result == NULL) {
    fprintf(stderr, "unable to load function %s: %s\n", symbol, dlerror());
  }
  return result;
}

int hackrf_lib_create(hackrf_lib **lib) {
  struct hackrf_lib_t *result = malloc(sizeof(struct hackrf_lib_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct hackrf_lib_t){0};
#if defined(__APPLE__)
  result->handle = dlopen("libhackrf.dylib", RTLD_LAZY);
#else
  result->handle = dlopen("libhackrf.so", RTLD_LAZY);
#endif
  if (!result->handle) {
    fprintf(stderr, "<3>unable to load hackrf lib: %s\n", dlerror());
    hackrf_lib_destroy(result);
    return -1;
  }

  SETUP_FUNCTION(result, hackrf_init);
  SETUP_FUNCTION(result, hackrf_error_name);
  SETUP_FUNCTION(result, hackrf_version_string_read);
  SETUP_FUNCTION(result, hackrf_usb_api_version_read);
  SETUP_FUNCTION(result, hackrf_set_vga_gain);
  SETUP_FUNCTION(result, hackrf_set_lna_gain);
  SETUP_FUNCTION(result, hackrf_set_amp_enable);
  SETUP_FUNCTION(result, hackrf_set_freq);
  SETUP_FUNCTION(result, hackrf_set_baseband_filter_bandwidth);
  SETUP_FUNCTION(result, hackrf_set_sample_rate);
  SETUP_FUNCTION(result, hackrf_start_rx);
  SETUP_FUNCTION(result, hackrf_stop_rx);
  SETUP_FUNCTION(result, hackrf_close);
  SETUP_FUNCTION(result, hackrf_exit);
  SETUP_FUNCTION(result, hackrf_open);
  SETUP_FUNCTION(result, hackrf_open_by_serial);
  SETUP_FUNCTION(result, hackrf_set_antenna_enable);

  *lib = result;
  return 0;
}

void hackrf_lib_destroy(hackrf_lib *lib) {
  if (lib == NULL) {
    return;
  }
  if (lib->handle != NULL) {
    dlclose(lib->handle);
  }
  free(lib);
}