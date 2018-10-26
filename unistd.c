#include <errno.h>
#include <sys/types.h>
#include <unistd.h>


int open(const char *path, int mode, ...)
{
    (void)path;
    (void)mode;

    errno = ENOENT;
    return -1;
}


int close(int fd)
{
    (void)fd;

    errno = EBADF;
    return -1;
}


ssize_t read(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;

    errno = EBADF;
    return -1;
}


ssize_t write(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;

    errno = EBADF;
    return -1;
}


off_t lseek(int fd, off_t offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;

    errno = EBADF;
    return -1;
}
