#include <music.h>
#include <ogg-vorbis.h>
#include <platform.h>
#include <stdint.h>


static uint64_t track_resume_at = -1;
static int64_t music_frame_count;
static int16_t *music_samples;
static int frame_rate, channels;


static void track_complete(void);

void init_music(void)
{
    if (!load_ogg_vorbis("/music.ogg", &music_samples, &music_frame_count,
                         &frame_rate, &channels))
    {
        return;
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
