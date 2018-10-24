#include <htif.h>
#include <kprintf.h>
#include <sifive-clint.h>
#include <platform.h>
#include <platform-spike.h>
#include <stdbool.h>
#include <stdint.h>


static void spike_putchar(uint8_t c);

bool init_platform_spike(void)
{
    platform_funcs.putchar = spike_putchar;

    puts("[platform-spike] Unknown platform, assuming Spike board");

    init_sifive_clint(SPBA_SIFIVE_CLINT);

    return true;
}


static void spike_putchar(uint8_t c)
{
    htif_write_to_host(HTIF_CONSOLE, HTIF_CON_WRITE, c);
    htif_clear_from_host(); // acknowledge result
}
