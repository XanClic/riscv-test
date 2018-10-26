#include <ivorbisfile.h>
#include <limits.h>
#include <music.h>
#include <platform.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


extern const void _binary_music_ogg_start, _binary_music_ogg_size;

static uint64_t track_resume_at = -1;
static int64_t music_frame_count;
static int16_t *music_samples;
static int frame_rate, channels;


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

    music_frame_count = ov_pcm_total(&ovf, -1);
    if (music_frame_count < 0) {
        puts("[music] Stream is unseekable");
        return;
    } else if (music_frame_count > INT_MAX / 2) {
        puts("[music] Music track is too long");
        return;
    }

    vorbis_info *vi = ov_info(&ovf, -1);
    frame_rate = vi->rate;
    channels = vi->channels;

    music_samples = malloc(music_frame_count * channels * sizeof(int16_t));

    int remaining = music_frame_count * channels * 2;
    char *target = (char *)music_samples;
    int bitstream = 0;
    while (remaining) {
        int read = ov_read(&ovf, target, remaining, &bitstream);

        if (read <= 0) {
            music_frame_count -= remaining / (channels * 2);
            break;
        }

        remaining -= read;
        target += read;
    }

    platform_funcs.queue_audio_track(music_samples, music_frame_count,
                                     frame_rate, channels, track_complete);
}

void handle_music(void)
{
    if (platform_funcs.elapsed_us() < track_resume_at) {
        return;
    }

    platform_funcs.queue_audio_track(music_samples, music_frame_count,
                                     frame_rate, channels, track_complete);
    track_resume_at = -1;
}


static void track_complete(void)
{
    track_resume_at = platform_funcs.elapsed_us() + 1000000;
}
