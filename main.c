#include <cpu.h>
#include <kprintf.h>
#include <platform.h>
#include <string.h>


#define CURSOR_SIZE 20

static void draw_cursor(uint32_t *fb, size_t stride,
                        int x, int y, uint32_t color)
{
    fb += y * stride / 4 + x;

    for (int l = 0; l < CURSOR_SIZE; l++) {
        memset32(fb, color, l + 1);
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


void main(void)
{
    init_platform();

    puts("[main] Hello, RISC-V world!");

    base_int_t misa = read_csr(0x301);
    int mxl = misa >> (sizeof(base_int_t) * 8 - 2);

    kprintf("[main] CPU model: RV%i", 16 << mxl);
    for (int i = 0; i < 26; i++) {
        if (misa & (1 << i)) {
            putchar(i + 'A');
        }
    }
    putchar('\n');


    if (!platform_funcs()->framebuffer) {
        puts("[main] No framebuffer found, shutting down");
        return;
    }

    puts("[main] Framebuffer found, clearing it with gray");

    uint32_t *fb = platform_funcs()->framebuffer();
    int fbw = platform_funcs()->fb_width();
    int fbh = platform_funcs()->fb_height();
    size_t fb_stride = platform_funcs()->fb_stride();

    memset(fb, 0x40, fbh * fb_stride);
    platform_funcs()->fb_flush(0, 0, 0, 0);

    int mouse_x = fbw / 2, mouse_y = fbh / 2;

    draw_cursor(fb, fb_stride, mouse_x, mouse_y, 0x80ff00);
    platform_funcs()->fb_flush(mouse_x, mouse_y, CURSOR_SIZE, CURSOR_SIZE);

    for (;;) {
        int key, dx, dy, button;
        bool has_button, up, button_up;

        if (platform_funcs()->get_keyboard_event(&key, &up)) {
            kprintf("[main] key %i %s\n", key, up ? "up" : "down");
        }

        if (platform_funcs()->get_mouse_event(&dx, &dy, &has_button,
                                              &button, &button_up))
        {
            if (has_button) {
                kprintf("[main] mouse button %i %s\n", button, button_up ? "up" : "down");
            }
            if (dx || dy) {
                int omx = mouse_x, omy = mouse_y;

                mouse_x = saturated_add(mouse_x, dx, 0, fbw);
                mouse_y = saturated_add(mouse_y, dy, 0, fbh);

                draw_cursor(fb, fb_stride, omx, omy, 0x404040);
                draw_cursor(fb, fb_stride, mouse_x, mouse_y, 0x80ff00);

                int fulx = MIN(omx, mouse_x);
                int fuly = MIN(omy, mouse_y);
                int flrx = MAX(omx, mouse_x);
                int flry = MAX(omy, mouse_y);

                // I'm lazy
                flrx = saturated_add(flrx, CURSOR_SIZE, 0, fbw);
                flry = saturated_add(flry, CURSOR_SIZE, 0, fbh);

                platform_funcs()->fb_flush(fulx, fuly,
                                           flrx - fulx, flry - fuly);
            }
        }
    }
}
