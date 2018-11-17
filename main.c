#include <cpu.h>
#include <game-logic.h>
#include <image.h>
#include <incbinfs.h>
#include <music.h>
#include <nonstddef.h>
#include <platform.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define PRINT(...) \
    do { \
        uint64_t time = platform_funcs.elapsed_us(); \
        printf("[%zu.%06u] ", time / 1000000, (int)(time % 1000000)); \
        printf(__VA_ARGS__); \
    } while (0)


extern uint32_t *abort_image;


void main(void)
{
    init_platform();

    PRINT("Hello, RISC-V world!\n");

    base_int_t misa = read_csr(0x301);
    int mxl = misa >> (sizeof(base_int_t) * 8 - 2);

    PRINT("CPU model: RV%i", 16 << mxl);
    for (int i = 0; i < 26; i++) {
        if (misa & (1 << i)) {
            putchar(i + 'A');
        }
    }
    putchar('\n');


    // Quick initialization to get a loading screen up

    if (!platform_funcs.framebuffer) {
        PRINT("No framebuffer found, shutting down\n");
        abort();
    }

    init_incbinfs();

    uint32_t *fb = platform_funcs.framebuffer();
    int fbw = platform_funcs.fb_width();
    int fbh = platform_funcs.fb_height();
    size_t fb_stride = platform_funcs.fb_stride();

    if (!load_image("/abort.png", &abort_image, &fbw, &fbh, fb_stride)) {
        PRINT("Failed to load abort screen\n");
        abort();
    }

    uint32_t *loading_image = NULL;
    if (!load_image("/loading.png", &loading_image, &fbw, &fbh, fb_stride)) {
        PRINT("Failed to load loading screen\n"); // how ironic
        abort();
    }

    memcpy(fb, loading_image, fbh * fb_stride);
    platform_funcs.fb_flush(0, 0, 0, 0);
    free(loading_image);


    // Now initialize the rest

    {
        uint32_t *cursor = NULL;
        int cursor_w = 0, cursor_h = 0;
        if (!load_image("/cursor.png", &cursor, &cursor_w, &cursor_h, 0)) {
            PRINT("Failed to load cursor sprite\n");
            abort();
        }

        // FIXME: hot_x/hot_y
        if (!platform_funcs.setup_cursor(cursor, cursor_w, cursor_h, 2, 4)) {
            PRINT("Failed to setup the cursor\n");
            abort();
        }

        free(cursor);
    }


    init_game();

    init_music();


    for (;;) {
        handle_game();
        handle_music();
        platform_funcs.handle_audio();
    }
}
