#include <stdint.h>
#include "task.h"
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

static volatile uint32_t demo_task_runs = 0;

static void demo_task(void)
{
    demo_task_runs++;
    volatile uint64_t task_stack_marker = 0x1122334455667788ULL;

    uart_puts("TASK START\r\n");

    if (task_stack_marker == 0x1122334455667788ULL)
        uart_puts("TASK RUNNING\r\n");
    else
        uart_puts("TASK STACK FAIL\r\n");

    uart_puts("TASK END\r\n");
}

void kernel_main(void)
{
    uart_puts("TIMER TEST\r\n");

    gic_init();
    timer_init();
    page_alloc_init();

    uart_puts("TASK TEST\r\n");

    struct task demo_task_control;
    struct task second_task;

    int create_result;
    int run_result;
    int destroy_result;
    int second_create_result;
    int second_destroy_result;
    int invalid_create_result;
    int invalid_entry_result;
    int invalid_run_result;
    int invalid_destroy_result;

    uintptr_t first_stack_top;
    uintptr_t second_stack_top;
    void *first_stack_page;
    void *second_stack_page;

    demo_task_runs = 0;

    invalid_create_result =
        task_create((struct task *)0, demo_task);

    invalid_entry_result =
        task_create(&demo_task_control, (task_entry_t)0);

    create_result =
        task_create(&demo_task_control, demo_task);

    if (invalid_create_result == -1 &&
        invalid_entry_result == -1 &&
        create_result == 0)
        uart_puts("TASK CREATE PASS\r\n");
    else
        uart_puts("TASK CREATE FAIL\r\n");

    first_stack_page = demo_task_control.stack_page;
    first_stack_top = demo_task_control.stack_top;

    if (demo_task_control.state == TASK_READY &&
        first_stack_page != (void *)0 &&
        first_stack_top ==
            ((uintptr_t)first_stack_page + 0x1000UL))
        uart_puts("TASK READY PASS\r\n");
    else
        uart_puts("TASK READY FAIL\r\n");

    run_result = task_run(&demo_task_control);

    if (run_result == 0 &&
        demo_task_runs == 1 &&
        demo_task_control.state == TASK_DONE)
        uart_puts("TASK RUN PASS\r\n");
    else
        uart_puts("TASK RUN FAIL\r\n");

    if (destroy_result = task_destroy(&demo_task_control),
        destroy_result == 0 &&
        demo_task_control.state == TASK_DEAD &&
        demo_task_control.stack_page == (void *)0)
        uart_puts("TASK DESTROY PASS\r\n");
    else
        uart_puts("TASK DESTROY FAIL\r\n");

    second_create_result =
        task_create(&second_task, demo_task);

    second_stack_page = second_task.stack_page;
    second_stack_top = second_task.stack_top;

    if (second_create_result == 0 &&
        second_stack_page == first_stack_page &&
        second_stack_top ==
            ((uintptr_t)second_stack_page + 0x1000UL))
        uart_puts("TASK STACK REUSE PASS\r\n");
    else
        uart_puts("TASK STACK REUSE FAIL\r\n");

    second_destroy_result =
        task_destroy(&second_task);

    invalid_run_result =
        task_run(&demo_task_control);

    invalid_destroy_result =
        task_destroy(&demo_task_control);

    if (second_destroy_result == 0 &&
        invalid_run_result == -1 &&
        invalid_destroy_result == -1)
        uart_puts("TASK NEGATIVE PASS\r\n");
    else
        uart_puts("TASK NEGATIVE FAIL\r\n");

    if (demo_task_runs == 1)
        uart_puts("TASK BODY PASS\r\n");
    else
        uart_puts("TASK BODY FAIL\r\n");

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

