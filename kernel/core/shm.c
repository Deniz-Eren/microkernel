/**
 * @file     shm.c
 * @brief    Kernel POSIX shared memory objects (SHM) and MAP_SHARED backing
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <fcntl.h>
#include <kernel/kheap.h>
#include <kernel/pmm.h>
#include <kernel/sched.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/uaccess.h>
#include <kernel/vmm.h>
#include <sys/errno.h>
#include <sys/fd.h>
#include <sys/mman.h>
#include <sys/vm.h>

/* need:: COMP_MEM_0005 */

#define SHM_PAGE_SIZE 4096ULL
#define SHM_MAX_OBJS 32u
#define SHM_NAME_MAX 64u
#define SHM_MAX_PAGES 256u /* 1 MiB per shared memory object */
#define SHM_USER_VA_MIN 0x400000ULL
#define SHM_USER_MMAP_LIMIT 0x50000000ULL

typedef struct {
    uint8_t used;
    uint8_t unlinked;
    char name[SHM_NAME_MAX];
    uint32_t mode;
    uint32_t refs;
    uint64_t size;
    uint64_t *frames;
    size_t npages;
} shm_obj_t;

static spinlock_t g_shm_lock = SPINLOCK_INIT;
static shm_obj_t g_shm[SHM_MAX_OBJS];

static void shm_free_frames(shm_obj_t *obj) {
    if (obj->frames) {
        for (size_t i = 0; i < obj->npages; i++) {
            if (obj->frames[i]) {
                pmm_free_frame(obj->frames[i]);
            }
        }
        kfree(obj->frames);
        obj->frames = NULL;
    }
    obj->npages = 0u;
    obj->size = 0u;
}

static void shm_destroy_if_dead(shm_obj_t *obj) {
    if (obj->used && obj->unlinked && obj->refs == 0u) {
        shm_free_frames(obj);
        memset(obj, 0, sizeof(*obj));
    }
}

void shm_acquire_id(uint32_t id) {
    if (id == 0u || id > SHM_MAX_OBJS) {
        return;
    }
    uint64_t sf = spin_lock_irqsave(&g_shm_lock);
    shm_obj_t *obj = &g_shm[id - 1u];
    if (obj->used) {
        obj->refs++;
    }
    spin_unlock_irqrestore(&g_shm_lock, sf);
}

uint32_t shm_active_count(void) {
    uint64_t sf = spin_lock_irqsave(&g_shm_lock);
    uint32_t n = 0;
    for (uint32_t i = 0; i < SHM_MAX_OBJS; i++) {
        if (g_shm[i].used) {
            n++;
        }
    }
    spin_unlock_irqrestore(&g_shm_lock, sf);
    return n;
}

void shm_release_id(uint32_t id) {
    if (id == 0u || id > SHM_MAX_OBJS) {
        return;
    }
    uint64_t sf = spin_lock_irqsave(&g_shm_lock);
    shm_obj_t *obj = &g_shm[id - 1u];
    if (obj->used && obj->refs > 0u) {
        obj->refs--;
        shm_destroy_if_dead(obj);
    }
    spin_unlock_irqrestore(&g_shm_lock, sf);
}

static long copy_shm_name(uint64_t name_va, char dst[SHM_NAME_MAX]) {
    if (!name_va) {
        return -EFAULT;
    }
    size_t len = 0u;
    for (; len < SHM_NAME_MAX; len++) {
        char ch = 0;
        if (copy_from_user(&ch, name_va + len, 1u) < 0) {
            return -EFAULT;
        }
        dst[len] = ch;
        if (ch == '\0') {
            break;
        }
    }
    if (len == SHM_NAME_MAX) {
        return -ENAMETOOLONG;
    }
    /* POSIX SHM names must start with '/' and contain no other '/'. */
    if (len < 2u || dst[0] != '/') {
        return -EINVAL;
    }
    for (size_t i = 1u; i < len; i++) {
        if (dst[i] == '/') {
            return -EINVAL;
        }
    }
    return 0;
}

static shm_obj_t *shm_find_named(const char *name, uint32_t *out_id) {
    for (uint32_t i = 0u; i < SHM_MAX_OBJS; i++) {
        if (g_shm[i].used && !g_shm[i].unlinked &&
            kstrcmp(g_shm[i].name, name) == 0) {
            if (out_id) {
                *out_id = i + 1u;
            }
            return &g_shm[i];
        }
    }
    return NULL;
}

