#include <cpu.h>
#include <kprintf.h>
#include <platform.h>
#include <string.h>


void main(void)
{
    init_platform();

    puts("Hello, RISC-V world!");

    base_int_t misa = read_csr(0x301);
    int mxl = misa >> (sizeof(base_int_t) * 8 - 2);

    kprintf("CPU model: RV%i", 16 << mxl);
    for (int i = 0; i < 26; i++) {
        if (misa & (1 << i)) {
            putchar(i + 'A');
        }
    }
    putchar('\n');


    if (platform_funcs()->framebuffer) {
        puts("Framebuffer found, clearing it with gray");

        uint32_t *fb = platform_funcs()->framebuffer();
        memset(fb, 0x40,
               platform_funcs()->fb_height() * platform_funcs()->fb_stride());
        platform_funcs()->fb_flush(0, 0, 0, 0);
    }
}
