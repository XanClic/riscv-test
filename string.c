#include <string.h>


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
