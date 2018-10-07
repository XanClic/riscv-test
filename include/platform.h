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

    uint32_t *(*framebuffer)(void);
    int (*fb_width)(void);
    int (*fb_height)(void);
    size_t (*fb_stride)(void);
    bool (*fb_flush)(int x, int y, int w, int h);
} PlatformFuncs;


void init_platform(void);
PlatformFuncs *platform_funcs(void);

#endif
