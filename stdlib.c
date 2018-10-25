#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


_Noreturn void abort(void)
{
    for (;;);
}


#define element(i) \
    (void *)((uintptr_t)base + i * size)

#define compare(i, j) \
    compar(element(i), element(j))

#define swap(i, j) \
    memcpy(tmp, element(i), size); \
    memcpy(element(i), element(j), size); \
    memcpy(element(j), tmp, size);

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
    uint8_t tmp[size];

    if (nmemb < 2) {
        return;
    } else if (nmemb == 2) {
        if (compare(0, 1) > 0) {
            swap(0, 1);
        }
        return;
    }

    size_t pivot = 0, i = 1, j = nmemb - 1;

    while (i < j) {
        while (i < j && compare(i, pivot) < 0) {
            i++;
        }
        while (i < j && compare(pivot, j) < 0) {
            j--;
        }
        if (i < j) {
            swap(i, j);
            i++;
            j--;
        }
    }

    if (compare(pivot, i) > 0) {
        swap(pivot, j);
    }

    qsort(element(0), i, size, compar);
    qsort(element(i), nmemb - i, size, compar);
}
