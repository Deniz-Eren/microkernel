// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#include <kernel/multiboot.h>
#include <stddef.h>
#include <kernel/printk.h>
#include <kernel/vmm.h>

/* need:: COMP_BOOT_0001 */

#define MAX_MMAP 32
#define MAX_MODULES 8

static mb_mmap_entry_t g_mmap[MAX_MMAP];
static size_t g_mmap_count;
static mb_module_t g_modules[MAX_MODULES];
static size_t g_module_count;
static const char *g_cmdline;
static uint64_t g_mbi_phys;
static uint64_t g_usable_total;

typedef struct {
    uint32_t type;
    uint32_t size;
} mb_tag_t;

typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    /* followed by cmdline string */
} mb_module_tag_t;

uint64_t multiboot_mbi_phys(void) {
    return g_mbi_phys;
}

void multiboot_parse(uint64_t mbi_phys) {
    g_mbi_phys = mbi_phys;
    const mb_tag_t *tag = (const mb_tag_t *)phys_to_virt(mbi_phys + 8);
    for (; tag->type != 0; tag = (const mb_tag_t *)((uint8_t *)tag + ((tag->size + 7) & ~7U))) {
        switch (tag->type) {
        case 1: /* boot cmdline */
            g_cmdline = (const char *)(tag + 1);
            break;
        case 3: { /* module */
            if (g_module_count < MAX_MODULES) {
                const mb_module_tag_t *m = (const mb_module_tag_t *)(tag + 1);
                mb_module_t *out = &g_modules[g_module_count++];
                out->start = m->mod_start;
                out->end = m->mod_end;
                out->cmdline = (const char *)(m + 1);
            }
            break;
        }
        case 6: { /* memory map */
            const uint32_t *hdr = (const uint32_t *)(tag + 1);
            uint32_t entry_size = hdr[0];
            const uint8_t *p = (const uint8_t *)(hdr + 2);
            const uint8_t *end = (const uint8_t *)tag + tag->size;
            for (; p < end; p += entry_size) {
                uint64_t base, len;
                uint32_t type;
                __builtin_memcpy(&base, p, 8);
                __builtin_memcpy(&len, p + 8, 8);
                __builtin_memcpy(&type, p + 16, 4);
                if (g_mmap_count < MAX_MMAP) {
                    mb_mmap_entry_t *e = &g_mmap[g_mmap_count++];
                    e->base = base;
                    e->len = len;
                    e->type = type;
                    if (type == MB_MMAP_USABLE) {
                        g_usable_total += len;
                    }
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

const mb_mmap_entry_t *mb_mmap(size_t *count) {
    *count = g_mmap_count;
    return g_mmap;
}

const mb_module_t *mb_modules(size_t *count) {
    *count = g_module_count;
    return g_modules;
}

const char *mb_cmdline(void) {
    return g_cmdline ? g_cmdline : "";
}

uint64_t mb_total_usable(void) {
    return g_usable_total;
}
