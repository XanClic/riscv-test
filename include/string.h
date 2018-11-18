#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>
#include <stdint.h>


#ifndef NULL
#define NULL ((void *)0)
#endif


char *strcat(char *dest, const char *src);
char *strchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
size_t strcspn(const char *s, const char *reject);
size_t strlen(const char *s);
int strncmp(const char *s1, const char *s2, size_t n);

void *memchr(const void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *s, int c, size_t n);
void *memset32(void *s, uint32_t c, size_t n);

const char *strerror(int errnum);

#endif
