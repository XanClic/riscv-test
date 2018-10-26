#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>


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


#define stdout ((FILE *)NULL)
#define stderr ((FILE *)NULL)


void stdio_add_inode(const char *name, const void *base, size_t size);

FILE *fopen(const char *pathname, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int fflush(FILE *stream);

// non-standard
void putchar(uint8_t c);
void puts(const char *str);

int printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
int fprintf(FILE *stream, const char *format, ...) \
        __attribute__((format(printf, 2, 3)));
int snprintf(char *dest, size_t n, const char *format, ...) \
        __attribute__((format(printf, 3, 4)));
int vprintf(const char *format, va_list ap);
int vsnprintf(char *dest, size_t n, const char *format, va_list ap);


#endif
