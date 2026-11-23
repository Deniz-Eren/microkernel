/**
 * @file     malloc.c
 * @brief    Thread-safe mmap-backed coalescing slab and large-block allocator
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/microkernel.h>
#include <sys/mman.h>

/* need:: COMP_MEM_0004 COMP_LIBC_0009 */

#define HEAP_PAGE_SIZE 4096u
#define HEAP_ALIGN 16u
#define ARENA_SLAB_SIZE (64u * 1024u)
#define LARGE_THRESHOLD (32u * 1024u)
#define BLOCK_MAGIC 0x4D4B484541503031ULL

#define BLK_FLAG_FREE (1u << 0)
#define BLK_FLAG_LARGE (1u << 1)
#define BLK_FLAG_ALIGNED (1u << 2)

struct arena_slab {
    size_t map_size;
    size_t live_blocks;
};

struct block_hdr {
    uint64_t magic;
    uint32_t flags;
    uint32_t reserved;
    size_t size;
    size_t req_size;
    struct arena_slab *slab;
    struct block_hdr *prev_phys;
    struct block_hdr *next_phys;
    struct block_hdr *prev_free;
    struct block_hdr *next_free;
    uint64_t pad;
};

_Static_assert((sizeof(struct arena_slab) & (HEAP_ALIGN - 1u)) == 0u,
               "arena_slab must preserve 16-byte alignment");
_Static_assert((sizeof(struct block_hdr) & (HEAP_ALIGN - 1u)) == 0u,
               "block_hdr must preserve 16-byte alignment");

/* Futex state: 0 = unlocked, 1 = locked uncontended, 2 = locked contended. */
static uint32_t g_heap_lock;
static struct block_hdr *g_free_head;
static size_t g_empty_slabs;

static void heap_lock(void) {
    uint32_t c = 0u;
    if (__atomic_compare_exchange_n(&g_heap_lock, &c, 1u, 0, __ATOMIC_ACQUIRE,
                                    __ATOMIC_RELAXED)) {
        return;
    }
    if (c != 2u) {
        c = __atomic_exchange_n(&g_heap_lock, 2u, __ATOMIC_ACQUIRE);
    }
    while (c != 0u) {
        (void)__sys5(SYS_FUTEX, (long)(uintptr_t)&g_heap_lock, FUTEX_WAIT, 2, 0,
                     0);
        c = __atomic_exchange_n(&g_heap_lock, 2u, __ATOMIC_ACQUIRE);
    }
}

static void heap_unlock(void) {
    if (__atomic_exchange_n(&g_heap_lock, 0u, __ATOMIC_RELEASE) == 2u) {
        (void)__sys5(SYS_FUTEX, (long)(uintptr_t)&g_heap_lock, FUTEX_WAKE, 1, 0,
                     0);
    }
}

static size_t align_up(size_t n, size_t align) {
    size_t mask = align - 1u;
    if (n > ((size_t)-1) - mask) {
        return 0u;
    }
    return (n + mask) & ~mask;
}

static void free_list_insert(struct block_hdr *blk) {
    blk->prev_free = NULL;
    blk->next_free = g_free_head;
    if (g_free_head) {
        g_free_head->prev_free = blk;
    }
    g_free_head = blk;
}

static void free_list_remove(struct block_hdr *blk) {
    if (blk->prev_free) {
        blk->prev_free->next_free = blk->next_free;
    } else if (g_free_head == blk) {
        g_free_head = blk->next_free;
    }
    if (blk->next_free) {
        blk->next_free->prev_free = blk->prev_free;
    }
    blk->prev_free = NULL;
    blk->next_free = NULL;
}

static void split_tail(struct block_hdr *blk, size_t need) {
    size_t min_split = need + sizeof(struct block_hdr) + HEAP_ALIGN;
    if (blk->size < min_split) {
        return;
    }
    char *payload = (char *)(blk + 1);
    struct block_hdr *rem = (struct block_hdr *)(void *)(payload + need);
    rem->magic = BLOCK_MAGIC;
    rem->flags = BLK_FLAG_FREE;
    rem->reserved = 0u;
    rem->size = blk->size - need - sizeof(struct block_hdr);
    rem->req_size = 0u;
    rem->slab = blk->slab;
    rem->prev_phys = blk;
    rem->next_phys = blk->next_phys;
    rem->prev_free = NULL;
    rem->next_free = NULL;
    if (blk->next_phys) {
        blk->next_phys->prev_phys = rem;
    }
    blk->next_phys = rem;
    blk->size = need;

    if (rem->next_phys && (rem->next_phys->flags & BLK_FLAG_FREE)) {
        struct block_hdr *nxt = rem->next_phys;
        free_list_remove(nxt);
        rem->size += sizeof(struct block_hdr) + nxt->size;
        rem->next_phys = nxt->next_phys;
        if (rem->next_phys) {
            rem->next_phys->prev_phys = rem;
        }
        nxt->magic = 0u;
    }
    free_list_insert(rem);
}

