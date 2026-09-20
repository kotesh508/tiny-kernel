#ifndef FDT_H
#define FDT_H

#include <stdint.h>

#define FDT_MAGIC       0xd00dfeedU
#define FDT_BEGIN_NODE  0x00000001U
#define FDT_END_NODE    0x00000002U
#define FDT_PROP        0x00000003U
#define FDT_NOP         0x00000004U
#define FDT_END         0x00000009U

struct fdt_header_info {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

struct fdt_reg {
    uintptr_t base;
    uintptr_t size;
    uintptr_t base2;
    uintptr_t size2;
};

int fdt_read_header(uintptr_t dtb,
                    struct fdt_header_info *info);

int fdt_dump_root_properties(
    uintptr_t dtb,
    const struct fdt_header_info *info);

int fdt_dump_node_names(
    uintptr_t dtb,
    const struct fdt_header_info *info);

int fdt_dump_named_node_properties(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const char *target_name);

int fdt_find_compatible_reg(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const char *compatible,
    struct fdt_reg *reg);

int fdt_find_timer_virtual_irq(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    uint32_t *irq);

#endif
