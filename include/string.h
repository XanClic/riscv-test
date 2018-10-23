#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>
#include <stdint.h>


#ifndef NULL
#define NULL ((void *)0)
#endif


void *memset(void *s, int c, size_t n);
void *memset32(void *s, uint32_t c, size_t n);

#endif
