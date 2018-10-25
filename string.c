#include <string.h>


char *strcat(char *dest, const char *src)
{
    char *dc = dest;

    while (*dest) {
        dest++;
    }
    while (*src) {
        *(dest++) = *(src++);
    }
    *dest = 0;

    return dc;
}


int strcmp(const char *s1, const char *s2)
{
    int diff = 0;

    while (*s1 && (diff = *(s1++) - *(s2++)) == 0);

    return diff;
}


char *strcpy(char *dest, const char *src)
{
    char *od = dest;

    while (*src) {
        *(dest++) = *(src++);
    }
    *(dest++) = 0;

    return od;
}


size_t strlen(const char *s)
{
    size_t len = 0;
    while (*(s++)) {
        len++;
    }
    return len;
}


void *memset(void *s, int c, size_t n)
{
    char *p = s;
    while (n--) {
        *(p++) = c;
    }
    return s;
}


void *memset32(void *s, uint32_t c, size_t n)
{
    uint32_t *p = s;
    while (n--) {
        *(p++) = c;
    }
    return s;
}


void *memcpy(void *dest, const void *src, size_t n)
{
    char *d8 = dest;
    const char *s8 = src;

    while (n--) {
        *(d8++) = *(s8++);
    }

    return dest;
}


void *memmove(void *d, const void *s, size_t n)
{
    const char *s8 = s;
    char *d8 = d;

    if (((uintptr_t)d < (uintptr_t)s) || ((uintptr_t)s + n <= (uintptr_t)d)) {
        while (n--) {
            *(d8++) = *(s8++);
        }
    } else {
        while (n--) {
            d8[n] = s8[n];
        }
    }

    return d;
}


int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *s18 = s1, *s28 = s2;
    int diff = 0;

    while (n-- && (diff = *(s18++) - *(s28++)) == 0);

    return diff;
}


void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *s8 = s;

    while (n && *s8 != c) {
        s8++;
        n--;
    }

    return n ? (void *)s8 : NULL;
}
