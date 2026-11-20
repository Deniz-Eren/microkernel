/**
 * @file     kheap.c
 * @brief    Kernel virtual heap first-fit dynamic memory allocator
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/kheap.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <stddef.h>
#include <stdint.h>
#include <kernel/printk.h>
#include <kernel/panic.h>

/* need:: COMP_MEM_0001 */

/* First-fit free-list allocator growing over a kernel VA window at
 * KERNEL_HEAP_BASE. Pages are mapped on demand from the PMM. */

#define HEAP_MAGIC 0x3130504145484B4DULL /* "MKHEAP01" */
#define HEAP_MAX_SIZE (64ULL * 1024 * 1024)
#define MIN_BLOCK 32

typedef struct kblock {
    uint64_t magic;
    uint64_t size; /* total size incl. header */
    struct kblock *next_free;
    uint64_t used;
} kblock_t;

static uint8_t *g_heap_cursor;
static kblock_t *g_free_head;
static uint64_t g_heap_mapped;

static void heap_grow(size_t need) {
    while (g_heap_mapped < HEAP_MAX_SIZE &&
           (g_heap_cursor + need) >
               (uint8_t *)(KERNEL_HEAP_BASE + g_heap_mapped)) {
        uint64_t frame = pmm_alloc_frame();
        if (!frame) {
            panic("kheap: out of memory");
        }
        uint64_t va = KERNEL_HEAP_BASE + g_heap_mapped;
        vmm_map_page(va, frame, VMM_PRESENT | VMM_WRITE | VMM_NX);
        g_heap_mapped += 4096;
    }
    if ((g_heap_cursor + need) >
        (uint8_t *)(KERNEL_HEAP_BASE + HEAP_MAX_SIZE)) {
        panic("kheap: heap exhausted");
    }
}

void kheap_init(void) {
    g_heap_cursor = (uint8_t *)KERNEL_HEAP_BASE;
    g_free_head = NULL;
    g_heap_mapped = 0;
    printk("[kheap] heap window at %llx\n",
           (unsigned long long)KERNEL_HEAP_BASE);
}

void *kmalloc(size_t n) {
    if (n == 0) {
        n = 1;
    }
    size_t total = (n + sizeof(kblock_t) + 15) & ~(size_t)15;
    if (total < MIN_BLOCK) {
        total = MIN_BLOCK;
    }

    /* First fit in the free list. */
    kblock_t **prev = &g_free_head;
    for (kblock_t *b = g_free_head; b; prev = &b->next_free, b = b->next_free) {
        if (b->size >= total) {
            /* Split if the remainder is large enough. */
            if (b->size >= total + MIN_BLOCK) {
                uint8_t *raw = (uint8_t *)b;
                kblock_t *rest = (kblock_t *)(raw + total);
                rest->magic = HEAP_MAGIC;
                rest->size = b->size - total;
                rest->used = 0;
                rest->next_free = b->next_free;
                *prev = rest;
                b->size = total;
            } else {
                *prev = b->next_free;
            }
            b->used = 1;
            b->next_free = NULL;
            return (uint8_t *)b + sizeof(kblock_t);
        }
    }

    /* Bump-allocate a fresh block. */
    heap_grow(total);
    kblock_t *b = (kblock_t *)g_heap_cursor;
    g_heap_cursor += total;
    b->magic = HEAP_MAGIC;
    b->size = total;
    b->used = 1;
    b->next_free = NULL;
    return (uint8_t *)b + sizeof(kblock_t);
}

void *kzalloc(size_t n) {
    void *p = kmalloc(n);
    if (p) {
        uint8_t *q = (uint8_t *)p;
        for (size_t i = 0; i < n; i++) {
            q[i] = 0;
        }
    }
    return p;
}

void kfree(void *p) {
    if (!p) {
        return;
    }
    kblock_t *b = (kblock_t *)((uint8_t *)p - sizeof(kblock_t));
    if (b->magic != HEAP_MAGIC || !b->used) {
        panic("kheap: kfree of bad pointer %p", p);
    }
    b->used = 0;

    /* Insert into address-sorted free list, coalescing both neighbours. */
    kblock_t *prevb = NULL;
    kblock_t *cur = g_free_head;
    while (cur && cur < b) {
        prevb = cur;
        cur = cur->next_free;
    }
    b->next_free = cur;
    if (prevb) {
        prevb->next_free = b;
    } else {
        g_free_head = b;
    }

    if ((uint8_t *)b + b->size == (uint8_t *)cur) {
        b->size += cur->size;
        b->next_free = cur->next_free;
    }
    if (prevb && (uint8_t *)prevb + prevb->size == (uint8_t *)b) {
        prevb->size += b->size;
        prevb->next_free = b->next_free;
    }
}
