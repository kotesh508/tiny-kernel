#include <stdint.h>

#define GICD_CTLR       0x000
#define GICD_IGROUPR0   0x080
#define GICD_ISENABLER0 0x100

#define GICC_CTLR       0x000
#define GICC_PMR        0x004
#define GICC_IAR        0x00C
#define GICC_EOIR       0x010

static uintptr_t gicd_base;
static uintptr_t gicc_base;
static uint32_t timer_intid;

static inline void mmio_write32(uintptr_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

void gic_init(uintptr_t distributor_base,
              uintptr_t cpu_interface_base,
              uint32_t timer_irq)
{
    uint32_t group;

    if (distributor_base == 0 ||
        cpu_interface_base == 0 ||
        timer_irq >= 32)
        return;

    gicd_base = distributor_base;
    gicc_base = cpu_interface_base;
    timer_intid = timer_irq;

    group = mmio_read32(gicd_base + GICD_IGROUPR0);
    group |= (1U << timer_intid);
    mmio_write32(gicd_base + GICD_IGROUPR0, group);

    mmio_write32(gicd_base + GICD_ISENABLER0,
                 (1U << timer_intid));

    mmio_write32(gicc_base + GICC_PMR, 0xFF);
    mmio_write32(gicc_base + GICC_CTLR, 0x6);
    mmio_write32(gicd_base + GICD_CTLR, 0x2);
}

uint32_t gic_acknowledge(void)
{
    if (gicc_base == 0)
        return 0;

    return mmio_read32(gicc_base + GICC_IAR);
}

void gic_end_interrupt(uint32_t iar)
{
    if (gicc_base == 0)
        return;

    mmio_write32(gicc_base + GICC_EOIR, iar);
}

uint32_t gic_timer_irq(void)
{
    return timer_intid;
}
