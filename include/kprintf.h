#ifndef _KPRINTF_H
#define _KPRINTF_H

#include <stdint.h>


void putchar(uint8_t c);
void puts(const char *str);

void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
