#include <errno.h>
#include <nonstddef.h>


int errno;

static const char *const errstrings[] = {
    [ENOENT]    = "No such file or directory",
    [EBADF]     = "Bad file descriptor",
    [ENOMEM]    = "Out of memory",
    [EINVAL]    = "Invalid argument",
    [EROFS]     = "Read-only file system",
};


const char *strerror(int errnum)
{
    if (errnum >= (int)ARRAY_SIZE(errstrings) || !errstrings[errnum]) {
        return "Unknown error";
    }

    return errstrings[errnum];
}
