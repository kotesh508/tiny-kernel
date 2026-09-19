#include <stdint.h>

#define PAGE_SIZE 0x1000UL
#define PAGE_MASK (PAGE_SIZE - 1)

extern char heap_start[];
extern char heap_end[];

volatile uintptr_t allocator_start;
volatile uintptr_t allocator_end;
volatile uint32_t allocator_total_pages;
volatile uint32_t allocator_bitmap_bytes;
volatile uint32_t allocator_initialized;

static uint8_t *page_bitmap;

static uintptr_t align_up(uintptr_t value)
{
    return (value + PAGE_MASK) & ~PAGE_MASK;
}

static void bitmap_clear(void)
{
    uint32_t i;

    for (i = 0; i < allocator_bitmap_bytes; i++)
        page_bitmap[i] = 0;
}

static int bitmap_test(uint32_t index)
{
    return (page_bitmap[index >> 3] &
            (uint8_t)(1U << (index & 7))) != 0;
}

static void bitmap_set(uint32_t index)
{
    page_bitmap[index >> 3] |=
        (uint8_t)(1U << (index & 7));
}

static void bitmap_clear_bit(uint32_t index)
{
    page_bitmap[index >> 3] &=
        (uint8_t)~(1U << (index & 7));
}

void page_alloc_init(void)
{
    uintptr_t start = (uintptr_t)heap_start;
    uintptr_t end = (uintptr_t)heap_end;
    uint32_t raw_pages =
        (uint32_t)((end - start) / PAGE_SIZE);

    allocator_bitmap_bytes =
        (raw_pages + 7U) / 8U;

    allocator_start =
        align_up(start + allocator_bitmap_bytes);

    allocator_end = end;

    allocator_total_pages =
        (uint32_t)((allocator_end - allocator_start)
                   / PAGE_SIZE);

    page_bitmap = (uint8_t *)start;

    bitmap_clear();

    allocator_initialized = 1;
}

void *page_alloc(void)
{
    uint32_t i;

    if (!allocator_initialized)
        return (void *)0;

    for (i = 0; i < allocator_total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);

            return (void *)(allocator_start +
                            ((uintptr_t)i * PAGE_SIZE));
        }
    }

    return (void *)0;
}

int page_free(void *page)
{
    uintptr_t address;
    uint32_t index;

    if (!allocator_initialized || page == (void *)0)
        return -1;

    address = (uintptr_t)page;

    if (address < allocator_start ||
        address >= allocator_end)
        return -1;

    if ((address & PAGE_MASK) != 0)
        return -1;

    index =
        (uint32_t)((address - allocator_start)
                   / PAGE_SIZE);

    if (index >= allocator_total_pages)
        return -1;

    if (!bitmap_test(index))
        return -2;

    bitmap_clear_bit(index);

    return 0;
}
