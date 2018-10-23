#ifndef _PLATFORM_VIRT_H
#define _PLATFORM_VIRT_H

#include <stdbool.h>


enum VirtPlatformBaseAddresses {
    VPBA_SIFIVE_CLINT   = 0x02000000ul,
    VPBA_UART_BASE      = 0x10000000ul,
    VPBA_VIRTIO_BASE    = 0x10001000ul,
};


bool init_platform_virt(void);

#endif
