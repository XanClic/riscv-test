#ifndef _PLATFORM_SPIKE_H
#define _PLATFORM_SPIKE_H

#include <stdbool.h>


enum SpikePlatformBaseAddresses {
    SPBA_SIFIVE_CLINT   = 0x02000000ul,
};


bool init_platform_spike(void);

#endif