static void *alloc_large(size_t n) {
    if (n > ((size_t)-1) - sizeof(struct block_hdr)) {
        errno = ENOMEM;
        return NULL;
    }
    size_t total = align_up(n + sizeof(struct block_hdr), HEAP_PAGE_SIZE);
    if (total == 0u) {
        errno = ENOMEM;
        return NULL;
    }
    void *raw = mmap(NULL, total, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) {
        errno = ENOMEM;
        return NULL;
    }
    struct block_hdr *blk = (struct block_hdr *)raw;
    blk->magic = BLOCK_MAGIC;
    blk->flags = BLK_FLAG_LARGE;
    blk->reserved = 0u;
    blk->size = total - sizeof(struct block_hdr);
    blk->req_size = n;
    blk->slab = NULL;
    blk->prev_phys = NULL;
    blk->next_phys = NULL;
    blk->prev_free = NULL;
    blk->next_free = NULL;
    return (void *)(blk + 1);
}

static struct block_hdr *alloc_new_slab(size_t need, size_t req_size) {
    void *raw = mmap(NULL, ARENA_SLAB_SIZE, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) {
        return NULL;
    }
    struct arena_slab *slab = (struct arena_slab *)raw;
    slab->map_size = ARENA_SLAB_SIZE;
    slab->live_blocks = 1u;

    struct block_hdr *blk = (struct block_hdr *)(void *)(slab + 1);
    blk->magic = BLOCK_MAGIC;
    blk->flags = 0u;
    blk->reserved = 0u;
    blk->size =
        ARENA_SLAB_SIZE - sizeof(struct arena_slab) - sizeof(struct block_hdr);
    blk->req_size = req_size;
    blk->slab = slab;
    blk->prev_phys = NULL;
    blk->next_phys = NULL;
    blk->prev_free = NULL;
    blk->next_free = NULL;

    heap_lock();
    split_tail(blk, need);
    heap_unlock();
    return blk;
}

void *malloc(size_t n) {
    if (n >= LARGE_THRESHOLD) {
        return alloc_large(n);
    }
    size_t req = (n == 0u) ? HEAP_ALIGN : n;
    size_t need = align_up(req, HEAP_ALIGN);
    if (need == 0u) {
        errno = ENOMEM;
        return NULL;
    }

    heap_lock();
    for (struct block_hdr *blk = g_free_head; blk; blk = blk->next_free) {
        if (blk->size >= need) {
            free_list_remove(blk);
            if (blk->slab->live_blocks == 0u && g_empty_slabs > 0u) {
                g_empty_slabs--;
            }
            blk->slab->live_blocks++;
            blk->flags = 0u;
            blk->req_size = n;
            split_tail(blk, need);
            heap_unlock();
            return (void *)(blk + 1);
        }
    }
    heap_unlock();

    struct block_hdr *fresh = alloc_new_slab(need, n);
    if (!fresh) {
        errno = ENOMEM;
        return NULL;
    }
    return (void *)(fresh + 1);
}

void free(void *p) {
    if (!p) {
        return;
    }
    int saved_errno = errno;
    struct block_hdr *blk = ((struct block_hdr *)p) - 1;
    if (blk->magic != BLOCK_MAGIC || (blk->flags & BLK_FLAG_FREE) != 0u) {
        errno = saved_errno;
        return;
    }
    if (blk->flags & BLK_FLAG_ALIGNED) {
        void *raw = (void *)blk->prev_phys;
        blk->magic = 0u;
        free(raw);
        errno = saved_errno;
        return;
    }
    if (blk->flags & BLK_FLAG_LARGE) {
        size_t map_size = sizeof(struct block_hdr) + blk->size;
        blk->magic = 0u;
        (void)munmap((void *)blk, map_size);
        errno = saved_errno;
        return;
    }

    void *unmap_ptr = NULL;
    size_t unmap_size = 0u;

    heap_lock();
    blk->flags |= BLK_FLAG_FREE;
    blk->req_size = 0u;
    struct arena_slab *slab = blk->slab;
    if (slab->live_blocks > 0u) {
        slab->live_blocks--;
    }

    if (blk->next_phys && (blk->next_phys->flags & BLK_FLAG_FREE)) {
        struct block_hdr *nxt = blk->next_phys;
        free_list_remove(nxt);
        blk->size += sizeof(struct block_hdr) + nxt->size;
        blk->next_phys = nxt->next_phys;
        if (blk->next_phys) {
            blk->next_phys->prev_phys = blk;
        }
        nxt->magic = 0u;
    }

    if (blk->prev_phys && (blk->prev_phys->flags & BLK_FLAG_FREE)) {
        struct block_hdr *prv = blk->prev_phys;
        free_list_remove(prv);
        prv->size += sizeof(struct block_hdr) + blk->size;
        prv->next_phys = blk->next_phys;
        if (prv->next_phys) {
            prv->next_phys->prev_phys = prv;
        }
        blk->magic = 0u;
        blk = prv;
    }

    if (slab->live_blocks == 0u) {
        if (g_empty_slabs >= 1u) {
            blk->magic = 0u;
            unmap_ptr = (void *)slab;
            unmap_size = slab->map_size;
        } else {
            g_empty_slabs = 1u;
            free_list_insert(blk);
        }
    } else {
        free_list_insert(blk);
    }
    heap_unlock();

    if (unmap_ptr) {
        (void)munmap(unmap_ptr, unmap_size);
    }
    errno = saved_errno;
}

