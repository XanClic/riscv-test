#ifndef _PLATFORM_H
#define _PLATFORM_H

#include <stdint.h>


typedef enum PlatformType {
    PLATFORM_SPIKE,
    PLATFORM_VIRT,
} PlatformType;


typedef struct PlatformFuncs {
    void (*putchar)(uint8_t c);
} PlatformFuncs;


void init_platform(void);
PlatformFuncs *platform_funcs(void);

#endif
