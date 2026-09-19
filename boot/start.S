.section .text.boot
.global _start

_start:
    ldr x0, =stack_top
    mov sp, x0

    ldr x0, =vector_table
    msr vbar_el1, x0
    isb

    bl kernel_main

1:
    wfe
    b 1b
