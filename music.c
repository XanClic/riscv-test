#include <music.h>
#include <platform.h>
#include <stddef.h>


static int16_t music_track[8000 * 10];
static uint64_t track_resume_at = -1;


static void track_complete(void);

void init_music(void)
{
    int16_t sample = 0;

    for (int i = 0; i < (int)ARRAY_SIZE(music_track); i++) {
        music_track[i] = sample;
        sample += 0x1000;
    }

    platform_funcs.queue_audio_track(music_track, ARRAY_SIZE(music_track),
                                     track_complete);
}

void handle_music(void)
{
    if (platform_funcs.elapsed_us() < track_resume_at) {
        return;
    }

    platform_funcs.queue_audio_track(music_track, ARRAY_SIZE(music_track),
                                     track_complete);
    track_resume_at = -1;
}


static void track_complete(void)
{
    track_resume_at = platform_funcs.elapsed_us() + 1000000;
}
