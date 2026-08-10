#ifndef PNG_UTIL_H_
#define PNG_UTIL_H_

#include <stdint.h>
#include <png.h>
#include <stdio.h>

typedef struct {
  png_structp png_ptr;
  png_infop info_ptr;
  FILE *fp;
  png_bytep row;
  uint32_t width;
  uint32_t height;
} png_util;

int png_util_init(uint32_t width, uint32_t height, FILE *fp, png_util **result);

void png_util_set_data(const float *data, png_util *png);

void png_util_write_image(png_util *png);

void png_util_destroy(png_util *png);

#endif /* PNG_UTIL_H_ */
