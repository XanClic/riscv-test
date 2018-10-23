#include <string.h>


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
