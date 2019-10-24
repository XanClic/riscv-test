#include <platform.h>
#include <platform-virt.h>
#include <sifive-clint.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <virt-sound.h>
#include <virt-uart.h>
#include <virtio.h>
#include <virtio-gpu.h>


#define STR_TO_U32(str) (*(uint32_t *)str)


bool init_platform_virt(void)
{
    struct VirtIOControlRegs *virtio_base =
        (struct VirtIOControlRegs *)VPBA_VIRTIO_BASE;
    struct VirtIOControlRegs *virtio_control = virtio_base;

    if (virtio_control->magic != STR_TO_U32("virt")) {
        return false;
    }

    platform_funcs.putchar = virt_uart_putchar;

    puts("[platform-virt] Virt platform detected");

    init_sifive_clint(VPBA_SIFIVE_CLINT);

    /*
     * TODO: Instead of artificially limiting the number of devices
     *       here, maybe catch faults instead.
     */
    while (virtio_control - virtio_base < 8 &&
           virtio_control->magic == STR_TO_U32("virt"))
    {
        init_virtio_device(virtio_control);
        virtio_control++;
    }

    init_virt_sound();

    return true;
}
