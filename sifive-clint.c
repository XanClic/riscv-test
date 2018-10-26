#include <platform.h>
#include <sifive-clint.h>
#include <stdint.h>
#include <stdio.h>


static uintptr_t base;

static uint64_t elapsed_us(void);

void init_sifive_clint(uintptr_t b)
{
    if (base) {
        puts("[sifive-clint] Ignoring second CLINT");
        return;
    }

    printf("[sifive-clint] Found CLINT @%p\n", (void *)b);

    base = b;

    platform_funcs.elapsed_us = elapsed_us;
}


static uint64_t elapsed_us(void)
{
    uint32_t lo = *(uint32_t *)(base + 0xbff8);
    uint32_t hi = *(uint32_t *)(base + 0xbffc);

    return (((uint64_t)hi << 32) | lo) / 10;
}
