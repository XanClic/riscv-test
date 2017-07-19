typedef unsigned char uint8_t;
typedef unsigned int  uint32_t;


struct htif_register {
    uint32_t lo;
    uint32_t hi;
} __attribute__((packed));

volatile struct htif_register __attribute__((section("htif"))) tohost, fromhost;


void htif_write_to_host(uint8_t device, uint8_t function, uint64_t payload)
{
    tohost.lo = payload & 0xffffffffu;
    tohost.hi = ((uint32_t)device << 24) | ((uint32_t)function << 16) | ((payload >> 32) & 0xffff);
}

void htif_clear_from_host(void)
{
    fromhost.lo = 0;
    fromhost.hi = 0;
}


void putchar(uint8_t c)
{
    if (c == '\n') {
        putchar('\r');
    }

    /* device 1 (console), function 1 (put string) */
    htif_write_to_host(1, 1, c);

    /* acknowledge result */
    htif_clear_from_host();
}


void puts(const char *str)
{
    while (*str) {
        putchar(*(str++));
    }
    putchar('\n');
}


void main(void)
{
    puts("Hello, RISC-V world!");
}
