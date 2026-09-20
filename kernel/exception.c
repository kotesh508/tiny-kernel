#include <stdint.h>

extern void uart_puts(const char *s);
extern uint32_t gic_acknowledge(void);
extern void gic_end_interrupt(uint32_t iar);
extern uint32_t gic_timer_irq(void);
extern void timer_reload(void);

volatile uint64_t captured_elr;
volatile uint64_t captured_esr;
volatile uint64_t captured_spsr;
volatile uint64_t captured_vbar;
volatile uint64_t captured_far;

volatile uint64_t timer_ticks;

void exception_report(void)
{
    asm volatile("mrs %0, elr_el1"  : "=r"(captured_elr));
    asm volatile("mrs %0, esr_el1"  : "=r"(captured_esr));
    asm volatile("mrs %0, spsr_el1" : "=r"(captured_spsr));
    asm volatile("mrs %0, vbar_el1" : "=r"(captured_vbar));
    asm volatile("mrs %0, far_el1"  : "=r"(captured_far));

    uart_puts("SYNC EXCEPTION\r\n");
}

void irq_handler(void)
{
    uint32_t iar;
    uint32_t intid;

    iar = gic_acknowledge();
    intid = iar & 0x3FF;

    if (intid == gic_timer_irq()) {
        timer_ticks++;
        timer_reload();
        uart_puts("TICK\r\n");
    }

    gic_end_interrupt(iar);
}
