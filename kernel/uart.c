#include <stdint.h>

#define UART_DR   0x00
#define UART_FR   0x18

static uintptr_t uart_base;

void uart_init(uintptr_t base)
{
    uart_base = base;
}

void uart_putc(char c)
{
    volatile uint32_t *uart;

    if (uart_base == 0)
        return;

    uart =
        (volatile uint32_t *)uart_base;

    while (uart[UART_FR / 4] & (1U << 5))
        ;

    uart[UART_DR / 4] = (uint32_t)c;
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}
