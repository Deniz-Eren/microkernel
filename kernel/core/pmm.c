#include <kernel/pmm.h>
#include <kernel/multiboot.h>
#include <stddef.h>
#include <stdint.h>
#include <kernel/printk.h>

/* Bitmap frame allocator: bit 1 = free, bit 0 = reserved/allocated.
 * Covers up to 4 GiB of physical memory (QEMU tests use 256 MiB). */
#define PMM_MAX_BYTES (4ULL * 1024 * 1024 * 1024)
#define FRAME_SIZE 4096ULL
#define BITMAP_WORDS (PMM_MAX_BYTES / FRAME_SIZE / 64)

static uint64_t g_bitmap[BITMAP_WORDS];
static uint64_t g_total_frames;
static uint64_t g_free_frames;
static uint64_t g_rover; /* scan hint */

static inline int bit_test(uint64_t idx) {
    return (g_bitmap[idx / 64] >> (idx % 64)) & 1;
}

static inline void bit_clear(uint64_t idx) {
    g_bitmap[idx / 64] &= ~(1ULL << (idx % 64));
}

static inline void bit_set(uint64_t idx) {
    g_bitmap[idx / 64] |= (1ULL << (idx % 64));
}

void pmm_reserve(uint64_t base, uint64_t len) {
    uint64_t start = base / FRAME_SIZE;
    uint64_t end = (base + len + FRAME_SIZE - 1) / FRAME_SIZE;
    if (end > g_total_frames) {
        end = g_total_frames;
    }
    for (uint64_t i = start; i < end; i++) {
        if (bit_test(i)) {
            bit_clear(i);
            g_free_frames--;
        }
    }
}

uint64_t pmm_total_bytes(void) {
    return g_total_frames * FRAME_SIZE;
}

uint64_t pmm_free_bytes(void) {
    return g_free_frames * FRAME_SIZE;
}

void pmm_init(void) {
    size_t count;
    const mb_mmap_entry_t *mmap = mb_mmap(&count);

    /* Determine usable top of memory. */
    for (size_t i = 0; i < count; i++) {
        if (mmap[i].type == MB_MMAP_USABLE) {
            uint64_t top = mmap[i].base + mmap[i].len;
            if (top > PMM_MAX_BYTES) {
                top = PMM_MAX_BYTES;
            }
            if (top / FRAME_SIZE > g_total_frames) {
                g_total_frames = top / FRAME_SIZE;
            }
        }
    }

    /* Mark all frames free, then beyond-total used. */
    for (uint64_t i = 0; i < g_total_frames; i++) {
        bit_set(i);
    }
    g_free_frames = g_total_frames;
    for (uint64_t i = g_total_frames; i < BITMAP_WORDS * 64; i++) {
        bit_clear(i);
    }

    /* Reserve everything not covered by usable mmap regions. */
    uint64_t cursor = 0;
    for (size_t i = 0; i < count; i++) {
        if (mmap[i].type != MB_MMAP_USABLE) {
            continue;
        }
        if (mmap[i].base > cursor) {
            pmm_reserve(cursor, mmap[i].base - cursor);
        }
        cursor = mmap[i].base + mmap[i].len;
        if (cursor > PMM_MAX_BYTES) {
            cursor = PMM_MAX_BYTES;
            break;
        }
    }
    if (cursor < g_total_frames * FRAME_SIZE) {
        pmm_reserve(cursor, g_total_frames * FRAME_SIZE - cursor);
    }

    /* Kernel-owned regions. */
    extern char __kernel_phys_start[], __kernel_phys_end[];
    pmm_reserve((uint64_t)(uintptr_t)__kernel_phys_start,
                (uint64_t)(uintptr_t)__kernel_phys_end - (uint64_t)(uintptr_t)__kernel_phys_start);
    pmm_reserve(0, 0x100000); /* low memory: BIOS, real-mode IVT, EBDA */

    /* The Multiboot2 info structure itself (64 KiB window). */
    uint64_t mbi = multiboot_mbi_phys();
    if (mbi) {
        pmm_reserve(mbi & ~0xFFFULL, 0x10000);
    }
}

uint64_t pmm_alloc_frame(void) {
    if (g_free_frames == 0) {
        return 0;
    }
    for (uint64_t w = 0; w < BITMAP_WORDS; w++) {
        uint64_t word = g_bitmap[(g_rover + w) % BITMAP_WORDS];
        if (!word) {
            continue;
        }
        uint64_t idx = ((g_rover + w) % BITMAP_WORDS) * 64 + __builtin_ctzll(word);
        if (idx >= g_total_frames) {
            continue;
        }
        bit_clear(idx);
        g_free_frames--;
        g_rover = (idx / 64 + 1) % BITMAP_WORDS;
        return idx * FRAME_SIZE;
    }
    return 0;
}

int pmm_alloc_contiguous(size_t count, uint64_t *out_base) {
    if (count == 0) {
        return -1;
    }
    uint64_t run = 0;
    for (uint64_t i = 0; i < g_total_frames; i++) {
        if (bit_test(i)) {
            run++;
            if (run == count) {
                uint64_t base = i + 1 - count;
                for (uint64_t j = 0; j < count; j++) {
                    bit_clear(base + j);
                }
                g_free_frames -= count;
                *out_base = base * FRAME_SIZE;
                return 0;
            }
        } else {
            run = 0;
        }
    }
    return -1;
}

void pmm_free_frame(uint64_t phys) {
    uint64_t idx = phys / FRAME_SIZE;
    if (phys % FRAME_SIZE || idx >= g_total_frames || bit_test(idx)) {
        return;
    }
    bit_set(idx);
    g_free_frames++;
}
