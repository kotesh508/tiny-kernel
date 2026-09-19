ENTRY(_start)

SECTIONS
{
    . = 0x40080000;

    .text : ALIGN(16)
    {
        KEEP(*(.text.boot))
        *(.text)
        *(.text.exception)
    }

    .text.vectors : ALIGN(2048)
    {
        KEEP(*(.text.vectors))
    }

    .rodata : ALIGN(16)
    {
        *(.rodata*)
    }

    .data : ALIGN(16)
    {
        *(.data*)
    }

    .bss (NOLOAD) : ALIGN(16)
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;

        . = ALIGN(16);
        . += 0x10000;

        stack_top = .;
        . = ALIGN(0x1000);
        heap_start = .;
        heap_end = ABSOLUTE(0x48000000);
    }

    /DISCARD/ :
    {
        *(.comment*)
        *(.note*)
        *(.eh_frame*)
        *(.gnu.build.attributes*)
    }
}
