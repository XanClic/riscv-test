#include <cpu.h>
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


static int16_t key_sound[4000];

static uint32_t *bg_image;
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

    if (!load_image("/loading.png", &bg_image, &fbw, &fbh, fb_stride)) {
        PRINT("Failed to load loading screen\n"); // how ironic
        abort();
    }

    memcpy(fb, bg_image, fbh * fb_stride);
    platform_funcs.fb_flush(0, 0, 0, 0);


    // Now initialize the rest

    if (!load_image("/bg.png", &bg_image, &fbw, &fbh, fb_stride)) {
        PRINT("Failed to load background image\n");
        abort();
    }


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

    platform_funcs.limit_pointing_device(fbw, fbh);


    init_music();


    int16_t sample = 0;
    for (int i = 0; i < (int)ARRAY_SIZE(key_sound); i++) {
        key_sound[i] = sample;
        sample += 0x2000;
    }


    int mouse_x = fbw / 2, mouse_y = fbh / 2;

    memcpy(fb, bg_image, fbh * fb_stride);
    platform_funcs.fb_flush(0, 0, 0, 0);

    bool need_cursor_updates = platform_funcs.need_cursor_updates &&
        platform_funcs.need_cursor_updates();

    for (;;) {
        int key, button;
        bool has_button, up, button_up;

        if (platform_funcs.get_keyboard_event(&key, &up)) {
            PRINT("key %i %s\n", key, up ? "up" : "down");
            if (!up) {
                platform_funcs.queue_audio_track(key_sound,
                                                 ARRAY_SIZE(key_sound),
                                                 8000, 1, NULL);
            }
        }

        if (platform_funcs.get_pointing_event(&mouse_x, &mouse_y, &has_button,
                                              &button, &button_up))
        {
            if (has_button) {
                PRINT("mouse button %i %s\n",
                      button, button_up ? "up" : "down");
            }

            if (need_cursor_updates) {
                platform_funcs.move_cursor(mouse_x, mouse_y);
            }
        }

        handle_music();
        platform_funcs.handle_audio();
    }
}
