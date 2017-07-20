#ifndef _CPU_H
#define _CPU_H

#include <stddef.h>


typedef size_t base_int_t;


static inline base_int_t read_csr(unsigned index)
{
    base_int_t result;
    __asm__ __volatile__ ("csrr %0, %1" : "=r"(result) : "i"(index));
    return result;
}

#endif
