#include <ivorbisfile.h>
#include <limits.h>
#include <music.h>
#include <platform.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


extern const void _binary_music_ogg_start, _binary_music_ogg_size;

static uint64_t track_resume_at = -1;
static int64_t music_sample_count;
static int16_t *music_samples;
static int sample_rate, channels;


static void track_complete(void);

void init_music(void)
{
    stdio_add_inode("/music.ogg", &_binary_music_ogg_start,
                    (size_t)&_binary_music_ogg_size);

    FILE *fp = fopen("/music.ogg", "rb");
    if (!fp) {
        puts("[music] Failed to find /music.ogg");
        return;
    }

    OggVorbis_File ovf;
    if (ov_open(fp, &ovf, NULL, 0) < 0) {
        puts("[music] Failed to load /music.ogg");
        fclose(fp);
        return;
    }

    music_sample_count = ov_pcm_total(&ovf, -1);
    if (music_sample_count < 0) {
        puts("[music] Stream is unseekable");
        return;
    } else if (music_sample_count > INT_MAX / 2) {
        puts("[music] Music track is too long");
        return;
    }

    music_samples = malloc(music_sample_count * sizeof(int16_t));

    vorbis_info *vi = ov_info(&ovf, -1);
    sample_rate = vi->rate;
    channels = vi->channels;

    int remaining = music_sample_count * 2;
    char *target = (char *)music_samples;
    int bitstream = 0;
    while (remaining) {
        int read = ov_read(&ovf, target, remaining, &bitstream);

        if (read <= 0) {
            music_sample_count -= remaining / 2;
            break;
        }

        remaining -= read;
        target += read;
    }

    platform_funcs.queue_audio_track(music_samples, music_sample_count,
                                     sample_rate, channels, track_complete);
}

void handle_music(void)
{
    if (platform_funcs.elapsed_us() < track_resume_at) {
        return;
    }

    platform_funcs.queue_audio_track(music_samples, music_sample_count,
                                     sample_rate, channels, track_complete);
    track_resume_at = -1;
}


static void track_complete(void)
{
    track_resume_at = platform_funcs.elapsed_us() + 1000000;
}
