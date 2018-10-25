#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>


struct Inode {
    const char *name;
    const void *base;
    size_t size;
};

typedef struct {
    const struct Inode *inode;
    long loc;
} FILE;


enum {
    SEEK_SET,
    SEEK_END,
    SEEK_CUR,
};


void stdio_add_inode(const char *name, const void *base, size_t size);

FILE *fopen(const char *pathname, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);

#endif
