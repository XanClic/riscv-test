#ifndef _OGG_VORBIS_H
#define _OGG_VORBIS_H

#include <stdbool.h>
#include <stdint.h>

bool load_ogg_vorbis(const char *fname, int16_t **samples,
                     int64_t *frame_count, int *frame_rate, int *channels);

#endif
