typedef unsigned char uint8_t;
typedef unsigned int  uint32_t;


struct htif_register {
    uint32_t lo;
    uint32_t hi;
} __attribute__((packed));

volatile struct htif_register __attribute__((section("htif"))) tohost, fromhost;


void putchar(uint8_t c)
{
    tohost.lo = c;
    /* device 1 (serial), function 1 (put string) */
    tohost.hi = (1 << 24) | (1 << 16);

    /* acknowledge result */
    fromhost.lo = 0;
    fromhost.hi = 0;
}


void puts(const char *str)
{
    while (*str) {
        putchar(*(str++));
    }
}


void main(void)
{
    puts("Hello, RISC-V world!\r\n");
}
