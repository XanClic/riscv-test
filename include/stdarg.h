#ifndef _STDARG_H
#define _STDARG_H

#define va_start(argptr, last_fixed_arg) __builtin_va_start(argptr, last_fixed_arg)
#define va_arg(argptr, type) __builtin_va_arg(argptr, type)
#define va_copy(dest, src) __builtin_va_copy(dest, src)
#define va_end(argptr) __builtin_va_end(argptr)


typedef __builtin_va_list va_list;

#endif
