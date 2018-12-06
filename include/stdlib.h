#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>
#include <stdio.h>


#ifdef __GNUC__
#define alloca(size) __builtin_alloca(size)
#endif

#ifndef __quote
#define __squote(arg) #arg
#define __quote(arg) __squote(arg)
#endif

#define abort() \
    do { \
        printf(__FILE__ ":" __quote(__LINE__) ": %s(): Aborted\n", __func__); \
        _abort(); \
    } while (0)


void *calloc(size_t nmemb, size_t size);
void *malloc(size_t sz);
void *memalign(size_t alignment, size_t size);
void *realloc(void *ptr, size_t sz);
void free(void *ptr);

_Noreturn void _abort(void);

int rand(void);

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

void shuffle(void *base, size_t nmemb, size_t size);

static inline int abs(int j)
{
    return j < 0 ? -j : j;
}

static inline long labs(long j)
{
    return j < 0 ? -j : j;
}

#endif