static long shm_do_open(pcb_t *p, uint64_t name_va, uint64_t oflag,
                        uint64_t mode) {
    char name[SHM_NAME_MAX];
    long nr = copy_shm_name(name_va, name);
    if (nr < 0) {
        return nr;
    }
    uint64_t acc = oflag & (uint64_t)O_ACCMODE;
    if (acc != (uint64_t)O_RDONLY && acc != (uint64_t)O_RDWR) {
        return -EINVAL;
    }
    if (oflag & ~(uint64_t)(O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC)) {
        return -EINVAL;
    }

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    spin_lock(&g_shm_lock);
    uint32_t obj_id = 0u;
    shm_obj_t *obj = shm_find_named(name, &obj_id);
    if (obj) {
        if ((oflag & (uint64_t)(O_CREAT | O_EXCL)) ==
            (uint64_t)(O_CREAT | O_EXCL)) {
            spin_unlock(&g_shm_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EEXIST;
        }
        if (oflag & (uint64_t)O_TRUNC) {
            if (acc != (uint64_t)O_RDWR) {
                spin_unlock(&g_shm_lock);
                spin_unlock_irqrestore(&g_proc_lock, pf);
                return -EACCES;
            }
            shm_free_frames(obj);
        }
    } else {
        if (!(oflag & (uint64_t)O_CREAT)) {
            spin_unlock(&g_shm_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -ENOENT;
        }
        for (uint32_t i = 0u; i < SHM_MAX_OBJS; i++) {
            if (!g_shm[i].used) {
                obj = &g_shm[i];
                obj_id = i + 1u;
                memset(obj, 0, sizeof(*obj));
                obj->used = 1u;
                memcpy(obj->name, name, SHM_NAME_MAX);
                obj->mode = (uint32_t)(mode & 0777u);
                break;
            }
        }
        if (!obj) {
            spin_unlock(&g_shm_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -ENOSPC;
        }
    }

    for (int fd = 0; fd < PROC_MAX_FDS; fd++) {
        if (p->fds[fd].kind == FD_KIND_NONE) {
            p->fds[fd].kind = FD_KIND_SHM;
            p->fds[fd].fd_flags = FD_FLAG_CLOEXEC; /* POSIX mandatory */
            p->fds[fd].status_flags = (uint16_t)acc;
            p->fds[fd].srv_pid = 0u;
            p->fds[fd].srv_chanid = 0u;
            p->fds[fd].srv_handle = obj_id;
            obj->refs++;
            spin_unlock(&g_shm_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return fd;
        }
    }
    if (obj->refs == 0u && (oflag & (uint64_t)O_CREAT)) {
        memset(obj, 0, sizeof(*obj));
    }
    spin_unlock(&g_shm_lock);
    spin_unlock_irqrestore(&g_proc_lock, pf);
    return -EMFILE;
}

static long shm_do_unlink(uint64_t name_va) {
    char name[SHM_NAME_MAX];
    long nr = copy_shm_name(name_va, name);
    if (nr < 0) {
        return nr;
    }
    uint64_t sf = spin_lock_irqsave(&g_shm_lock);
    shm_obj_t *obj = shm_find_named(name, NULL);
    if (!obj) {
        spin_unlock_irqrestore(&g_shm_lock, sf);
        return -ENOENT;
    }
    obj->unlinked = 1u;
    shm_destroy_if_dead(obj);
    spin_unlock_irqrestore(&g_shm_lock, sf);
    return 0;
}

static long shm_do_truncate(pcb_t *p, uint64_t fd, int64_t length) {
    if (fd >= PROC_MAX_FDS || length < 0) {
        return fd >= PROC_MAX_FDS ? -EBADF : -EINVAL;
    }
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    if (p->fds[fd].kind != FD_KIND_SHM) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EBADF;
    }
    if ((p->fds[fd].status_flags & O_ACCMODE) != O_RDWR) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EINVAL;
    }
    uint32_t id = p->fds[fd].srv_handle;
    spin_unlock_irqrestore(&g_proc_lock, pf);

    uint64_t sf = spin_lock_irqsave(&g_shm_lock);
    if (id == 0u || id > SHM_MAX_OBJS || !g_shm[id - 1u].used) {
        spin_unlock_irqrestore(&g_shm_lock, sf);
        return -EBADF;
    }
    shm_obj_t *obj = &g_shm[id - 1u];
    uint64_t ulen = (uint64_t)length;
    size_t want_pages = (size_t)((ulen + SHM_PAGE_SIZE - 1ULL) / SHM_PAGE_SIZE);
    if (want_pages > SHM_MAX_PAGES) {
        spin_unlock_irqrestore(&g_shm_lock, sf);
        return -ENOMEM;
    }

    if (want_pages == 0u) {
        shm_free_frames(obj);
        spin_unlock_irqrestore(&g_shm_lock, sf);
        return 0;
    }

    if (want_pages > obj->npages) {
        uint64_t *nf = (uint64_t *)kzalloc(want_pages * sizeof(uint64_t));
        if (!nf) {
            spin_unlock_irqrestore(&g_shm_lock, sf);
            return -ENOMEM;
        }
        for (size_t i = 0; i < obj->npages; i++) {
            nf[i] = obj->frames[i];
        }
        for (size_t i = obj->npages; i < want_pages; i++) {
            uint64_t fr = pmm_alloc_frame();
            if (!fr) {
                for (size_t j = obj->npages; j < i; j++) {
                    pmm_free_frame(nf[j]);
                }
                kfree(nf);
                spin_unlock_irqrestore(&g_shm_lock, sf);
                return -ENOMEM;
            }
            memset(phys_to_virt(fr), 0, SHM_PAGE_SIZE);
            nf[i] = fr;
        }
        kfree(obj->frames);
        obj->frames = nf;
        obj->npages = want_pages;
    } else if (want_pages < obj->npages) {
        for (size_t i = want_pages; i < obj->npages; i++) {
            if (obj->frames[i]) {
                pmm_free_frame(obj->frames[i]);
                obj->frames[i] = 0ULL;
            }
        }
        obj->npages = want_pages;
    }

    /* Zero any partial tail inside the final page when shrinking. */
    uint64_t tail_off = ulen & (SHM_PAGE_SIZE - 1ULL);
    if (ulen < obj->size && tail_off != 0ULL && obj->npages > 0u) {
        uint8_t *pg = (uint8_t *)phys_to_virt(obj->frames[obj->npages - 1u]);
        memset(pg + tail_off, 0, SHM_PAGE_SIZE - tail_off);
    }
    obj->size = ulen;
    spin_unlock_irqrestore(&g_shm_lock, sf);
    return 0;
}

static long shm_do_stat(pcb_t *p, uint64_t fd, uint64_t stat_va) {
    if (fd >= PROC_MAX_FDS) {
        return -EBADF;
    }
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    if (p->fds[fd].kind != FD_KIND_SHM) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EBADF;
    }
    uint32_t id = p->fds[fd].srv_handle;
    spin_unlock_irqrestore(&g_proc_lock, pf);

    uint64_t sf = spin_lock_irqsave(&g_shm_lock);
    if (id == 0u || id > SHM_MAX_OBJS || !g_shm[id - 1u].used) {
        spin_unlock_irqrestore(&g_shm_lock, sf);
        return -EBADF;
    }
    const shm_obj_t *obj = &g_shm[id - 1u];
    struct __k_shm_stat ks = {
        .size = obj->size,
        .mode = obj->mode,
        .id = id,
    };
    spin_unlock_irqrestore(&g_shm_lock, sf);
    if (copy_to_user(stat_va, &ks, sizeof(ks)) < 0) {
        return -EFAULT;
    }
    return 0;
}

long sys_shm_op(uint64_t op, uint64_t a1, uint64_t a2, uint64_t a3) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    switch (op) {
    case SHM_OP_OPEN:
        return shm_do_open(p, a1, a2, a3);
    case SHM_OP_UNLINK:
        return shm_do_unlink(a1);
    case SHM_OP_TRUNCATE:
        return shm_do_truncate(p, a1, (int64_t)a2);
    case SHM_OP_STAT:
        return shm_do_stat(p, a1, a2);
    default:
        return -EINVAL;
    }
}

