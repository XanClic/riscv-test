#include <assert.h>
#include <limits.h>
#include <platform.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


uint32_t *abort_image;


_Noreturn void _abort(void)
{
    if (abort_image) {
        memcpy(platform_funcs.framebuffer(),
               abort_image,
               platform_funcs.fb_height() * platform_funcs.fb_stride());
        platform_funcs.fb_flush(0, 0, 0, 0);
    }

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


void shuffle(void *base, size_t nmemb, size_t size)
{
    assert(size && INT_MAX / size >= nmemb);

    char tmp[size];

    for (int i = nmemb - 1; i >= 1; i--) {
        int j = rand() % (i + 1);

        void *ei = (void *)((uintptr_t)base + i * size);
        void *ej = (void *)((uintptr_t)base + j * size);

        memcpy(tmp, ei, size);
        memcpy(ei, ej, size);
        memcpy(ej, tmp, size);
    }
}


static uint32_t rand_seed;

// does not conform to any standards
int rand(void)
{
    uint64_t eus = platform_funcs.elapsed_us();

    // whatever
    rand_seed += (eus | (eus >> 10)) + 0x3ed69f04 +
                 (rand_seed << 16) + (rand_seed >> 16);

    return rand_seed & 0x7fffffff;
}
