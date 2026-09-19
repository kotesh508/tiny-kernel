#define UART_BASE 0x09000000UL
#define UART_DR   0x00
#define UART_FR   0x18

void uart_putc(char c)
{
    volatile unsigned int *uart =
        (volatile unsigned int *)UART_BASE;

    while (uart[UART_FR / 4] & (1 << 5))
        ;

    uart[UART_DR / 4] = c;
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}
