#include <assert.h>
#include <errno.h>
#include <htif.h>
#include <platform.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <virt-uart.h>


void putchar(uint8_t c)
{
#ifdef SERIAL_IS_SOUND
    (void)c;
#else
    if (c == '\n') {
        putchar('\r');
    }

    platform_funcs.putchar(c);
#endif
}


void puts(const char *str)
{
    while (*str) {
        putchar(*(str++));
    }
    putchar('\n');
}


static int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

static void printf_putu(void (*putfn)(char, void *), void *opaque,
                        unsigned long long x, int base, int fill_width,
                        char fill_chr, const char *prefix)
{
    char buffer[sizeof(x) * 8];
    int bi = sizeof(buffer);

    assert(base >= 2 && base <= 36);

    if (!x) {
        buffer[--bi] = '0';
    } else {
        while (x) {
            int digit = x % base;
            buffer[--bi] = digit < 10 ? digit + '0' : digit - 10 + 'a';
            x /= base;

            assert(bi >= 0);
        }
    }

    fill_width -= (sizeof(buffer) - bi) + strlen(prefix);
    if (fill_width > 0 && !isdigit(fill_chr)) {
        while (fill_width--) {
            putfn(fill_chr, opaque);
        }
    }

    while (*prefix) {
        putfn(*(prefix++), opaque);
    }

    if (fill_width > 0) {
        while (fill_width--) {
            putfn(fill_chr, opaque);
        }
    }

    while (bi < (int)sizeof(buffer)) {
        putfn(buffer[bi++], opaque);
    }
}

static void printf_puti(void (*putfn)(char, void *), void *opaque,
                        long long x, int base, int fill_width, char fill_chr,
                        const char *prefix)
{
    char nprefix[16];

    assert(strlen(prefix) < 15);

    if (x < 0) {
        nprefix[0] = '-';
        strcpy(nprefix + 1, prefix);
        x = -x;
    } else {
        strcpy(nprefix, prefix);
    }

    printf_putu(putfn, opaque, x, base, fill_width, fill_chr, nprefix);
}

// On error, 0 is returned and *@endptr == @nptr.
// On success, *@endptr > @nptr.
unsigned atou(const char *nptr, char **endptr)
{
    unsigned val = 0;

    *endptr = (char *)nptr;

    while (isdigit(*nptr)) {
        unsigned nval = val * 10 + (*(nptr++) - '0');
        if (nval / 10 != val) {
            // Overflow
            return 0;
        }
        val = nval;
    }

    *endptr = (char *)nptr;
    return val;
}

static void vprintf_do(void (*putfn)(char, void *), void *opaque,
                       const char *format, va_list ap)
{
    for (; *format; format++) {
        if (*format != '%') {
            putfn(*format, opaque);
            continue;
        }

        const char *fmt_rewind = format++;
        int fill_width = 0;
        char fill_chr = ' ';
        bool alternative = false;

        for (;;) {
            if (*format == '0') {
                fill_chr = '0';
            } else if (*format == '#') {
                alternative = true;
            } else {
                break;
            }
            format++;
        }

        fill_width = atou(format, (char **)&format);

        switch (*format) {
            case '%':
                putfn('%', opaque);
                break;

            case 'd':
            case 'i':
                printf_puti(putfn, opaque, va_arg(ap, int), 10,
                            fill_width, fill_chr, "");
                break;

            case 'p':
                printf_putu(putfn, opaque, (uintptr_t)va_arg(ap, void *), 16,
                            fill_width, fill_chr, "0x");
                break;

            case 's': {
                const char *s = va_arg(ap, const char *);
                while (*s) {
                    putfn(*(s++), opaque);
                }
                break;
            }

            case 'u':
                printf_putu(putfn, opaque, va_arg(ap, unsigned), 10,
                            fill_width, fill_chr, "");
                break;

            case 'x':
                printf_putu(putfn, opaque, va_arg(ap, unsigned), 16,
                            fill_width, fill_chr, alternative ? "0x" : "");
                break;

            case 'z':
                switch (*(++format)) {
                    case 'd':
                    case 'i':
                        printf_puti(putfn, opaque, va_arg(ap, ssize_t), 10,
                                    fill_width, fill_chr, "");
                        break;

                    case 'u':
                        printf_putu(putfn, opaque, va_arg(ap, size_t), 10,
                                    fill_width, fill_chr, "");
                        break;

                    case 'x':
                        printf_putu(putfn, opaque, va_arg(ap, size_t), 16,
                                    fill_width, fill_chr,
                                     alternative ? "0x" : "");
                        break;

                    default:
                        format = fmt_rewind;
                        putfn('%', opaque);
                }
                break;

            default:
                format = fmt_rewind;
                putfn('%', opaque);
        }
    }
}


static void __putfn_stdout(char c, void *opaque)
{
    (*(int *)opaque)++;
    putchar(c);
}

struct __putfn_buffer_args {
    char *dest;
    size_t remaining;
    int char_count;
};

static void __putfn_buffer(char c, void *opaque)
{
    struct __putfn_buffer_args *args = opaque;

    args->char_count++;

    if (!args->remaining) {
        return;
    } else if (args->remaining == 1) {
        c = 0;
    }

    *(args->dest++) = c;
    args->remaining--;
}

int printf(const char *format, ...)
{
    int char_count;
    va_list ap;

    va_start(ap, format);
    vprintf_do(__putfn_stdout, &char_count, format, ap);
    va_end(ap);

    return char_count;
}

int vprintf(const char *format, va_list ap)
{
    int char_count;
    vprintf_do(__putfn_stdout, &char_count, format, ap);
    return char_count;
}

int fprintf(FILE *stream, const char *format, ...)
{
    int char_count;
    va_list ap;

    if (stream) {
        errno = -EBADF;
        return -1;
    }

    va_start(ap, format);
    vprintf_do(__putfn_stdout, &char_count, format, ap);
    va_end(ap);

    return char_count;
}

int snprintf(char *dest, size_t n, const char *format, ...)
{
    int ret;
    va_list ap;

    va_start(ap, format);
    ret = vsnprintf(dest, n, format, ap);
    va_end(ap);

    return ret;
}

int vsnprintf(char *dest, size_t n, const char *format, va_list ap)
{
    int ret;
    struct __putfn_buffer_args args = {
        .dest = dest,
        .remaining = n
    };

    vprintf_do(__putfn_buffer, &args, format, ap);

    ret = args.char_count;
    __putfn_buffer(0, &args);
    return ret;
}
