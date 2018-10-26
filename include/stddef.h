#ifndef _STDDEF_H
#define _STDDEF_H

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef unsigned long size_t;
typedef   signed long ssize_t;

#ifndef offsetof
#define offsetof(t, m) ((size_t)(&(((t *)0)->m)))
#endif

#endif
