#include "task.h"

#define PAGE_SIZE 0x1000UL

extern void *page_alloc(void);
extern int page_free(void *page);
extern void task_start(uintptr_t stack_top, task_entry_t entry);

int task_create(struct task *task, task_entry_t entry)
{
    void *page;

    if (task == (void *)0 || entry == (task_entry_t)0)
        return -1;

    page = page_alloc();

    if (page == (void *)0)
        return -2;

    task->stack_page = page;
    task->stack_top = (uintptr_t)page + PAGE_SIZE;
    task->entry = entry;
    task->state = TASK_READY;

    return 0;
}

int task_run(struct task *task)
{
    if (task == (void *)0 ||
        task->stack_page == (void *)0 ||
        task->entry == (task_entry_t)0 ||
        task->state != TASK_READY)
        return -1;

    task->state = TASK_RUNNING;

    task_start(task->stack_top, task->entry);

    task->state = TASK_DONE;

    return 0;
}

int task_destroy(struct task *task)
{
    int result;

    if (task == (void *)0 ||
        task->stack_page == (void *)0)
        return -1;

    if (task->state == TASK_RUNNING)
        return -2;

    result = page_free(task->stack_page);

    if (result != 0)
        return result;

    task->stack_page = (void *)0;
    task->stack_top = 0;
    task->entry = (task_entry_t)0;
    task->state = TASK_DEAD;

    return 0;
}
