#include <platform.h>
#include <platform-virt.h>
#include <kprintf.h>
#include <stdbool.h>
#include <stdint.h>
#include <virt-uart.h>
#include <virtio.h>
#include <virtio-gpu.h>


#define STR_TO_U32(str) (*(uint32_t *)str)


bool init_platform_virt(void)
{
    struct VirtIOControlRegs *virtio_control =
        (struct VirtIOControlRegs *)VPBA_VIRTIO_BASE;

    if (virtio_control->magic != STR_TO_U32("virt")) {
        return false;
    }

    platform_funcs()->putchar = virt_uart_putchar;

    kprintf("VirtIO platform detected\n");

    while (virtio_control->magic == STR_TO_U32("virt")) {
        init_virtio_device(virtio_control);
        virtio_control++;
    }

    return true;
}
