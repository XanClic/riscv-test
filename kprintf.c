#include <assert.h>
#include <htif.h>
#include <kprintf.h>
#include <platform.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <virt-uart.h>


void putchar(uint8_t c)
{
    if (c == '\n') {
        putchar('\r');
    }

    platform_funcs()->putchar(c);
}


void puts(const char *str)
{
    while (*str) {
        putchar(*(str++));
    }
    putchar('\n');
}


static void putu(unsigned long long x, int base)
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


static void puti(long long x, int base)
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

                case 'x':
                    putchar('0');
                    putchar('x');
                    putu(va_arg(ap, int), 16);
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
