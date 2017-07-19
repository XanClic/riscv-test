typedef unsigned char uint8_t;
typedef unsigned int  uint32_t;
typedef unsigned long uint64_t;

typedef unsigned long uintptr_t;
typedef unsigned long size_t;

typedef unsigned long base_int_t;


typedef __builtin_va_list va_list;

#define va_start(v, l) __builtin_va_start(v,l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, l)   __builtin_va_arg(v,l)
#define va_copy(d, s)  __builtin_va_copy(d,s)


#define bool _Bool
#define false 0
#define true  1
#define __bool_true_false_are_defined 1


#define assert(x) do { } while (0)


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


void putu(unsigned long long x, int base)
{
    char buffer[sizeof(x) * 8];
    int bi = sizeof(buffer);

    assert(base >= 2 && base <= 36);

    if (!x) {
        putchar('0');
    } else {
        while (x) {
            int digit = x % base;
            buffer[--bi] = digit < 10 ? digit + '0' : digit - 10 + 'a';
            x /= base;

            assert(bi >= 0);
        }

        while (bi < (int)sizeof(buffer)) {
            putchar(buffer[bi++]);
        }
    }
}


void puti(long long x, int base)
{
    if (x < 0) {
        putchar('-');
        x = -x;
    }
    putu(x, base);
}


void __attribute__((format(printf, 1, 2))) kprintf(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);

    while (*format) {
        if (*format != '%') {
            putchar(*(format++));
        } else {
            bool unhandled = false;
            const char *fbase = format + 1;

            format++;

            switch (*(format++)) {
                case '%':
                    putchar('%');
                    break;

                case 's': {
                    const char *s = va_arg(ap, const char *);
                    while (*s) {
                        putchar(*(s++));
                    }
                    break;
                }

                case 'i':
                    puti(va_arg(ap, int), 10);
                    break;

                case 'z':
                    switch (*(format++)) {
                        case 'u':
                            putu(va_arg(ap, size_t), 10);
                            break;

                        case 'x':
                            putchar('0');
                            putchar('x');
                            putu(va_arg(ap, size_t), 16);
                            break;

                        default:
                            unhandled = true;
                            break;
                    }
                    break;

                case 'p':
                    putchar('0');
                    putchar('x');
                    putu((uintptr_t)va_arg(ap, void *), 16);
                    break;

                default:
                    unhandled = true;
                    break;
            }

            if (unhandled) {
                putchar('%');
                format = fbase;
            }
        }
    }

    va_end(ap);
}


static inline base_int_t read_csr(unsigned index)
{
    base_int_t result;
    __asm__ __volatile__ ("csrr %0, %1" : "=r"(result) : "i"(index));
    return result;
}


void main(void)
{
    puts("Hello, RISC-V world!");

    base_int_t misa = read_csr(0x301);
    int mxl = misa >> (sizeof(base_int_t) * 8 - 2);

    kprintf("CPU model: RV%i", 16 << mxl);
    for (int i = 0; i < 26; i++) {
        if (misa & (1 << i)) {
            putchar(i + 'A');
        }
    }
    putchar('\n');
}
