#ifndef _IMAGE_H
#define _IMAGE_H

#include <stdbool.h>
#include <stdint.h>

void init_images(void);
bool load_image(const char *name, uint32_t **dest, int *w, int *h, int stride);

#endif