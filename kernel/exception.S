.section .text.vectors, "ax"
.balign 2048
.global vector_table

.macro VECTOR target
    b \target
    .space 124
.endm

vector_table:

    // Current EL with SP0
    VECTOR sync_exception
    VECTOR default_exception
    VECTOR default_exception
    VECTOR default_exception

    // Current EL with SPx
    VECTOR sync_exception
    VECTOR irq_exception
    VECTOR default_exception
    VECTOR default_exception

    // Lower EL using AArch64
    VECTOR default_exception
    VECTOR default_exception
    VECTOR default_exception
    VECTOR default_exception

    // Lower EL using AArch32
    VECTOR default_exception
    VECTOR default_exception
    VECTOR default_exception
    VECTOR default_exception


.section .text.exception, "ax"

.global sync_exception
sync_exception:
    bl exception_report

1:
    wfe
    b 1b

.global irq_exception
irq_exception:
    bl irq_handler
    eret

.global default_exception
default_exception:
2:
    wfe
    b 2b
