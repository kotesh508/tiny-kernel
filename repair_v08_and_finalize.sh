#!/usr/bin/env bash
set -euo pipefail

REPO="$HOME/project_Tiny_kernel/tiny-kernel"
DOCS_SRC="${DOCS_SRC:-$HOME/project_Tiny_kernel/v08_release_package}"
cd "$REPO"

echo "===== v0.8 CONTINUE FINALIZATION ====="

if git rev-parse --verify --quiet refs/heads/v0.8-dtb-gic >/dev/null; then
    git switch v0.8-dtb-gic
else
    git switch -c v0.8-dtb-gic
fi

cp -f kernel/fdt.c /tmp/tiny_kernel_fdt.c.before_final_fix
cp -f kernel/main.c /tmp/tiny_kernel_main.c.before_final_fix

python3 - <<'PY'
from pathlib import Path
import re

repo = Path.home() / "project_Tiny_kernel" / "tiny-kernel"

# Restore the FDT scanner that was already working before the finalization script.
p = repo / "kernel/fdt.c"
s = p.read_text()

start = s.find("int fdt_find_compatible_reg(")
end = s.find("int fdt_find_timer_virtual_irq(", start)
if start < 0 or end < 0:
    raise SystemExit("ERROR: FDT function boundaries not found.")

working_fdt = r'''int fdt_find_compatible_reg(
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
                *reg = reg_values[depth];
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

'''
p.write_text(s[:start] + working_fdt + s[end:])

# Patch main.c without assuming exact whitespace or ordering.
p = repo / "kernel/main.c"
s = p.read_text()

s = re.sub(
    r'extern\s+void\s+gic_init\s*\(\s*void\s*\)\s*;',
    '''extern void gic_init(uintptr_t distributor_base,
                     uintptr_t cpu_interface_base,
                     uint32_t timer_irq);''',
    s,
    count=1
)

if "struct fdt_reg gic_reg;" not in s:
    m = re.search(r'^(\s*)struct\s+fdt_reg\s+uart_reg\s*;\s*$', s, re.M)
    if not m:
        raise SystemExit("ERROR: uart_reg declaration not found")
    indent = m.group(1)
    replacement = m.group(0) + (
        f"\n{indent}struct fdt_reg gic_reg;"
        f"\n{indent}uint32_t timer_irq;"
    )
    s = s[:m.start()] + replacement + s[m.end():]

dynamic_block = r'''    if (fdt_find_compatible_reg(
            dtb,
            &fdt_info,
            "arm,cortex-a15-gic",
            &gic_reg) == 0 &&
        gic_reg.base != 0 &&
        gic_reg.base2 != 0) {
        uart_puts("DTB GIC DISCOVERY PASS\r\n");
    } else {
        uart_puts("DTB GIC DISCOVERY FAIL\r\n");
        for (;;) asm volatile("wfe");
    }

    if (fdt_find_timer_virtual_irq(
            dtb,
            &fdt_info,
            &timer_irq) == 0) {
        uart_puts("DTB TIMER DISCOVERY PASS\r\n");
    } else {
        uart_puts("DTB TIMER DISCOVERY FAIL\r\n");
        for (;;) asm volatile("wfe");
    }

    gic_init(gic_reg.base, gic_reg.base2, timer_irq);'''

if re.search(r'gic_init\s*\(\s*\)\s*;', s):
    s = re.sub(r'gic_init\s*\(\s*\)\s*;', dynamic_block, s, count=1)
else:
    raise SystemExit("ERROR: zero-argument gic_init() call not found")

p.write_text(s)
PY

echo "===== SOURCE CHECK ====="
grep -n -A5 -B2 "gic_init" kernel/main.c
grep -n -A7 -B2 "void gic_init" kernel/gic.c | head -30
grep -n "fdt_find_timer_virtual_irq" include/fdt.h kernel/fdt.c

echo "===== BUILD ====="
make clean
make

echo "===== RUNTIME REGRESSION ====="
LOG=/tmp/tiny_kernel_v08_runtime.log
rm -f "$LOG"

timeout 15s qemu-system-aarch64 \
    -M virt,gic-version=2 \
    -cpu cortex-a53 \
    -m 128M \
    -nographic \
    -monitor none \
    -kernel kernel.bin >"$LOG" 2>&1 || true

cat "$LOG"

required=(
    "DTB UART DISCOVERY PASS"
    "DTB GIC DISCOVERY PASS"
    "DTB TIMER DISCOVERY PASS"
    "FDT RESERVATION PASS"
    "TASK CREATE PASS"
    "TASK RUN PASS"
    "TASK DESTROY PASS"
    "FREE/REUSE PASS"
    "NEGATIVE TEST PASS"
    "EXHAUSTION PASS"
    "TICK"
)

for x in "${required[@]}"; do
    grep -Fq "$x" "$LOG" || {
        echo "ERROR: runtime acceptance missing: $x"
        echo "v0.8 NOT finalized. No commit/tag/push performed."
        exit 1
    }
done

echo "===== COPY EXISTING V0.8 DOCUMENTATION ====="
mkdir -p docs
if [ -d "$DOCS_SRC" ]; then
    cp -f "$DOCS_SRC"/README.md docs/ 2>/dev/null || true
    cp -f "$DOCS_SRC"/V0.8_RELEASE_GATE.md docs/ 2>/dev/null || true
    cp -f "$DOCS_SRC"/Tiny_Kernel_*.docx docs/ 2>/dev/null || true
fi
cp -f "$LOG" docs/v0.8_runtime_regression.log

# The earlier helper was copied into the repository during debugging; it is not part of the kernel release.
rm -f repair_v08_and_finalize.sh continue_v08_finalization.sh 2>/dev/null || true

echo "===== GIT FINALIZATION ====="
git diff --check
git add .
git commit -m "v0.8: DTB-driven UART GIC and timer discovery"
git tag -a v0.8-final -m "Tiny Kernel v0.8 final"
git push origin v0.8-dtb-gic
git push origin v0.8-final

echo "===== v0.8 FINALIZED ====="
git log --oneline --decorate -3
git status --short --branch

