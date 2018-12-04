#include <ivorbisfile.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


bool load_ogg_vorbis(const char *fname, int16_t **samples,
                     int64_t *frame_count, int *frame_rate, int *channels)
{
    FILE *fp = fopen(fname, "rb");
    if (!fp) {
        printf("Failed to find %s\n", fname);
        return false;
    }

    OggVorbis_File ovf;
    if (ov_open(fp, &ovf, NULL, 0) < 0) {
        printf("Failed to ov_open() %s\n", fname);
        fclose(fp);
        return false;
    }

    *frame_count = ov_pcm_total(&ovf, -1);
    if (*frame_count < 0) {
        printf("%s: Stream is unseekable\n", fname);
        ov_clear(&ovf);
        return false;
    } else if (*frame_count > INT_MAX / 2) {
        printf("%s: Track is too long\n", fname);
        ov_clear(&ovf);
        return false;
    }

    vorbis_info *vi = ov_info(&ovf, -1);
    *frame_rate = vi->rate;
    *channels = vi->channels;

    *samples = malloc(*frame_count * *channels * sizeof(int16_t));

    int remaining = *frame_count * *channels * 2;
    char *target = (char *)*samples;
    int bitstream = 0;
    while (remaining) {
        int read = ov_read(&ovf, target, remaining, &bitstream);

        if (read <= 0) {
            *frame_count -= remaining / (*channels * sizeof(int16_t));
            break;
        }

        remaining -= read;
        target += read;
    }

    ov_clear(&ovf);
    return true;
}
