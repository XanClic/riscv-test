#ifndef _PLATFORM_H
#define _PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef enum PlatformType {
    PLATFORM_SPIKE,
    PLATFORM_VIRT,
} PlatformType;


typedef struct PlatformFuncs {
    void (*putchar)(uint8_t c);

    uint64_t (*elapsed_us)(void);

    uint32_t *(*framebuffer)(void);
    int (*fb_width)(void);
    int (*fb_height)(void);
    size_t (*fb_stride)(void);
    void (*fb_flush)(int x, int y, int w, int h);

    bool (*setup_cursor)(uint32_t *data, int w, int h, int hot_x, int hot_y);
    void (*move_cursor)(int x, int y);

    bool (*need_cursor_updates)(void);

    void (*limit_pointing_device)(int width, int height);

    bool (*get_keyboard_event)(int *key, bool *up);
    bool (*get_pointing_event)(int *x, int *y, bool *has_button, int *button,
                               bool *button_up);

    bool (*has_absolute_pointing_device)(void);

    // completed() must be quick to return and may not call any audio
    // function.
    // @frame_rate and @channels of the first track queued determine
    // what rate and channel count to use from then on; for the
    // remaining tracks, we just see whether they match.  If they do
    // not, the track is not queued.
    bool (*queue_audio_track)(const int16_t *buffer, size_t frames,
                              int frame_rate, int channels,
                              void (*completed)(void));
    void (*handle_audio)(void);
} PlatformFuncs;


extern PlatformFuncs platform_funcs;


void init_platform(void);

#endif
