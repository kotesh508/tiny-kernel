#ifndef RESOURCE_H
#define RESOURCE_H

#include <stdint.h>

enum resource_type {
    RESOURCE_NONE = 0,
    RESOURCE_MEM  = 1,
    RESOURCE_IRQ  = 2
};

#define RESOURCE_MAX_PER_DEVICE 8

struct resource {
    enum resource_type type;

    uintptr_t start;
    uintptr_t end;

    uint32_t flags;
};

#endif
