#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_READY    0
#define TASK_RUNNING  1
#define TASK_DONE     2
#define TASK_DEAD     3

typedef void (*task_entry_t)(void);

struct task {
    void *stack_page;
    uintptr_t stack_top;
    task_entry_t entry;
    int state;
};

int task_create(struct task *task, task_entry_t entry);
int task_run(struct task *task);
int task_destroy(struct task *task);

#endif
