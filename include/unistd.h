#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_END 1
#define SEEK_CUR 2
#endif


void *sbrk(ptrdiff_t sz);

int open(const char *path, int mode, ...);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);

#endif
