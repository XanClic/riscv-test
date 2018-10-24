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

    bool (*get_keyboard_event)(int *key, bool *up);
    bool (*get_mouse_event)(int *dx, int *dy, bool *has_button, int *button,
                            bool *button_up);

    // completed() must be quick to return and may not call any audio
    // function
    bool (*queue_audio_track)(const int16_t *buffer, size_t samples,
                              void (*completed)(void));
    void (*handle_audio)(void);
} PlatformFuncs;


extern PlatformFuncs platform_funcs;


void init_platform(void);

#endif
