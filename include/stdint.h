#ifndef _STDINT_H
#define _STDINT_H

typedef   signed char   int8_t;
typedef unsigned char  uint8_t;
typedef   signed short  int16_t;
typedef unsigned short uint16_t;
typedef   signed int    int32_t;
typedef unsigned int   uint32_t;
typedef   signed long   int64_t;
typedef unsigned long  uint64_t;

typedef  int64_t  intptr_t;
typedef uint64_t uintptr_t;
typedef  int64_t ptrdiff_t;

typedef uint64_t wchar_t;


typedef  int8_t   int_least8_t;
typedef uint8_t  uint_least8_t;
typedef  int16_t  int_least16_t;
typedef uint16_t uint_least16_t;
typedef  int32_t  int_least32_t;
typedef uint32_t uint_least32_t;
typedef  int64_t  int_least64_t;
typedef uint64_t uint_least64_t;


typedef  int64_t  int_fast8_t;
typedef uint64_t uint_fast8_t;
typedef  int64_t  int_fast16_t;
typedef uint64_t uint_fast16_t;
typedef  int64_t  int_fast32_t;
typedef uint64_t uint_fast32_t;
typedef  int64_t  int_fast64_t;
typedef uint64_t uint_fast64_t;


typedef  int64_t  intmax_t;
typedef uint64_t uintmax_t;


#define  INT8_C(x)  x
#define UINT8_C(x)  x ## u
#define  INT16_C(x) x
#define UINT16_C(x) x ## u
#define  INT32_C(x) x
#define UINT32_C(x) x ## u
#define  INT64_C(x) x ## l
#define UINT64_C(x) x ## ul


#define  INT8_MIN  (-0x7f-1)
#define  INT8_MAX  ( 0x7f)
#define UINT8_MAX  ( 0xff)
#define  INT16_MIN (-0x7fff-1)
#define  INT16_MAX ( 0x7fff)
#define UINT16_MAX ( 0xffff)
#define  INT32_MIN (-0x7fffffff-1)
#define  INT32_MAX ( 0x7fffffff)
#define UINT32_MAX ( 0xffffffffu)
#define  INT64_MIN (-0x7fffffffffffffffl-1)
#define  INT64_MAX ( 0x7fffffffffffffffl)
#define UINT64_MAX ( 0xfffffffffffffffful)

#define  INT_LEAST8_MIN   INT8_MIN
#define  INT_LEAST8_MAX   INT8_MAX
#define UINT_LEAST8_MAX  UINT8_MAX
#define  INT_LEAST16_MIN  INT16_MIN
#define  INT_LEAST16_MAX  INT16_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#define  INT_LEAST32_MIN  INT32_MIN
#define  INT_LEAST32_MAX  INT32_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#define  INT_LEAST64_MIN  INT64_MIN
#define  INT_LEAST64_MAX  INT64_MAX
#define UINT_LEAST64_MAX UINT64_MAX

#define  INT_FAST8_MIN   INT64_MIN
#define  INT_FAST8_MAX   INT64_MAX
#define UINT_FAST8_MAX  UINT64_MAX
#define  INT_FAST16_MIN  INT64_MIN
#define  INT_FAST16_MAX  INT64_MAX
#define UINT_FAST16_MAX UINT64_MAX
#define  INT_FAST32_MIN  INT64_MIN
#define  INT_FAST32_MAX  INT64_MAX
#define UINT_FAST32_MAX UINT64_MAX
#define  INT_FAST64_MIN  INT64_MIN
#define  INT_FAST64_MAX  INT64_MAX
#define UINT_FAST64_MAX UINT64_MAX

#define  INTPTR_MIN  INT64_MIN
#define  INTPTR_MAX  INT64_MAX
#define UINTPTR_MAX UINT64_MAX
#define  INTMAX_MIN  INT64_MIN
#define  INTMAX_MAX  INT64_MAX
#define UINTMAX_MAX UINT64_MAX
#define  PTRDIFF_MIN INT64_MIN
#define  PTRDIFF_MAX INT64_MAX

#endif
