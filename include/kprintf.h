#ifndef _KPRINTF_H
#define _KPRINTF_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>


void putchar(uint8_t c);
void puts(const char *str);

void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
void ksnprintf(char *dest, size_t n, const char *format, ...) \
         __attribute__((format(printf, 3, 4)));
void kvprintf(const char *format, va_list ap);

#endif
