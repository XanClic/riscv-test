#include <config.h>
#include <kmalloc.h>
#include <stdint.h>


extern const void __kernel_end;
static uintptr_t heap_end;

#ifndef ROUND_UP
#define ROUND_UP(x, y) (((x) + (y) - 1) & -(y))
#endif


void *kmalloc(size_t sz)
{
    if (!heap_end) {
        heap_end = (uintptr_t)&__kernel_end;
    }

    uintptr_t res = ROUND_UP(heap_end, PAGESIZE);
    heap_end = res + sz;

    return (void *)res;
}
