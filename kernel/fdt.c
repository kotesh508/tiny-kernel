#include <stdint.h>
#include "fdt.h"

extern void uart_puts(const char *s);
extern void uart_putc(char c);

static uint32_t fdt_be32(uintptr_t addr) {
    const uint8_t *b = (const uint8_t *)addr;

    return ((uint32_t)b[0] << 24) |
           ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  |
           (uint32_t)b[3];
}

static uintptr_t fdt_align4(uintptr_t value)
{
    return (value + 3U) & ~3U;
}

static int fdt_string_valid(uintptr_t address,
                            uintptr_t end)
{
    const uint8_t *p;

    if (address >= end)
        return 0;

    p = (const uint8_t *)address;

    while ((uintptr_t)p < end) {
        if (*p == '\0')
            return 1;

        p++;
    }

    return 0;
}

int fdt_read_header(uintptr_t dtb,
                    struct fdt_header_info *info)
{
    if (dtb == 0 || info == 0)
        return -1;

    info->magic = fdt_be32(dtb + 0x00);

    if (info->magic != FDT_MAGIC)
        return -2;

    info->totalsize = fdt_be32(dtb + 0x04);
    info->off_dt_struct = fdt_be32(dtb + 0x08);
    info->off_dt_strings = fdt_be32(dtb + 0x0c);
    info->size_dt_strings = fdt_be32(dtb + 0x20);
    info->size_dt_struct = fdt_be32(dtb + 0x24);

    if (info->totalsize < 40)
        return -3;

    if (info->off_dt_struct >= info->totalsize)
        return -4;

    if (info->off_dt_strings >= info->totalsize)
        return -5;

    if (info->size_dt_struct >
        info->totalsize - info->off_dt_struct)
        return -6;

    if (info->size_dt_strings >
        info->totalsize - info->off_dt_strings)
        return -7;

    return 0;
}

int fdt_dump_root_properties(
    uintptr_t dtb,
    const struct fdt_header_info *info)
{
    uintptr_t p;
    uintptr_t struct_start;
    uintptr_t struct_end;
    uintptr_t strings_start;
    uintptr_t strings_end;
    uintptr_t name_address;
    uint32_t token;
    uint32_t len;
    uint32_t nameoff;

    if (dtb == 0 || info == 0)
        return -1;

    struct_start = dtb + info->off_dt_struct;
    struct_end = struct_start + info->size_dt_struct;

    strings_start = dtb + info->off_dt_strings;
    strings_end = strings_start + info->size_dt_strings;

    p = struct_start;

    if (p + 4 > struct_end)
        return -2;

    token = fdt_be32(p);
    p += 4;

    if (token != FDT_BEGIN_NODE)
        return -3;

    while (p < struct_end && *(const uint8_t *)p != '\0')
        p++;

    if (p >= struct_end)
        return -4;

    p++;
    p = fdt_align4(p);

    while (p + 4 <= struct_end) {
        token = fdt_be32(p);
        p += 4;

        if (token == FDT_PROP) {
            if (p + 8 > struct_end)
                return -5;

            len = fdt_be32(p);
            nameoff = fdt_be32(p + 4);
            p += 8;

            if (nameoff >= info->size_dt_strings)
                return -6;

            name_address =
                strings_start + nameoff;

            if (!fdt_string_valid(
                    name_address,
                    strings_end))
                return -7;

            if (len > (uint32_t)(struct_end - p))
                return -8;

            uart_puts("FDT ROOT PROP = ");
            uart_puts((const char *)name_address);

            uart_puts(" LEN = 0x");

            {
                static const char hex[] =
                    "0123456789abcdef";
                int shift;

                for (shift = 28; shift >= 0; shift -= 4)
                    uart_putc(
                        hex[(len >> shift) & 0xf]);
            }

            uart_puts("\r\n");

            p = fdt_align4(p + len);
            continue;
        }

        if (token == FDT_NOP)
            continue;

        if (token == FDT_BEGIN_NODE ||
            token == FDT_END_NODE ||
            token == FDT_END)
            break;

        return -9;
    }

    return 0;
}

