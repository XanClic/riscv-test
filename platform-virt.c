#include <platform.h>
#include <platform-virt.h>
#include <kprintf.h>
#include <stdbool.h>
#include <stdint.h>
#include <virt-uart.h>


bool init_platform_virt(void)
{
    static const char virt_str[4] = "virt";

    if (*(uint32_t *)VPBA_VIRTIO_BASE != *(uint32_t *)virt_str) {
        return false;
    }

    platform_funcs()->putchar = virt_uart_putchar;

    kprintf("VirtIO platform detected\n");

    return true;
}
