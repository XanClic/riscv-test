#include <assert.h>
#include <errno.h>
#include <nonstddef.h>
#include <png.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const char *current_png;
static bool png_had_error;


static void png_error_func(png_struct *png, const char *msg)
{
    (void)png;

    png_had_error = true;
    printf("[image] %s: %s\n", current_png, msg);
}


bool load_image(const char *name, uint32_t **dest, int *w, int *h, int stride)
{
    FILE *fp = NULL;
    png_struct *png = NULL;
    png_info *info = NULL;
    uint32_t png_w, png_h;
    bool ret = false;

    if (stride || *dest) {
        assert(*w && *h);
    }

    fp = fopen(name, "rb");
    if (!fp) {
        printf("[image] %s: %s\n", name, strerror(errno));
        goto fail;
    }

    png_had_error = false;
    current_png = name;

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
                                 png_error_func, NULL);
    assert(png);

    info = png_create_info_struct(png);
    assert(info);

    png_init_io(png, fp);

    png_read_info(png, info);
    png_get_IHDR(png, info, &png_w, &png_h, NULL, NULL, NULL, NULL, NULL);
    if ((*w && (int)png_w != *w) || (*h && (int)png_h != *h)) {
        printf("[image] %s: Wrong dimensions (expected %ix%i, got %ix%i)\n",
               name, *w, *h, (int)png_w, (int)png_h);
        goto fail;
    }

    *w = png_w;
    *h = png_h;

    stride = MAX(stride, (int)(png_w * sizeof(uint32_t)));

    png_set_scale_16(png);
    png_set_packing(png);
    png_set_palette_to_rgb(png);
    png_set_expand_gray_1_2_4_to_8(png);
    png_set_tRNS_to_alpha(png);
    png_set_bgr(png);
    png_set_filler(png, 0, PNG_FILLER_AFTER);
    png_read_update_info(png, info);

    if (!*dest) {
        *dest = malloc(png_h * stride);
    }

    {
        unsigned char *rows[png_h];
        for (int i = 0; i < (int)png_h; i++) {
            rows[i] = (unsigned char *)*dest + i * stride;
        }
        png_read_image(png, rows);
    }

    png_read_end(png, info);

    if (png_had_error) {
        goto fail;
    }

    ret = true;
fail:
    png_destroy_read_struct(&png, &info, NULL);
    if (fp) {
        fclose(fp);
    }

    return ret;
}
