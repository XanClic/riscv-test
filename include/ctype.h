#ifndef _CTYPE_H
#define _CTYPE_H

static inline int toupper(int c)
{
    if (c >= 'a' && c <= 'z') {
        return c - 32;
    }
    return c;
}

#endif
