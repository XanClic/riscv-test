#include <config.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>


extern const void __kernel_end;
static uintptr_t heap_end;

#ifndef ROUND_UP
#define ROUND_UP(x, y) (((x) + (y) - 1) & -(y))
#endif


void *sbrk(ptrdiff_t sz)
{
    if (!heap_end) {
        heap_end = ROUND_UP((uintptr_t)&__kernel_end, PAGESIZE);
    }

    void *ptr = (void *)heap_end;
    heap_end += sz;
    return ptr;
}
