#include <htif.h>
#include <kprintf.h>
#include <platform.h>
#include <platform-spike.h>
#include <stdbool.h>
#include <stdint.h>


static void spike_putchar(uint8_t c);

bool init_platform_spike(void)
{
    platform_funcs()->putchar = spike_putchar;

    kprintf("Unknown platform, assuming Spike board\n");

    return true;
}


static void spike_putchar(uint8_t c)
{
    htif_write_to_host(HTIF_CONSOLE, HTIF_CON_WRITE, c);
    htif_clear_from_host(); // acknowledge result
}