int fdt_dump_node_names(
    uintptr_t dtb,
    const struct fdt_header_info *info)
{
    uintptr_t p;
    uintptr_t struct_start;
    uintptr_t struct_end;
    const char *name;
    uint32_t token;
    uint32_t len;
    uint32_t nameoff;
    uint32_t depth = 0;

    if (dtb == 0 || info == 0)
        return -1;

    struct_start = dtb + info->off_dt_struct;
    struct_end = struct_start + info->size_dt_struct;

    p = struct_start;

    while (p + 4 <= struct_end) {
        token = fdt_be32(p);
        p += 4;

        if (token == FDT_BEGIN_NODE) {
            name = (const char *)p;

            while (p < struct_end &&
                   *(const uint8_t *)p != '\0')
                p++;

            if (p >= struct_end)
                return -2;

            if (*name == '\0') {
                uart_puts("FDT NODE = <root>\r\n");
            } else {
                uart_puts("FDT NODE = ");
                uart_puts(name);
                uart_puts("\r\n");
            }

            depth++;

            p++;
            p = fdt_align4(p);
            continue;
        }

        if (token == FDT_END_NODE) {
            if (depth == 0)
                return -3;

            depth--;
            continue;
        }

        if (token == FDT_PROP) {
            if (p + 8 > struct_end)
                return -4;

            len = fdt_be32(p);
            nameoff = fdt_be32(p + 4);
            p += 8;

            if (nameoff >= info->size_dt_strings)
                return -5;

            if (len > (uint32_t)(struct_end - p))
                return -6;

            p = fdt_align4(p + len);
            continue;
        }

        if (token == FDT_NOP)
            continue;

        if (token == FDT_END) {
            if (depth != 0)
                return -7;

            return 0;
        }

        return -8;
    }

    return -9;
}

static int fdt_name_equal(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b)
            return 0;

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static void fdt_print_u32_cells(uintptr_t value,
                                uint32_t len)
{
    uint32_t i;

    for (i = 0; i + 4 <= len; i += 4) {
        uart_puts("0x");

        {
            static const char hex[] =
                "0123456789abcdef";
            uint32_t cell = fdt_be32(value + i);
            int shift;

            for (shift = 28; shift >= 0; shift -= 4)
                uart_putc(hex[(cell >> shift) & 0xf]);
        }

        if (i + 4 < len)
            uart_puts(" ");
    }
}

int fdt_dump_named_node_properties(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const char *target_name)
{
    uintptr_t p;
    uintptr_t struct_start;
    uintptr_t struct_end;
    uintptr_t strings_start;
    uintptr_t strings_end;
    uintptr_t name_address;
    uintptr_t value_address;
    const char *node_name;
    uint32_t token;
    uint32_t len;
    uint32_t nameoff;
    uint32_t depth = 0;
    int target_depth = -1;
    int found = 0;

    if (dtb == 0 || info == 0 || target_name == 0)
        return -1;

    struct_start = dtb + info->off_dt_struct;
    struct_end = struct_start + info->size_dt_struct;

    strings_start = dtb + info->off_dt_strings;
    strings_end = strings_start + info->size_dt_strings;

    p = struct_start;

    while (p + 4 <= struct_end) {
        token = fdt_be32(p);
        p += 4;

        if (token == FDT_BEGIN_NODE) {
            node_name = (const char *)p;

            while (p < struct_end &&
                   *(const uint8_t *)p != '\0')
                p++;

            if (p >= struct_end)
                return -2;

            depth++;

            if (target_depth == -1 &&
                fdt_name_equal(node_name, target_name)) {

                target_depth = (int)depth;
                found = 1;

                uart_puts("FDT TARGET NODE = ");
                uart_puts(node_name);
                uart_puts("\r\n");
            }

            p++;
            p = fdt_align4(p);
            continue;
        }

        if (token == FDT_END_NODE) {
            if (depth == 0)
                return -3;

            if ((int)depth == target_depth)
                target_depth = -1;

            depth--;
            continue;
        }

        if (token == FDT_PROP) {
            if (p + 8 > struct_end)
                return -4;

            len = fdt_be32(p);
            nameoff = fdt_be32(p + 4);
            p += 8;

            if (nameoff >= info->size_dt_strings)
                return -5;

            name_address = strings_start + nameoff;

            if (!fdt_string_valid(name_address,
                                  strings_end))
                return -6;

            if (len > (uint32_t)(struct_end - p))
                return -7;

            value_address = p;

            if (target_depth == (int)depth) {
                uart_puts("FDT PROP = ");
                uart_puts((const char *)name_address);
                uart_puts(" LEN = 0x");

                {
                    static const char hex[] =
                        "0123456789abcdef";
                    int shift;

                    for (shift = 28; shift >= 0; shift -= 4)
                        uart_putc(
                            hex[(len >> shift) & 0xf]);
                }

                uart_puts("\r\n");

                if (fdt_name_equal(
                        (const char *)name_address,
                        "compatible")) {

                    uart_puts("FDT COMPATIBLE = ");
                    uart_puts((const char *)value_address);
                    uart_puts("\r\n");
                }

                if (fdt_name_equal(
                        (const char *)name_address,
                        "reg")) {

                    uart_puts("FDT REG CELLS = ");
                    fdt_print_u32_cells(value_address, len);
                    uart_puts("\r\n");
                }
            }

            p = fdt_align4(p + len);
            continue;
        }

        if (token == FDT_NOP)
            continue;

        if (token == FDT_END)
            return found ? 0 : -8;

        return -9;
    }

    return -10;
}



