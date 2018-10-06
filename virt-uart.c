#include <platform-virt.h>
#include <virt-uart.h>


void virt_uart_putchar(uint8_t c)
{
    *(volatile uint8_t *)VPBA_UART_BASE = c;
}
