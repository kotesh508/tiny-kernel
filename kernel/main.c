#include <stdint.h>
#include "device.h"
#include "task.h"
#include "fdt.h"
extern void uart_init(uintptr_t base);
extern void uart_puts(const char *s);
extern void uart_putc(char c);

extern void gic_init(uintptr_t distributor_base,
                     uintptr_t cpu_interface_base,
                     uint32_t timer_irq);
extern void timer_init(void);
extern void page_alloc_init(uintptr_t reserved_start,
                             uintptr_t reserved_size);
extern volatile uint32_t allocator_reserved_pages;
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

void kernel_main(uintptr_t dtb)
{
    struct fdt_header_info fdt_info;
    struct fdt_reg uart_reg;
    struct fdt_reg gic_reg;
    uint32_t timer_irq;
    int fdt_result;
    int uart_result;

    fdt_result = fdt_read_header(dtb, &fdt_info);

    if (fdt_result != 0)
        while (1)
            __asm__ volatile ("wfe");

    uart_result =
        fdt_find_compatible_reg(
            dtb,
            &fdt_info,
            "arm,pl011",
            &uart_reg);

    if (uart_result != 0 ||
        uart_reg.base == 0)
        while (1)
            __asm__ volatile ("wfe");

    uart_init(uart_reg.base);

    uart_puts("DTB = 0x");
    uart_puthex(dtb);
    uart_puts("\r\n");

    uart_puts("UART BASE = 0x");
    uart_puthex(uart_reg.base);
    uart_puts("\r\n");

    uart_puts("UART SIZE = 0x");
    uart_puthex(uart_reg.size);
    uart_puts("\r\n");

    uart_puts("DTB UART DISCOVERY PASS\r\n");

    if (fdt_result == 0) {
        uart_puts("FDT MAGIC = 0x");
        uart_puthex(fdt_info.magic);
        uart_puts("\r\n");

        uart_puts("FDT SIZE = 0x");
        uart_puthex(fdt_info.totalsize);
        uart_puts("\r\n");

        uart_puts("FDT STRUCT = 0x");
        uart_puthex(fdt_info.off_dt_struct);
        uart_puts("\r\n");

        uart_puts("FDT STRINGS = 0x");
        uart_puthex(fdt_info.off_dt_strings);
        uart_puts("\r\n");

        uart_puts("FDT HEADER PASS\r\n");

        if (fdt_dump_root_properties(dtb, &fdt_info) == 0)
            uart_puts("FDT ROOT PROPERTY PASS\r\n");
        else
            uart_puts("FDT ROOT PROPERTY FAIL\r\n");

        if (fdt_dump_node_names(dtb, &fdt_info) == 0)
            uart_puts("FDT NODE WALK PASS\r\n");
        else
            uart_puts("FDT NODE WALK FAIL\r\n");

        if (fdt_dump_named_node_properties(
                dtb,
                &fdt_info,
                "pl011@9000000") == 0)
            uart_puts("FDT PL011 NODE PASS\r\n");
        else
            uart_puts("FDT PL011 NODE FAIL\r\n");

        if (fdt_dump_named_node_properties(
                dtb,
                &fdt_info,
                "intc@8000000") == 0)
            uart_puts("FDT GIC NODE PASS\r\n");
        else
            uart_puts("FDT GIC NODE FAIL\r\n");

    } else {
        uart_puts("FDT HEADER FAIL = ");
        uart_puthex((uintptr_t)fdt_result);
        uart_puts("\r\n");
    }
    uart_puts("DEVICE MODEL TEST\r\n");

    {
        int device_model_result = device_model_test();

        if (device_model_result == 0)
            uart_puts("DEVICE MODEL PASS\r\n");
        else {
            uart_puts("DEVICE MODEL FAIL\r\n");
            uart_puthex((uintptr_t)(-device_model_result));
            uart_puts("\\r\\n");

            for (;;)
                asm volatile("wfe");
        }
    }

    uart_puts("DTB DEVICE TEST\r\n");

    {
        struct device *pl011_dev;
        int device_result;

        device_result = device_discover_from_fdt_reg(
            dtb,
            &fdt_info,
            "pl011",
            "arm,pl011");

        if (device_result != 0) {
            uart_puts("DTB DEVICE REGISTER FAIL\r\n");
            uart_puthex((uintptr_t)(-device_result));
            uart_puts("\r\n");

            for (;;)
                asm volatile("wfe");
        }

        pl011_dev = device_find_compatible("arm,pl011");

        if (pl011_dev == (void *)0 ||
            pl011_dev->state != DEVICE_REGISTERED ||
            pl011_dev->base != uart_reg.base ||
            pl011_dev->size != uart_reg.size) {

            uart_puts("DTB DEVICE VERIFY FAIL\r\n");

            for (;;)
                asm volatile("wfe");
        }

        uart_puts("DTB DEVICE REGISTER PASS\r\n");

        uart_puts("DEVICE BASE = 0x");
        uart_puthex(pl011_dev->base);
        uart_puts("\r\n");

        uart_puts("DEVICE SIZE = 0x");
        uart_puthex(pl011_dev->size);
        uart_puts("\r\n");

        uart_puts("DTB DEVICE TEST PASS\r\n");
    }

    uart_puts("TIMER TEST\r\n");

    if (fdt_find_compatible_reg(
            dtb,
            &fdt_info,
            "arm,cortex-a15-gic",
            &gic_reg) == 0 &&
        gic_reg.base != 0 &&
        gic_reg.base2 != 0) {
        uart_puts("DTB GIC DISCOVERY PASS\r\n");
    } else {
        uart_puts("DTB GIC DISCOVERY FAIL\r\n");
        for (;;) asm volatile("wfe");
    }

    if (fdt_find_timer_virtual_irq(
            dtb,
            &fdt_info,
            &timer_irq) == 0) {
        uart_puts("DTB TIMER DISCOVERY PASS\r\n");
    } else {
        uart_puts("DTB TIMER DISCOVERY FAIL\r\n");
        for (;;) asm volatile("wfe");
    }

    gic_init(gic_reg.base, gic_reg.base2, timer_irq);
    timer_init();
    page_alloc_init(dtb, fdt_info.totalsize);

    uart_puts("FDT RESERVED PAGES = 0x");
    uart_puthex(allocator_reserved_pages);
    uart_puts("\r\n");

    if (allocator_reserved_pages != 0)
        uart_puts("FDT RESERVATION PASS\r\n");
    else
        uart_puts("FDT RESERVATION FAIL\r\n");

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

    if (exhausted_count ==
            (allocator_total_pages -
             allocator_reserved_pages -
             3U) &&
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

