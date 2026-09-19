#include <stdint.h>
extern void uart_puts(const char *s);
extern void uart_putc(char c);

extern void gic_init(void);
extern void timer_init(void);
extern void page_alloc_init(void);
extern void *page_alloc(void);
extern int page_free(void *page);
extern uint32_t allocator_total_pages;


static void uart_puthex(uintptr_t value)
{
    static const char hex[] = "0123456789abcdef";
    int shift;
    for (shift = 60; shift >= 0; shift -= 4)
        uart_putc(hex[(value >> shift) & 0xF]);
}

void kernel_main(void)
{
    uart_puts("TIMER TEST\r\n");

    gic_init();
    timer_init();
    page_alloc_init();
    uart_puts("MEMORY TEST\r\n");

    uart_puts("PAGE1 = 0x");
    uart_puthex((uintptr_t)page_alloc());
    uart_puts("\r\n");

    uart_puts("PAGE2 = 0x");
    uart_puthex((uintptr_t)page_alloc());
    uart_puts("\r\n");

    uart_puts("PAGE3 = 0x");
    uart_puthex((uintptr_t)page_alloc());
    uart_puts("\r\n");

    void *free_test_page;
    void *reused_page;

    free_test_page = page_alloc();
    uart_puts("FREE TEST PAGE = 0x");
    uart_puthex((uintptr_t)free_test_page);
    uart_puts("\r\n");

    uart_puts("FREE RESULT = ");
    uart_puthex((uintptr_t)page_free(free_test_page));
    uart_puts("\r\n");

    reused_page = page_alloc();
    uart_puts("REUSED PAGE = 0x");
    uart_puthex((uintptr_t)reused_page);
    uart_puts("\r\n");

    if (reused_page == free_test_page)
        uart_puts("FREE/REUSE PASS\r\n");
    else
        uart_puts("FREE/REUSE FAIL\r\n");

    int double_free_result;
    int invalid_free_result;

    page_free(reused_page);

    double_free_result = page_free(reused_page);
    uart_puts("DOUBLE FREE RESULT = 0x");
    uart_puthex((uintptr_t)double_free_result);
    uart_puts("\r\n");

    invalid_free_result = page_free((void *)((uintptr_t)reused_page + 1));
    uart_puts("INVALID FREE RESULT = 0x");
    uart_puthex((uintptr_t)invalid_free_result);
    uart_puts("\r\n");

    if (double_free_result == -2 && invalid_free_result == -1)
        uart_puts("NEGATIVE TEST PASS\r\n");
    else
        uart_puts("NEGATIVE TEST FAIL\r\n");

    uint32_t exhausted_count = 0;
    void *exhausted_page;

    while ((exhausted_page = page_alloc()) != (void *)0)
        exhausted_count++;

    uart_puts("EXHAUST COUNT = 0x");
    uart_puthex((uintptr_t)exhausted_count);
    uart_puts("\r\n");

    uart_puts("EXHAUST RESULT = 0x");
    uart_puthex((uintptr_t)page_alloc());
    uart_puts("\r\n");

    if (exhausted_count == (allocator_total_pages - 3U) &&
        page_alloc() == (void *)0)
        uart_puts("EXHAUSTION PASS\r\n");
    else
        uart_puts("EXHAUSTION FAIL\r\n");


    asm volatile("msr daifclr, #2");
    asm volatile("isb");

    while (1) {
        asm volatile("wfe");
    }
}

