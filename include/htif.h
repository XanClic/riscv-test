#ifndef _HTIF_H
#define _HTIF_H

#include <stdint.h>


enum HTIFDevice {
    /* HTIF_FRONTEND_SYSCALL_HANDLER, */
    HTIF_CONSOLE = 1,
};

enum HTIFConsoleFunction {
    HTIF_CON_READ,
    HTIF_CON_WRITE,
};


void htif_write_to_host(uint8_t device, uint8_t function, uint64_t payload);
void htif_clear_from_host(void);

#endif
