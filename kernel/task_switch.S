.section .text
.global task_start
.type task_start, %function

task_start:
    stp x19, x20, [sp, #-16]!
    stp x29, x30, [sp, #-16]!

    mov x19, sp
    mov sp, x0
    blr x1

    mov sp, x19

    ldp x29, x30, [sp], #16
    ldp x19, x20, [sp], #16

    ret
