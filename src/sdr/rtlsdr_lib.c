#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>

#include "rtlsdr_lib.h"

#define SETUP_FUNCTION(result, name)           \
  do {                           \
    result->name = rtlsdr_lib_dlsym(result->handle, #name);          \
    if (result->name == NULL) {                \
      rtlsdr_lib_destroy(result);                                         \
      return -1;           \
    }                            \
  } while (0)

void *rtlsdr_lib_dlsym(void *handle, const char *symbol) {
  void *result = dlsym(handle, symbol);
  if (result == NULL) {
    fprintf(stderr, "unable to load function %s: %s\n", symbol, dlerror());
  }
  return result;
}

int rtlsdr_lib_create(rtlsdr_lib **lib) {
  struct rtlsdr_lib_t *result = malloc(sizeof(struct rtlsdr_lib_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct rtlsdr_lib_t) {0};

#if defined(__APPLE__)
  result->handle = dlopen("librtlsdr.dylib", RTLD_LAZY);
#else
  result->handle = dlopen("librtlsdr.so", RTLD_LAZY);
#endif
  if (!result->handle) {
    fprintf(stderr, "unable to load librtlsdr: %s\n", dlerror());
    rtlsdr_lib_destroy(result);
    return -1;
  }
  SETUP_FUNCTION(result, rtlsdr_open);
  SETUP_FUNCTION(result, rtlsdr_close);
  SETUP_FUNCTION(result, rtlsdr_set_sample_rate);
  SETUP_FUNCTION(result, rtlsdr_set_center_freq);
  SETUP_FUNCTION(result, rtlsdr_set_tuner_gain_mode);
  SETUP_FUNCTION(result, rtlsdr_get_tuner_gains);
  SETUP_FUNCTION(result, rtlsdr_set_tuner_gain);
  SETUP_FUNCTION(result, rtlsdr_set_bias_tee);
  SETUP_FUNCTION(result, rtlsdr_reset_buffer);
  SETUP_FUNCTION(result, rtlsdr_read_sync);
  SETUP_FUNCTION(result, rtlsdr_set_freq_correction);
  *lib = result;
  return 0;
}

void rtlsdr_lib_destroy(rtlsdr_lib *lib) {
  if (lib->handle != NULL) {
    dlclose(lib->handle);
  }
  free(lib);
}
