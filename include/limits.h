#ifndef _LIMITS_H
#define _LIMITS_H

#define  CHAR_BIT    8
#define SCHAR_MIN   -0x80
#define SCHAR_MAX    0x7f
#define UCHAR_MAX    0xff
#define  CHAR_MIN    SCHAR_MIN
#define  CHAR_MAX    SCHAR_MAX

#define  SHRT_MIN   -0x8000
#define  SHRT_MAX    0x7fff
#define USHRT_MAX    0xffff

#define  WORD_BIT    32
#define  INT_MIN   (-0x7fffffff - 1)
#define  INT_MAX     0x7fffffff
#define UINT_MAX     0xffffffffu

#define  LONG_BIT    64
#define  LONG_MIN  (-0x7fffffffffffffffl - 1l)
#define  LONG_MAX    0x7fffffffffffffffl
#define ULONG_MAX    0xfffffffffffffffful

#define  LLONG_MIN (-0x7fffffffffffffffll - 1ll)
#define  LLONG_MAX   0x7fffffffffffffffll
#define ULLONG_MAX   0xffffffffffffffffull

#define SSIZE_MAX    LONG_MAX

#define MB_LEN_MAX   4

#endif
