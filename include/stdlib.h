#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>


#ifdef __GNUC__
#define alloca(size) __builtin_alloca(size)
#endif


void *calloc(size_t nmemb, size_t size);
void *malloc(size_t sz);
void *memalign(size_t alignment, size_t size);
void *realloc(void *ptr, size_t sz);
void free(void *ptr);

_Noreturn void abort(void);

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

static inline int abs(int j)
{
    return j < 0 ? -j : j;
}

static inline long labs(long j)
{
    return j < 0 ? -j : j;
}

#endif
