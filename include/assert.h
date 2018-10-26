#ifdef assert
#undef assert
#endif

#ifndef NDEBUG

#include <stdio.h>


#ifndef __quote
#define __squote(arg) #arg
#define __quote(arg) __squote(arg)
#endif

#define assert(assertion) \
    do { \
        if (!(assertion)) { \
            printf(__FILE__ ":" __quote(__LINE__) ": %s(): " \
                    "Assertion '" #assertion "' failed.\n", \
                    __func__); \
            for (;;); \
        } \
    } while (0)

#else

#define assert(assertion) do { } while (0)

#endif


#ifndef static_assert
#define static_assert _Static_assert
#endif
