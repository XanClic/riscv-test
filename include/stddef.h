#ifndef _STDDEF_H
#define _STDDEF_H

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef unsigned long size_t;

#ifndef offsetof
#define offsetof(t, m) ((size_t)(&(((t *)0)->m)))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#endif