long sys_shm_map(uint64_t va_hint, uint64_t len, uint64_t prot, uint64_t flags,
                 uint64_t fd, uint64_t off) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (len == 0ULL || (off & (SHM_PAGE_SIZE - 1ULL)) != 0ULL) {
        return -EINVAL;
    }
    if (prot & ~(uint64_t)(VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXEC)) {
        return -EINVAL;
    }
    if ((prot & VM_PROT_WRITE) && (prot & VM_PROT_EXEC)) {
        return -EACCES; /* W^X policy */
    }
    if (fd >= PROC_MAX_FDS) {
        return -EBADF;
    }
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    if (p->fds[fd].kind != FD_KIND_SHM) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EBADF;
    }
    int is_shared = (flags & (uint64_t)MAP_SHARED) != 0ULL;
    if (is_shared && (prot & VM_PROT_WRITE) &&
        (p->fds[fd].status_flags & O_ACCMODE) == O_RDONLY) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EACCES;
    }
    uint32_t id = p->fds[fd].srv_handle;
    spin_unlock_irqrestore(&g_proc_lock, pf);

    uint64_t sf = spin_lock_irqsave(&g_shm_lock);
    if (id == 0u || id > SHM_MAX_OBJS || !g_shm[id - 1u].used) {
        spin_unlock_irqrestore(&g_shm_lock, sf);
        return -EBADF;
    }
    shm_obj_t *obj = &g_shm[id - 1u];
    uint64_t first_page = off / SHM_PAGE_SIZE;
    uint64_t npages = (len + SHM_PAGE_SIZE - 1ULL) / SHM_PAGE_SIZE;
    if (npages == 0ULL || first_page > obj->npages ||
        npages > obj->npages - first_page) {
        spin_unlock_irqrestore(&g_shm_lock, sf);
        return -EINVAL;
    }

    uint64_t bytes = npages * SHM_PAGE_SIZE;
    uint64_t base = 0ULL;
    if (va_hint != 0ULL) {
        if ((va_hint & 0xFFFULL) != 0ULL || va_hint < SHM_USER_VA_MIN ||
            va_hint > SHM_USER_MMAP_LIMIT ||
            bytes > SHM_USER_MMAP_LIMIT - va_hint) {
            spin_unlock_irqrestore(&g_shm_lock, sf);
            return -EINVAL;
        }
        if (flags & (uint64_t)MAP_FIXED) {
            for (uint64_t i = 0; i < npages; i++) {
                uint64_t old = vmm_unmap_page_to(p->pml4_phys,
                                                 va_hint + i * SHM_PAGE_SIZE);
                if (old) {
                    pmm_free_frame(old);
                }
            }
            base = va_hint;
        }
    }
    if (!base) {
        if (flags & (uint64_t)MAP_FIXED) {
            spin_unlock_irqrestore(&g_shm_lock, sf);
            return -EINVAL;
        }
        base = vmm_alloc_mmap_va(p->pml4_phys, &p->next_mmap_va, npages);
        if (!base) {
            spin_unlock_irqrestore(&g_shm_lock, sf);
            return -ENOMEM;
        }
    }

    uint64_t pte_flags = vmm_prot_to_pte_flags(prot);
    if (is_shared) {
        pte_flags |= VMM_SHARED;
    }
    if (p->mlock_flags & MCL_FUTURE) {
        pte_flags |= VMM_LOCKED;
    }

    for (uint64_t i = 0; i < npages; i++) {
        uint64_t backing = obj->frames[first_page + i];
        uint64_t pa = 0ULL;
        if (is_shared) {
            pmm_frame_inc(backing);
            pa = backing;
        } else {
            pa = pmm_alloc_frame();
            if (!pa) {
                for (uint64_t j = 0; j < i; j++) {
                    uint64_t old = vmm_unmap_page_to(p->pml4_phys,
                                                     base + j * SHM_PAGE_SIZE);
                    if (old) {
                        pmm_free_frame(old);
                    }
                }
                spin_unlock_irqrestore(&g_shm_lock, sf);
                return -ENOMEM;
            }
            memcpy(phys_to_virt(pa), phys_to_virt(backing), SHM_PAGE_SIZE);
        }
        if (vmm_map_page_to(p->pml4_phys, base + i * SHM_PAGE_SIZE, pa,
                            pte_flags) != 0) {
            pmm_free_frame(pa);
            for (uint64_t j = 0; j < i; j++) {
                uint64_t old =
                    vmm_unmap_page_to(p->pml4_phys, base + j * SHM_PAGE_SIZE);
                if (old) {
                    pmm_free_frame(old);
                }
            }
            spin_unlock_irqrestore(&g_shm_lock, sf);
            return -ENOMEM;
        }
    }
    spin_unlock_irqrestore(&g_shm_lock, sf);
    if (flags & (uint64_t)MAP_FIXED) {
        vmm_tlb_shootdown(p->pml4_phys, 0ULL);
    }
    return (long)base;
}
