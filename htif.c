#include <htif.h>
#include <stdint.h>


struct htif_register {
    uint32_t lo;
    uint32_t hi;
} __attribute__((packed));

volatile struct htif_register __attribute__((section("htif"))) tohost, fromhost;


void htif_write_to_host(uint8_t device, uint8_t function, uint64_t payload)
{
    tohost.lo = payload & UINT32_C(0xffffffff);
    tohost.hi = ((uint32_t)device << 24) | ((uint32_t)function << 16) | ((payload >> 32) & 0xffff);
}

void htif_clear_from_host(void)
{
    fromhost.lo = 0;
    fromhost.hi = 0;
}
