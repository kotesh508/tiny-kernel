#include <stdint.h>

#define GICD_BASE          0x08000000UL
#define GICC_BASE          0x08010000UL

#define GICD_CTLR          0x000
#define GICD_IGROUPR0      0x080
#define GICD_ISENABLER0    0x100

#define GICC_CTLR          0x000
#define GICC_PMR           0x004
#define GICC_IAR           0x00C
#define GICC_AIAR         0x020
#define GICC_EOIR          0x010
#define GICC_AEOIR        0x024

#define TIMER_INTID        27

static inline void mmio_write32(uintptr_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

void gic_init(void)
{
    /* Put timer PPI 27 into Group 1. */
    uint32_t group = mmio_read32(GICD_BASE + GICD_IGROUPR0);
    group |= (1U << TIMER_INTID);
    mmio_write32(GICD_BASE + GICD_IGROUPR0, group);

    /* Enable timer PPI 27. */
    mmio_write32(GICD_BASE + GICD_ISENABLER0,
                 (1U << TIMER_INTID));

    /* Allow all priorities. */
    mmio_write32(GICC_BASE + GICC_PMR, 0xFF);

    /* Enable Group 1 interrupts. */
    mmio_write32(GICC_BASE + GICC_CTLR, 0x6);

    /* Enable Group 1 at distributor. */
    mmio_write32(GICD_BASE + GICD_CTLR, 0x2);
}

uint32_t gic_acknowledge(void)
{
    return mmio_read32(GICC_BASE + GICC_IAR);
}

void gic_end_interrupt(uint32_t iar)
{
    mmio_write32(GICC_BASE + GICC_EOIR, iar);
}
