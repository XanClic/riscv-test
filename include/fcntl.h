#ifndef _FCNTL_H
#define _FCNTL_H

#include <unistd.h>


#define O_RDONLY    (1 << 0)
#define O_WRONLY    (1 << 1)
#define O_RDWR      (O_RDONLY | O_WRONLY)
#define O_CREAT     (1 << 2)
#define O_TRUNC     (1 << 3)
#define O_APPEND    (1 << 4)

#endif
