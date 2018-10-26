#include <cpu.h>
#include <music.h>
#include <nonstddef.h>
#include <platform.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#define CURSOR_SIZE 20

static void draw_cursor(uint32_t *fb, int fbw, int fbh, size_t stride,
                        int x, int y, bool draw)
{
    fb += y * stride / 4 + x;

    for (int l = 0; l < CURSOR_SIZE && y + l < fbh; l++) {
        int w = l;
        int bw;

        if (w >= 3 * CURSOR_SIZE / 4) {
            w -= (w - 3 * CURSOR_SIZE / 4) * 4;
            bw = 4;
        } else {
            w += 1;
            bw = 1;
        }

        if (w == CURSOR_SIZE - 1) {
            bw = w;
        }

        w = MIN(w, fbw - x);

        if (draw) {
            fb[0] = 0;
            for (int i = 1; i < w; i++) {
                fb[i] = (i < w - bw) ? 0xffffff : 0;
            }
        } else {
            memset32(fb, 0x404040, w);
        }
        fb += stride / 4;
    }
}


static int saturated_add(int x, int y, int min, int max)
{
    // Warning: Relies on undefined overflow behavior.
    x += y;
    if (x < min) {
        return min;
    } else if (x > max) {
        return max;
    }
    return x;
}


#define PRINT(...) \
    do { \
        uint64_t time = platform_funcs.elapsed_us(); \
        printf("[%zu.%06u] ", time / 1000000, (int)(time % 1000000)); \
        printf(__VA_ARGS__); \
    } while (0)


static int16_t key_sound[4000];


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


    if (!platform_funcs.framebuffer) {
        PRINT("No framebuffer found, shutting down\n");
        return;
    }

    PRINT("Framebuffer found, clearing it with gray\n");

    uint32_t *fb = platform_funcs.framebuffer();
    int fbw = platform_funcs.fb_width();
    int fbh = platform_funcs.fb_height();
    size_t fb_stride = platform_funcs.fb_stride();

    memset(fb, 0x40, fbh * fb_stride);
    platform_funcs.fb_flush(0, 0, 0, 0);

    int mouse_x = fbw / 2, mouse_y = fbh / 2;

    draw_cursor(fb, fbw, fbh, fb_stride, mouse_x, mouse_y, true);
    platform_funcs.fb_flush(mouse_x, mouse_y, CURSOR_SIZE, CURSOR_SIZE);

    init_music();

    int16_t sample = 0;
    for (int i = 0; i < (int)ARRAY_SIZE(key_sound); i++) {
        key_sound[i] = sample;
        sample += 0x2000;
    }

    for (;;) {
        int key, dx, dy, button;
        bool has_button, up, button_up;

        if (platform_funcs.get_keyboard_event(&key, &up)) {
            PRINT("key %i %s\n", key, up ? "up" : "down");
            if (!up) {
                platform_funcs.queue_audio_track(key_sound,
                                                 ARRAY_SIZE(key_sound),
                                                 8000, 1, NULL);
            }
        }

        if (platform_funcs.get_mouse_event(&dx, &dy, &has_button,
                                              &button, &button_up))
        {
            if (has_button) {
                PRINT("mouse button %i %s\n",
                      button, button_up ? "up" : "down");
            }
            if (dx || dy) {
                int omx = mouse_x, omy = mouse_y;

                mouse_x = saturated_add(mouse_x, dx, 0, fbw);
                mouse_y = saturated_add(mouse_y, dy, 0, fbh);

                draw_cursor(fb, fbw, fbh, fb_stride, omx, omy, false);
                draw_cursor(fb, fbw, fbh, fb_stride, mouse_x, mouse_y, true);

                int fulx = MIN(omx, mouse_x);
                int fuly = MIN(omy, mouse_y);
                int flrx = MAX(omx, mouse_x);
                int flry = MAX(omy, mouse_y);

                // I'm lazy
                flrx = saturated_add(flrx, CURSOR_SIZE, 0, fbw);
                flry = saturated_add(flry, CURSOR_SIZE, 0, fbh);

                platform_funcs.fb_flush(fulx, fuly,
                                        flrx - fulx, flry - fuly);
            }
        }

        handle_music();
        platform_funcs.handle_audio();
    }
}
