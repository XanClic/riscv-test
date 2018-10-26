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


#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_END 1
#define SEEK_CUR 2
#endif


void stdio_add_inode(const char *name, const void *base, size_t size);

FILE *fopen(const char *pathname, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int fflush(FILE *stream);

#endif