void *calloc(size_t nelem, size_t elsize) {
    if (nelem != 0u && elsize > ((size_t)-1) / nelem) {
        errno = ENOMEM;
        return NULL;
    }
    size_t total = nelem * elsize;
    if (total >= LARGE_THRESHOLD) {
        /* Fresh anonymous mmap pages are already zero-filled by the kernel. */
        return alloc_large(total);
    }
    void *p = malloc(total);
    if (p && total > 0u) {
        memset(p, 0, total);
    }
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }
    if (size == 0u) {
        free(ptr);
        return NULL;
    }
    struct block_hdr *blk = ((struct block_hdr *)ptr) - 1;
    if (blk->magic != BLOCK_MAGIC || (blk->flags & BLK_FLAG_FREE) != 0u) {
        errno = EINVAL;
        return NULL;
    }
    size_t old_req = blk->req_size;

    if (blk->flags & BLK_FLAG_ALIGNED) {
        void *np = malloc(size);
        if (!np) {
            return NULL;
        }
        size_t copy_n = (old_req < size) ? old_req : size;
        if (copy_n > 0u) {
            memcpy(np, ptr, copy_n);
        }
        free(ptr);
        return np;
    }
    if (blk->flags & BLK_FLAG_LARGE) {
        if (size <= blk->size && size >= (LARGE_THRESHOLD / 2u)) {
            blk->req_size = size;
            return ptr;
        }
    } else if (size < LARGE_THRESHOLD) {
        size_t need = align_up(size, HEAP_ALIGN);
        if (need == 0u) {
            errno = ENOMEM;
            return NULL;
        }
        heap_lock();
        if (need <= blk->size) {
            blk->req_size = size;
            split_tail(blk, need);
            heap_unlock();
            return ptr;
        }
        if (blk->next_phys && (blk->next_phys->flags & BLK_FLAG_FREE)) {
            size_t combined =
                blk->size + sizeof(struct block_hdr) + blk->next_phys->size;
            if (combined >= need) {
                struct block_hdr *nxt = blk->next_phys;
                free_list_remove(nxt);
                blk->size = combined;
                blk->next_phys = nxt->next_phys;
                if (blk->next_phys) {
                    blk->next_phys->prev_phys = blk;
                }
                nxt->magic = 0u;
                blk->req_size = size;
                split_tail(blk, need);
                heap_unlock();
                return ptr;
            }
        }
        heap_unlock();
    }

    void *np = malloc(size);
    if (!np) {
        return NULL;
    }
    size_t copy_n = (old_req < size) ? old_req : size;
    if (copy_n > 0u) {
        memcpy(np, ptr, copy_n);
    }
    free(ptr);
    return np;
}

void *reallocarray(void *ptr, size_t nelem, size_t elsize) {
    if (nelem != 0u && elsize > ((size_t)-1) / nelem) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(ptr, nelem * elsize);
}

void *aligned_alloc(size_t alignment, size_t size) {
    if (alignment == 0u || (alignment & (alignment - 1u)) != 0u) {
        errno = EINVAL;
        return NULL;
    }
    if (alignment <= HEAP_ALIGN) {
        return malloc(size);
    }
    if (alignment > ((size_t)-1) - sizeof(struct block_hdr)) {
        errno = ENOMEM;
        return NULL;
    }
    size_t extra = (alignment - 1u) + sizeof(struct block_hdr);
    if (size > ((size_t)-1) - extra) {
        errno = ENOMEM;
        return NULL;
    }
    void *raw = malloc(size + extra);
    if (!raw) {
        return NULL;
    }
    uintptr_t base = (uintptr_t)raw + sizeof(struct block_hdr);
    uintptr_t aligned =
        (base + (uintptr_t)(alignment - 1u)) & ~(uintptr_t)(alignment - 1u);
    struct block_hdr *hdr = ((struct block_hdr *)aligned) - 1;
    memset(hdr, 0, sizeof(*hdr));
    hdr->magic = BLOCK_MAGIC;
    hdr->flags = BLK_FLAG_ALIGNED;
    hdr->size = size;
    hdr->req_size = size;
    hdr->prev_phys = (struct block_hdr *)raw;
    return (void *)aligned;
}