static int fdt_compatible_contains(uintptr_t value,
                                   uint32_t len,
                                   const char *target)
{
    uint32_t offset = 0;

    while (offset < len) {
        const char *current =
            (const char *)(value + offset);
        const char *a = current;
        const char *b = target;
        uint32_t current_len = 0;

        while (offset + current_len < len &&
               current[current_len] != '\0')
            current_len++;

        while (*a != '\0' &&
               *b != '\0' &&
               *a == *b) {
            a++;
            b++;
        }

        if (*a == '\0' && *b == '\0')
            return 1;

        if (offset + current_len >= len)
            break;

        offset += current_len + 1;
    }

    return 0;
}

#define FDT_MAX_DEPTH 32




int fdt_find_compatible_reg(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const char *compatible,
    struct fdt_reg *reg)
{
    uintptr_t p;
    uintptr_t struct_start;
    uintptr_t struct_end;
    uintptr_t strings_start;
    uintptr_t strings_end;

    uint32_t token;
    uint32_t len;
    uint32_t nameoff;
    uint32_t depth = 0;

    uint8_t compat_found[FDT_MAX_DEPTH];
    uint8_t reg_found[FDT_MAX_DEPTH];
    struct fdt_reg reg_values[FDT_MAX_DEPTH];

    if (dtb == 0 ||
        info == 0 ||
        compatible == 0 ||
        reg == 0)
        return -1;

    reg->base = 0;
    reg->size = 0;
    reg->base2 = 0;
    reg->size2 = 0;

    struct_start = dtb + info->off_dt_struct;
    struct_end = struct_start + info->size_dt_struct;
    strings_start = dtb + info->off_dt_strings;
    strings_end = strings_start + info->size_dt_strings;
    p = struct_start;

    while (p + 4 <= struct_end) {
        token = fdt_be32(p);
        p += 4;

        if (token == FDT_BEGIN_NODE) {
            if (depth >= FDT_MAX_DEPTH)
                return -2;

            while (p < struct_end &&
                   *(const uint8_t *)p != '\0')
                p++;

            if (p >= struct_end)
                return -3;

            compat_found[depth] = 0;
            reg_found[depth] = 0;
            reg_values[depth].base = 0;
            reg_values[depth].size = 0;
            reg_values[depth].base2 = 0;
            reg_values[depth].size2 = 0;

            depth++;
            p++;
            p = fdt_align4(p);
            continue;
        }

        if (token == FDT_END_NODE) {
            if (depth == 0)
                return -4;

            depth--;

            if (compat_found[depth] &&
                reg_found[depth]) {
                reg->base  = reg_values[depth].base;
                reg->size  = reg_values[depth].size;
                reg->base2 = reg_values[depth].base2;
                reg->size2 = reg_values[depth].size2;
                return 0;
            }

            continue;
        }

        if (token == FDT_PROP) {
            uintptr_t value;
            const char *property_name;

            if (p + 8 > struct_end)
                return -5;

            if (depth == 0)
                return -6;

            len = fdt_be32(p);
            nameoff = fdt_be32(p + 4);
            p += 8;

            if (nameoff >= info->size_dt_strings)
                return -7;

            property_name =
                (const char *)(strings_start + nameoff);

            if (!fdt_string_valid(
                    (uintptr_t)property_name,
                    strings_end))
                return -8;

            if (len > (uint32_t)(struct_end - p))
                return -9;

            value = p;

            if (fdt_name_equal(property_name, "compatible")) {
                if (fdt_compatible_contains(
                        value, len, compatible))
                    compat_found[depth - 1] = 1;
            }

            if (fdt_name_equal(property_name, "reg")) {
                if (len < 16)
                    return -10;

                reg_values[depth - 1].base =
                    ((uintptr_t)fdt_be32(value) << 32) |
                    (uintptr_t)fdt_be32(value + 4);

                reg_values[depth - 1].size =
                    ((uintptr_t)fdt_be32(value + 8) << 32) |
                    (uintptr_t)fdt_be32(value + 12);

                if (len >= 32) {
                    reg_values[depth - 1].base2 =
                        ((uintptr_t)fdt_be32(value + 16) << 32) |
                        (uintptr_t)fdt_be32(value + 20);

                    reg_values[depth - 1].size2 =
                        ((uintptr_t)fdt_be32(value + 24) << 32) |
                        (uintptr_t)fdt_be32(value + 28);
                }

                reg_found[depth - 1] = 1;
            }

            p = fdt_align4(p + len);
            continue;
        }

        if (token == FDT_NOP)
            continue;

        if (token == FDT_END)
            break;

        return -11;
    }

