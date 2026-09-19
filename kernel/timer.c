#include <stdint.h>

static uint64_t timer_period;

static inline uint64_t read_cntfrq(void)
{
    uint64_t value;

    asm volatile("mrs %0, cntfrq_el0" : "=r"(value));

    return value;
}

static inline void write_cntv_tval(uint64_t value)
{
    asm volatile("msr cntv_tval_el0, %0" :: "r"(value));
}

static inline void write_cntv_ctl(uint64_t value)
{
    asm volatile("msr cntv_ctl_el0, %0" :: "r"(value));
}

void timer_init(void)
{
    uint64_t frequency = read_cntfrq();

    timer_period = frequency / 2;

    write_cntv_tval(timer_period);

    /* Enable virtual timer, unmask timer interrupt. */
    write_cntv_ctl(1);

    asm volatile("isb");
}

void timer_reload(void)
{
    write_cntv_tval(timer_period);
    asm volatile("isb");
}
