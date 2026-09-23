#include <stdint.h>

#include "device.h"
#include "resource.h"

extern void uart_init(uintptr_t base);
extern void uart_puts(const char *s);

#define PL011_DR 0x00
#define PL011_FR 0x18

struct pl011_state {
    uintptr_t base;
};

static struct pl011_state pl011_state;

static int pl011_probe(struct device *dev)
{
    struct resource *mem;
    volatile uint32_t *fr;
    uint32_t fr_value;

    if (dev == (void *)0)
        return -1;

    mem = device_get_resource(
        dev,
        RESOURCE_MEM,
        0);

    if (mem == (void *)0)
        return -2;

    if (mem->start > mem->end)
        return -3;

    if ((mem->start & 0x3UL) != 0)
        return -4;

    if ((mem->end - mem->start + 1) < 0x100UL)
        return -5;

    pl011_state.base = mem->start;

    /*
     * The driver gets its MMIO base exclusively
     * from the device resource.
     */
    uart_init(pl011_state.base);

    /*
     * Read the PL011 Flag Register through the
     * resource-derived MMIO address.
     */
    fr = (volatile uint32_t *)
         (pl011_state.base + PL011_FR);

    fr_value = *fr;
    (void)fr_value;

    dev->driver_data = &pl011_state;

    uart_puts("PL011 PROBE PASS\r\n");
    uart_puts("PL011 MMIO PASS\r\n");

    return 0;
}

static int pl011_remove(struct device *dev)
{
    if (dev == (void *)0)
        return -1;

    dev->driver_data = (void *)0;

    return 0;
}

static struct driver pl011_driver = {
    .name = "pl011-driver",
    .compatible = "arm,pl011",
    .probe = pl011_probe,
    .remove = pl011_remove,
    .registered = 0
};

int pl011_driver_register(void)
{
    return driver_register(&pl011_driver);
}