    return -12;
}

int fdt_find_timer_virtual_irq(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    uint32_t *irq)
{
    uintptr_t p;
    uintptr_t struct_start;
    uintptr_t struct_end;
    uintptr_t strings_start;
    uintptr_t strings_end;

    uint32_t token;
    uint32_t len;
    uint32_t nameoff;
    uint32_t depth = 0;

    uint8_t timer_found[FDT_MAX_DEPTH];
    uint8_t irq_found[FDT_MAX_DEPTH];
    uint32_t timer_irq[FDT_MAX_DEPTH];

    if (dtb == 0 || info == 0 || irq == 0)
        return -1;

    *irq = 0;

    struct_start = dtb + info->off_dt_struct;
    struct_end = struct_start + info->size_dt_struct;
    strings_start = dtb + info->off_dt_strings;
    strings_end = strings_start + info->size_dt_strings;
    p = struct_start;

    while (p + 4 <= struct_end) {
        token = fdt_be32(p);
        p += 4;

        if (token == FDT_BEGIN_NODE) {
            if (depth >= FDT_MAX_DEPTH)
                return -2;

            while (p < struct_end &&
                   *(const uint8_t *)p != '\0')
                p++;

            if (p >= struct_end)
                return -3;

            timer_found[depth] = 0;
            irq_found[depth] = 0;
            timer_irq[depth] = 0;

            depth++;
            p++;
            p = fdt_align4(p);
            continue;
        }

        if (token == FDT_END_NODE) {
            if (depth == 0)
                return -4;

            depth--;

            if (timer_found[depth] &&
                irq_found[depth]) {
                *irq = timer_irq[depth];
                return 0;
            }

            continue;
        }

        if (token == FDT_PROP) {
            uintptr_t value;
            const char *property_name;

            if (p + 8 > struct_end)
                return -5;

            if (depth == 0)
                return -6;

            len = fdt_be32(p);
            nameoff = fdt_be32(p + 4);
            p += 8;

            if (nameoff >= info->size_dt_strings)
                return -7;

            property_name =
                (const char *)(strings_start + nameoff);

            if (!fdt_string_valid(
                    (uintptr_t)property_name,
                    strings_end))
                return -8;

            if (len > (uint32_t)(struct_end - p))
                return -9;

            value = p;

            if (fdt_name_equal(property_name, "compatible")) {
                if (fdt_compatible_contains(
                        value,
                        len,
                        "arm,armv8-timer") ||
                    fdt_compatible_contains(
                        value,
                        len,
                        "arm,armv7-timer"))
                    timer_found[depth - 1] = 1;
            }

            if (fdt_name_equal(property_name, "interrupts")) {
                /*
                 * QEMU virt GICv2 emits four 3-cell timer
                 * interrupt specifiers. The third entry is
                 * the virtual timer PPI. In the DT binding a
                 * PPI is represented by the 0..15 PPI number;
                 * the GIC INTID is PPI + 16.
                 */
                if (len >= 36) {
                    uint32_t type = fdt_be32(value + 24);
                    uint32_t ppi = fdt_be32(value + 28);

                    if (type == 1 && ppi < 16) {
                        timer_irq[depth - 1] = ppi + 16;
                        irq_found[depth - 1] = 1;
                    }
                }
            }

            p = fdt_align4(p + len);
            continue;
        }

        if (token == FDT_NOP)
            continue;

        if (token == FDT_END)
            break;

        return -10;
    }

    return -11;
}

