#include "png_util.h"
#include <stdlib.h>
#include <errno.h>

int png_util_init(uint32_t width, uint32_t height, FILE *fp, png_util **png) {
  png_util *result = malloc(sizeof(png_util));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (png_util){0};
  result->width = width;
  result->height = height;
  result->fp = fp;

  result->png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (result->png_ptr == NULL) {
    fprintf(stderr, "unable to initialize png\n");
    png_util_destroy(result);
    return -ENOMEM;
  }

  result->info_ptr = png_create_info_struct(result->png_ptr);
  if (result->info_ptr == NULL) {
    fprintf(stderr, "unable to initialize png info\n");
    png_util_destroy(result);
    return -ENOMEM;
  }

  int code = setjmp(png_jmpbuf(result->png_ptr));
  if (code != 0) {
    fprintf(stderr, "unable to setjmp\n");
    png_util_destroy(result);
    return code;
  }

  png_set_IHDR(result->png_ptr,
               result->info_ptr,
               width,
               result->height,
               8,
               PNG_COLOR_TYPE_GRAY,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  result->row = (png_byte *) malloc(png_get_rowbytes(result->png_ptr, result->info_ptr));
  if (result->row == NULL) {
    fprintf(stderr, "unable to initialize temp buffer for png row\n");
    png_util_destroy(result);
    return -ENOMEM;
  }

  png_init_io(result->png_ptr, fp);
  png_write_info(result->png_ptr, result->info_ptr);

  *png = result;
  return 0;
}

void png_util_set_data(const float *data, png_util *png) {
  for (uint32_t i = 0; i < png->width; i++) {
    int pixel = (int) (data[i] + 255);
    if (pixel > 255) {
      pixel = 255;
    }
    if (pixel < 0) {
      pixel = 0;
    }
    png->row[i] = pixel;
  }
  png_write_row(png->png_ptr, png->row);
}

void png_util_write_image(png_util *png) {
  png_write_end(png->png_ptr, NULL);
}

void png_util_destroy(png_util *png) {
  if (png == NULL) {
    return;
  }
  if (png->row != NULL) {
    free(png->row);
  }
  if (png->fp != NULL) {
    fclose(png->fp);
  }
  if (png->png_ptr != NULL && png->info_ptr) {
    png_destroy_write_struct(&png->png_ptr, &png->info_ptr);
  }
  free(png);
}