/*
 * Find the first interrupt of a DT node selected by compatible.
 *
 * This implementation intentionally supports the GICv2 binding used
 * by the QEMU virt,gic-version=2 laboratory target:
 *
 *   interrupts = <type hwirq flags>
 *
 * type 0 = SPI  -> Linux-style INTID = hwirq + 32
 * type 1 = PPI  -> Linux-style INTID = hwirq + 16
 *
 * This is deliberately not a generic interrupt-domain resolver.
 */
int fdt_find_compatible_irq(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const char *compatible,
    struct fdt_irq *irq)
{
    uintptr_t p;
    uintptr_t struct_start;
    uintptr_t struct_end;
    uintptr_t strings_start;
    uintptr_t strings_end;

    uint32_t token;
    uint32_t len;
    uint32_t nameoff;
    uint32_t depth = 0;

    uint8_t compat_found[FDT_MAX_DEPTH];
    uint8_t irq_found[FDT_MAX_DEPTH];
    struct fdt_irq irq_values[FDT_MAX_DEPTH];

    if (dtb == 0 ||
        info == (void *)0 ||
        compatible == (void *)0 ||
        irq == (void *)0)
        return -1;

    irq->irq = 0;
    irq->flags = 0;

    struct_start = dtb + info->off_dt_struct;
    struct_end = struct_start + info->size_dt_struct;

    strings_start = dtb + info->off_dt_strings;
    strings_end = strings_start + info->size_dt_strings;

    p = struct_start;

    while (p + 4 <= struct_end) {
        token = fdt_be32(p);
        p += 4;

        if (token == FDT_BEGIN_NODE) {
            if (depth >= FDT_MAX_DEPTH)
                return -2;

            while (p < struct_end &&
                   *(const uint8_t *)p != '\0')
                p++;

            if (p >= struct_end)
                return -3;

            compat_found[depth] = 0;
            irq_found[depth] = 0;

            irq_values[depth].irq = 0;
            irq_values[depth].flags = 0;

            depth++;

            p++;
            p = fdt_align4(p);

            if (p > struct_end)
                return -4;

            continue;
        }

        if (token == FDT_END_NODE) {
            if (depth == 0)
                return -5;

            depth--;

            if (compat_found[depth] &&
                irq_found[depth]) {

                irq->irq = irq_values[depth].irq;
                irq->flags = irq_values[depth].flags;

                return 0;
            }

            continue;
        }

        if (token == FDT_PROP) {
            uintptr_t value;
            const char *property_name;

            if (depth == 0)
                return -6;

            if (p + 8 > struct_end)
                return -7;

            len = fdt_be32(p);
            nameoff = fdt_be32(p + 4);
            p += 8;

            if (nameoff >= info->size_dt_strings)
                return -8;

            property_name =
                (const char *)(strings_start + nameoff);

            if (!fdt_string_valid(
                    (uintptr_t)property_name,
                    strings_end))
                return -9;

            if (len > (uint32_t)(struct_end - p))
                return -10;

            value = p;

            if (fdt_name_equal(
                    property_name,
                    "compatible")) {

                if (fdt_compatible_contains(
                        value,
                        len,
                        compatible))
                    compat_found[depth - 1] = 1;
            }

            if (fdt_name_equal(
                    property_name,
                    "interrupts")) {

                uint32_t type;
                uint32_t hwirq;
                uint32_t flags;

                /*
                 * GICv2 interrupt specifier:
                 *
                 *   cell 0 = type
                 *   cell 1 = interrupt number
                 *   cell 2 = trigger/level flags
                 */
                if (len != 12)
                    return -11;

                type = fdt_be32(value);
                hwirq = fdt_be32(value + 4);
                flags = fdt_be32(value + 8);

                if (type == 0) {
                    /* SPI */
                    if (hwirq >= 988)
                        return -12;

                    irq_values[depth - 1].irq =
                        hwirq + 32;
                } else if (type == 1) {
                    /* PPI */
                    if (hwirq >= 16)
                        return -13;

                    irq_values[depth - 1].irq =
                        hwirq + 16;
                } else {
                    return -14;
                }

                irq_values[depth - 1].flags = flags;
                irq_found[depth - 1] = 1;
            }

            p = fdt_align4(p + len);
            continue;
        }

        if (token == FDT_NOP)
            continue;

        if (token == FDT_END)
            break;

        return -15;
    }

    return -16;
}
