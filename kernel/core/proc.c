/**
 * @file     proc.c
 * @brief    User process creation, fork, execve, FD table, and bootfs spawning
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <fcntl.h>
#include <kernel/acpi.h>
#include <kernel/arch.h>
#include <kernel/cov.h>
#include <kernel/elf.h>
#include <kernel/ipc.h>
#include <kernel/kheap.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/pmm.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <kernel/trapframe.h>
#include <kernel/uaccess.h>
#include <kernel/ustar.h>
#include <kernel/vmm.h>
#include <sys/bootinfo.h>
#include <sys/errno.h>
#include <sys/fd.h>
#include <sys/intr.h>
#include <sys/proc.h>
#include <sys/vm.h>
#include "channel.h"

/* need:: COMP_BOOT_0002 COMP_PROC_0002 COMP_PROC_0009 COMP_PROC_0010
 * need:: COMP_PROC_0011 */

#define MAIN_STACK_TOP 0x700000000000ULL
#define MAIN_STACK_SIZE (64ULL * 1024ULL)
#define EXEC_MAX_ARGS 32
#define EXEC_MAX_STR 2048
#define EXEC_BLOCK_CAP 8192

/* Locate the ACPI RSDP by scanning the BIOS ROM area (QEMU places it there). */
static uint64_t find_rsdp(void) {
    for (uint64_t a = 0xE0000; a < 0xFFFFF; a += 16) {
        const char *p = (const char *)phys_to_virt(a);
        if (p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == ' ' &&
            p[4] == 'P' && p[5] == 'T' && p[6] == 'R' && p[7] == ' ') {
            return a;
        }
    }
    return 0;
}

static int find_boot_module(const mb_module_t *mods, size_t nmods,
                            uint64_t *phys, uint64_t *size) {
    for (size_t i = 0; i < nmods; i++) {
        if (mods[i].cmdline && kstrstr(mods[i].cmdline, "bootfs")) {
            *phys = mods[i].start;
            *size = mods[i].end - mods[i].start;
            return 0;
        }
    }
    if (nmods) {
        *phys = mods[0].start;
        *size = mods[0].end - mods[0].start;
        return 0;
    }
    return -1;
}

static int find_bootfs_image(uint64_t tar_phys, uint64_t tar_size,
                             const char *name, const void **img_out,
                             uint64_t *sz_out) {
    const char *base = name;
    if (base[0] == '/' && base[1] == 'b' && base[2] == 'o' && base[3] == 'o' &&
        base[4] == 't' && base[5] == '/') {
        base += 6;
    } else if (base[0] == '/' && base[1] == 'b' && base[2] == 'i' &&
               base[3] == 'n' && base[4] == '/') {
        base += 5;
    } else if (base[0] == '/') {
        base += 1;
    }
    ustar_iter_t it;
    ustar_open(&it, tar_phys, tar_size);
    while (ustar_next(&it)) {
        if (kstrcmp(it.name, name) == 0 || kstrcmp(it.name, base) == 0) {
            *img_out = ustar_data(&it);
            *sz_out = it.size;
            return 0;
        }
    }
    return -1;
}

/* Load a bootfs image and, when it requests PT_INTERP, map the dynamic linker
 * from the same bootfs. The entry switches to the interpreter and the
 * auxv reports the main image through AT_ENTRY/AT_PHDR and AT_BASE. */
static int proc_load_image(uint64_t pml4_phys, const void *image,
                           size_t image_size, uint64_t tar_phys,
                           uint64_t tar_size, elf_load_info_t *out) {
    if (elf_load_user(pml4_phys, image, image_size, ELF_DYN_BASE, out) != 0) {
        return -1;
    }
    if (!out->interp[0]) {
        return 0;
    }
    const void *interp_image = NULL;
    uint64_t interp_size = 0;
    if (find_bootfs_image(tar_phys, tar_size, out->interp, &interp_image,
                          &interp_size) != 0) {
        printk("[proc] interpreter '%s' not found in bootfs\n", out->interp);
        return -1;
    }
    elf_load_info_t li;
    if (elf_load_user(pml4_phys, interp_image, interp_size, ELF_INTERP_BASE,
                      &li) != 0) {
        printk("[proc] failed to load interpreter '%s'\n", out->interp);
        return -1;
    }
    out->base = li.bias;
    out->start = li.entry;
    return 0;
}

/* Build the initial user stack with argv/envp/auxv per SysV ABI. */
static uint64_t build_user_stack_argv(uint64_t pml4_phys,
                                      const char *const *argv, int argc,
                                      const char *const *envp, int envc,
                                      const boot_info_t *bi,
                                      const elf_load_info_t *ei) {
    for (uint64_t va = MAIN_STACK_TOP - MAIN_STACK_SIZE; va < MAIN_STACK_TOP;
         va += 4096) {
        uint64_t frame = pmm_alloc_frame();
        if (!frame) {
            return 0;
        }
        memset(phys_to_virt(frame), 0, 4096);
        if (vmm_map_page_to(pml4_phys, va, frame,
                            VMM_USER | VMM_PRESENT | VMM_WRITE | VMM_NX) != 0) {
            pmm_free_frame(frame);
            return 0;
        }
    }

    uint8_t *block = kzalloc(EXEC_BLOCK_CAP);
    if (!block) {
        return 0;
    }

    uint64_t arg_va[EXEC_MAX_ARGS];
    uint64_t env_va[EXEC_MAX_ARGS];
    uint64_t off = EXEC_BLOCK_CAP;

    for (int i = envc - 1; i >= 0; i--) {
        size_t len = strlen(envp[i]) + 1;
        if (len > off) {
            kfree(block);
            return 0;
        }
        off -= len;
        memcpy(block + off, envp[i], len);
        env_va[i] = MAIN_STACK_TOP - (EXEC_BLOCK_CAP - off);
    }

    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        if (len > off) {
            kfree(block);
            return 0;
        }
        off -= len;
        memcpy(block + off, argv[i], len);
        arg_va[i] = MAIN_STACK_TOP - (EXEC_BLOCK_CAP - off);
    }

    off &= ~15ULL;
    uint64_t nwords = (uint64_t)(1 + argc + 1 + envc + 1 + 16);
    uint64_t table_bytes = nwords * sizeof(uint64_t);
    if (table_bytes + 16 > off) {
        kfree(block);
        return 0;
    }
    off = (off - table_bytes) & ~15ULL;

    uint64_t *w = (uint64_t *)(block + off);
    size_t k = 0;
    w[k++] = (uint64_t)argc;
    for (int i = 0; i < argc; i++) {
        w[k++] = arg_va[i];
    }
    w[k++] = 0;
    for (int i = 0; i < envc; i++) {
        w[k++] = env_va[i];
    }
    w[k++] = 0;
    w[k++] = 6; /* AT_PAGESZ */
    w[k++] = 4096;
    w[k++] = AT_MICROKERNEL_BOOTINFO;
    w[k++] = BOOTINFO_VA;
    w[k++] = 3; /* AT_PHDR */
    w[k++] = ei->phdr;
    w[k++] = 4; /* AT_PHENT */
    w[k++] = ei->phent;
    w[k++] = 5; /* AT_PHNUM */
    w[k++] = ei->phnum;
    w[k++] = 7; /* AT_BASE */
    w[k++] = ei->base;
    w[k++] = 9; /* AT_ENTRY */
    w[k++] = ei->entry;
    w[k++] = 0; /* AT_NULL */
    w[k++] = 0;

    uint64_t block_size = EXEC_BLOCK_CAP - off;
    uint64_t block_va = MAIN_STACK_TOP - block_size;

    for (uint64_t done = 0; done < block_size;) {
        uint64_t va = block_va + done;
        uint64_t page = va & ~0xFFFULL;
        uint64_t pgoff = va & 0xFFFULL;
        uint64_t n = 4096 - pgoff;
        if (n > block_size - done) {
            n = block_size - done;
        }
        uint64_t frame = vmm_get_phys_to(pml4_phys, page);
        if (!frame) {
            kfree(block);
            return 0;
        }
        memcpy((uint8_t *)phys_to_virt(frame) + pgoff, block + off + done, n);
        done += n;
    }
    kfree(block);

    /* Boot info page, mapped read-only into the user address space. */
    boot_info_t *kbi = kzalloc(sizeof(boot_info_t));
    if (!kbi) {
        return 0;
    }
    *kbi = *bi;
    kbi->magic = BOOTINFO_MAGIC;
    kbi->cmdline[sizeof(kbi->cmdline) - 1] = '\0';
    for (uint64_t va = BOOTINFO_VA; va < BOOTINFO_VA + 4096; va += 4096) {
        uint64_t frame = pmm_alloc_frame();
        if (!frame) {
            kfree(kbi);
            return 0;
        }
        uint64_t idx = va - BOOTINFO_VA;
        uint64_t n = sizeof(boot_info_t) - idx;
        if (n > 4096) {
            n = 4096;
        }
        memset(phys_to_virt(frame), 0, 4096);
        memcpy(phys_to_virt(frame), (uint8_t *)kbi + idx, n);
        if (vmm_map_page_to(pml4_phys, va, frame,
                            VMM_USER | VMM_PRESENT | VMM_NX) != 0) {
            pmm_free_frame(frame);
            kfree(kbi);
            return 0;
        }
    }
    kfree(kbi);

    return block_va;
}

static boot_info_t g_bootinfo;

static void proc_format_comm_args(char *comm_dst, char *args_dst,
                                  const char *path, const char *const *argv,
                                  int argc) {
    const char *src = (path && path[0]) ? path : "unknown";
    const char *base = src;
    for (const char *q = src; *q; q++) {
        if (*q == '/' && q[1] != '\0') {
            base = q + 1;
        }
    }
    size_t clen = strlen(base);
    if (clen >= PROC_COMM_MAX) {
        clen = PROC_COMM_MAX - 1u;
    }
    memcpy(comm_dst, base, clen);
    comm_dst[clen] = '\0';

    size_t pos = 0u;
    if (argv && argc > 0) {
        for (int i = 0; i < argc && pos + 1u < PROC_ARGS_MAX; i++) {
            const char *a = argv[i];
            if (!a) {
                continue;
            }
            if (i > 0 && pos + 1u < PROC_ARGS_MAX) {
                args_dst[pos++] = ' ';
            }
            for (size_t j = 0; a[j] && pos + 1u < PROC_ARGS_MAX; j++) {
                args_dst[pos++] = a[j];
            }
        }
    }
    if (pos == 0u) {
        size_t alen = strlen(src);
        if (alen >= PROC_ARGS_MAX) {
            alen = PROC_ARGS_MAX - 1u;
        }
        memcpy(args_dst, src, alen);
        pos = alen;
    }
    args_dst[pos] = '\0';
}

/* Load one ustar bootfs image into a fresh process with optional argv. */
static pcb_t *proc_spawn_from_bootfs_argv(pcb_t *parent, uint64_t tar_phys,
                                          uint64_t tar_size, const char *name,
                                          const char *const *argv, int argc,
                                          uint8_t prio, int precreate_chan,
                                          int *err_out) {
    const void *image = NULL;
    uint64_t image_size = 0;
    if (find_bootfs_image(tar_phys, tar_size, name, &image, &image_size) != 0) {
        printk("[proc] '%s' not found in bootfs (skipped)\n", name);
        if (err_out) {
            *err_out = -ENOENT;
        }
        return NULL;
    }

    pcb_t *p = proc_create(parent);
    if (!p) {
        if (err_out) {
            *err_out = -ENOMEM;
        }
        return NULL;
    }
    elf_load_info_t ei;
    if (proc_load_image(p->pml4_phys, image, image_size, tar_phys, tar_size,
                        &ei) != 0) {
        printk("[proc] failed to load '%s' ELF\n", name);
        p->state = PROC_DEAD;
        vmm_destroy_user_pml4(p->pml4_phys);
        p->pml4_phys = 0;
        if (err_out) {
            *err_out = -ENOEXEC;
        }
        return NULL;
    }
    if (ei.interp[0]) {
        printk("[proc] dynamic '%s' interp=%s base=%llx entry=%llx\n", name,
               ei.interp, (unsigned long long)ei.base,
               (unsigned long long)ei.entry);
    }
    p->brk = ei.brk;

    const char *argv_one[1] = {name};
    const char *const *use_argv = (argv && argc > 0) ? argv : argv_one;
    int use_argc = (argv && argc > 0) ? argc : 1;
    proc_format_comm_args(p->comm, p->args, name, use_argv, use_argc);
    uint64_t user_rsp = build_user_stack_argv(p->pml4_phys, use_argv, use_argc,
                                              NULL, 0, &g_bootinfo, &ei);
    if (!user_rsp) {
        p->state = PROC_DEAD;
        vmm_destroy_user_pml4(p->pml4_phys);
        p->pml4_phys = 0;
        if (err_out) {
            *err_out = -ENOMEM;
        }
        return NULL;
    }
    if (precreate_chan) {
        channel_precreate_sysproc(p->pid);
    }
    tcb_t *main_t = thread_create(p, ei.start, user_rsp, prio);
    /* Pre-point FS.base at the zeroed bottom of the main stack so %fs:0 reads
     * NULL safely before __libc_start_main installs the TLS block. */
    main_t->fs_base = MAIN_STACK_TOP - MAIN_STACK_SIZE;
    printk("[proc] spawned '%s' pid=%u ppid=%u entry=%llx rsp=%llx prio=%u\n",
           name, p->pid, p->parent_pid, (unsigned long long)ei.start,
           (unsigned long long)user_rsp, prio);
    return p;
}

static pcb_t *proc_spawn_from_bootfs(pcb_t *parent, uint64_t tar_phys,
                                     uint64_t tar_size, const char *name,
                                     uint8_t prio, int precreate_chan,
                                     int *err_out) {
    return proc_spawn_from_bootfs_argv(parent, tar_phys, tar_size, name, NULL,
                                       0, prio, precreate_chan, err_out);
}

static void map_bootfs_readonly(pcb_t *p, uint64_t tar_phys,
                                uint64_t tar_size) {
    if (!p || !tar_phys || !tar_size) {
        return;
    }
    uint64_t base_phys = tar_phys & ~0xFFFULL;
    uint64_t end_phys = (tar_phys + tar_size + 0xFFFULL) & ~0xFFFULL;
    uint64_t va = BOOTFS_USER_VA;
    for (uint64_t pa = base_phys; pa < end_phys; pa += 4096ULL, va += 4096ULL) {
        pmm_frame_inc(pa);
        if (vmm_map_page_to(p->pml4_phys, va, pa,
                            VMM_USER | VMM_PRESENT | VMM_NX) != 0) {
            pmm_free_frame(pa);
            return;
        }
    }
}

static pcb_t *spawn_image(uint64_t tar_phys, uint64_t tar_size,
                          const char *name, uint8_t prio, int precreate_chan) {
    return proc_spawn_from_bootfs(NULL, tar_phys, tar_size, name, prio,
                                  precreate_chan, NULL);
}

static pcb_t *spawn_image_argv(uint64_t tar_phys, uint64_t tar_size,
                               const char *name, const char *const *argv,
                               int argc, uint8_t prio, int precreate_chan) {
    return proc_spawn_from_bootfs_argv(NULL, tar_phys, tar_size, name, argv,
                                       argc, prio, precreate_chan, NULL);
}

static void fmt_hex_u32(char *buf, uint32_t v) {
    static const char hex[] = "0123456789abcdef";
    char rev[8];
    int n = 0;
    if (v == 0u) {
        rev[n++] = '0';
    }
    while (v != 0u) {
        rev[n++] = hex[v & 0xFu];
        v >>= 4;
    }
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < n; i++) {
        buf[2 + i] = rev[n - 1 - i];
    }
    buf[2 + n] = '\0';
}

static void fmt_dec_u32(char *buf, uint32_t v) {
    char rev[12];
    int n = 0;
    if (v == 0u) {
        rev[n++] = '0';
    }
    while (v != 0u) {
        rev[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    for (int i = 0; i < n; i++) {
        buf[i] = rev[n - 1 - i];
    }
    buf[n] = '\0';
}

void proc_close_fds(pcb_t *p, int cloexec_only) {
    if (!p) {
        return;
    }
    struct __k_fd_entry closed[PROC_MAX_FDS];
    int nclosed = 0;

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    for (int i = 0; i < PROC_MAX_FDS; i++) {
        if (p->fds[i].kind == FD_KIND_NONE) {
            continue;
        }
        if (cloexec_only && !(p->fds[i].fd_flags & FD_FLAG_CLOEXEC)) {
            continue;
        }
        closed[nclosed++] = p->fds[i];
        memset(&p->fds[i], 0, sizeof(p->fds[i]));
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);

    for (int i = 0; i < nclosed; i++) {
        if ((closed[i].kind == FD_KIND_SERVER ||
             closed[i].kind == FD_KIND_VFS || closed[i].kind == FD_KIND_SOCK) &&
            closed[i].srv_pid && closed[i].srv_chanid) {
            ipc_send_pulse_kernel(closed[i].srv_pid, closed[i].srv_chanid,
                                  PULSE_CODE_FD_CLOSE, closed[i].srv_handle);
        } else if (closed[i].kind == FD_KIND_SHM) {
            shm_release_id(closed[i].srv_handle);
        }
    }
}

long sys_proc_getpid(void) {
    pcb_t *p = proc_current();
    return p ? (long)p->pid : -EPERM;
}

long sys_proc_getppid(void) {
    pcb_t *p = proc_current();
    return p ? (long)p->parent_pid : -EPERM;
}

static long copy_user_cstr(uint64_t va, char *dst, size_t max_len);

long sys_fd_op(uint64_t op, uint64_t a1, uint64_t a2, uint64_t a3,
               uint64_t a4) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }

    switch (op) {
    case FD_OP_ALLOC: {
        uint64_t min_fd = a1;
        uint64_t entry_va = a2;
        if (min_fd >= PROC_MAX_FDS) {
            return -EINVAL;
        }
        struct __k_fd_entry e = {0};
        if (copy_from_user(&e, entry_va, sizeof(e)) < 0) {
            return -EFAULT;
        }
        if (e.kind == FD_KIND_NONE) {
            return -EINVAL;
        }
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        int fd_limit = PROC_MAX_FDS;
        if (p->rlimits[4].rlim_cur < (uint64_t)fd_limit) {
            fd_limit = (int)p->rlimits[4].rlim_cur;
        }
        for (int i = (int)min_fd; i < fd_limit; i++) {
            if (p->fds[i].kind == FD_KIND_NONE) {
                p->fds[i] = e;
                spin_unlock_irqrestore(&g_proc_lock, pf);
                return i;
            }
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EMFILE;
    }
    case FD_OP_GET: {
        if (a1 >= PROC_MAX_FDS) {
            return -EBADF;
        }
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->fds[a1].kind == FD_KIND_NONE) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EBADF;
        }
        struct __k_fd_entry e = p->fds[a1];
        spin_unlock_irqrestore(&g_proc_lock, pf);
        if (a2 != 0 && copy_to_user(a2, &e, sizeof(e)) < 0) {
            return -EFAULT;
        }
        return 0;
    }
    case FD_OP_CLOSE: {
        if (a1 >= PROC_MAX_FDS) {
            return -EBADF;
        }
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->fds[a1].kind == FD_KIND_NONE) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EBADF;
        }
        struct __k_fd_entry old = p->fds[a1];
        memset(&p->fds[a1], 0, sizeof(p->fds[a1]));
        spin_unlock_irqrestore(&g_proc_lock, pf);
        if (old.kind == FD_KIND_SHM) {
            shm_release_id(old.srv_handle);
        }
        if (a2 != 0 && copy_to_user(a2, &old, sizeof(old)) < 0) {
            return -EFAULT;
        }
        return 0;
    }
    case FD_OP_DUP: {
        if (a1 >= PROC_MAX_FDS) {
            return -EBADF;
        }
        if (a2 >= PROC_MAX_FDS) {
            return -EINVAL;
        }
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->fds[a1].kind == FD_KIND_NONE) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EBADF;
        }
        int fd_limit = PROC_MAX_FDS;
        if (p->rlimits[4].rlim_cur < (uint64_t)fd_limit) {
            fd_limit = (int)p->rlimits[4].rlim_cur;
        }
        for (int i = (int)a2; i < fd_limit; i++) {
            if (p->fds[i].kind == FD_KIND_NONE) {
                p->fds[i] = p->fds[a1];
                p->fds[i].fd_flags = (uint8_t)(a3 & FD_FLAG_CLOEXEC);
                uint8_t kind = p->fds[i].kind;
                uint32_t handle = p->fds[i].srv_handle;
                spin_unlock_irqrestore(&g_proc_lock, pf);
                if (kind == FD_KIND_SHM) {
                    shm_acquire_id(handle);
                }
                return i;
            }
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EMFILE;
    }
    case FD_OP_DUP3: {
        if (a1 >= PROC_MAX_FDS || a2 >= PROC_MAX_FDS) {
            return -EBADF;
        }
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->fds[a1].kind == FD_KIND_NONE || a2 >= p->rlimits[4].rlim_cur) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EBADF;
        }
        struct __k_fd_entry replaced = p->fds[a2];
        p->fds[a2] = p->fds[a1];
        p->fds[a2].fd_flags = (uint8_t)(a3 & FD_FLAG_CLOEXEC);
        uint8_t kind = p->fds[a2].kind;
        uint32_t handle = p->fds[a2].srv_handle;
        spin_unlock_irqrestore(&g_proc_lock, pf);
        if (kind == FD_KIND_SHM) {
            shm_acquire_id(handle);
        }
        if (replaced.kind == FD_KIND_SHM) {
            shm_release_id(replaced.srv_handle);
        }
        if (a4 != 0 && copy_to_user(a4, &replaced, sizeof(replaced)) < 0) {
            return -EFAULT;
        }
        return (long)a2;
    }
    case FD_OP_SETFD: {
        if (a1 >= PROC_MAX_FDS) {
            return -EBADF;
        }
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->fds[a1].kind == FD_KIND_NONE) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EBADF;
        }
        p->fds[a1].fd_flags = (uint8_t)(a2 & FD_FLAG_CLOEXEC);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return 0;
    }
    case FD_OP_SETFL: {
        if (a1 >= PROC_MAX_FDS) {
            return -EBADF;
        }
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->fds[a1].kind == FD_KIND_NONE) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EBADF;
        }
        uint16_t acc = (uint16_t)(p->fds[a1].status_flags & O_ACCMODE);
        uint16_t mut = (uint16_t)(a2 & (uint64_t)(O_NONBLOCK | O_APPEND));
        p->fds[a1].status_flags = (uint16_t)(acc | mut);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return 0;
    }
    case FD_OP_LIST: {
        uint64_t max_n = a2;
        if (max_n > PROC_MAX_FDS) {
            max_n = PROC_MAX_FDS;
        }
        struct __k_fd_entry snap[PROC_MAX_FDS];
        if (max_n > 0) {
            uint64_t pf = spin_lock_irqsave(&g_proc_lock);
            memcpy(snap, p->fds, (size_t)max_n * sizeof(p->fds[0]));
            spin_unlock_irqrestore(&g_proc_lock, pf);
            if (copy_to_user(a1, snap, (size_t)max_n * sizeof(p->fds[0])) < 0) {
                return -EFAULT;
            }
        }
        return (long)max_n;
    }
    case FD_OP_GETCWD: {
        if (a1 == 0u || a2 == 0u) {
            return -EINVAL;
        }
        char kcwd[PROC_CWD_MAX];
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        size_t len = strlen(p->cwd);
        memcpy(kcwd, p->cwd, len + 1u);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        if (len + 1u > a2) {
            return -ERANGE;
        }
        if (copy_to_user(a1, kcwd, len + 1u) < 0) {
            return -EFAULT;
        }
        return (long)len;
    }
    case FD_OP_SETCWD: {
        if (a1 == 0u) {
            return -EFAULT;
        }
        char kpath[PROC_CWD_MAX];
        long len = copy_user_cstr(a1, kpath, sizeof(kpath));
        if (len < 0) {
            return len;
        }
        if (len == 0 || kpath[0] != '/') {
            return -EINVAL;
        }
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        memcpy(p->cwd, kpath, (size_t)len + 1u);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return 0;
    }
    case FD_OP_UMASK: {
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        uint32_t old = p->umask;
        if (a2 == 0u) {
            p->umask = (uint32_t)(a1 & 0777u);
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return (long)old;
    }
    case FD_OP_CONSOLE_TX: {
        if (!(p->caps & CAP_SYS_INTR)) {
            return -EPERM;
        }
        if (a1 == 0u || a2 == 0u) {
            return 0;
        }
        size_t len = (size_t)a2;
        if (len > 1024u) {
            len = 1024u;
        }
        char kbuf[1024];
        if (copy_from_user(kbuf, a1, len) < 0) {
            return -EFAULT;
        }
        console_write_raw_locked(kbuf, len);
        return (long)len;
    }
    case FD_OP_PROC_LIST: {
        if (a1 == 0u && a2 == 0u) {
            size_t total = 0u;
            uint64_t pf = spin_lock_irqsave(&g_proc_lock);
            for (pcb_t *c = proc_list_head(); c; c = c->next) {
                if (c->state != PROC_DEAD) {
                    total++;
                }
            }
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return (long)total;
        }
        if (a1 == 0u) {
            return -EFAULT;
        }
        size_t cap = (size_t)a2;
        if (cap > PROC_LIST_MAX) {
            cap = PROC_LIST_MAX;
        }
        if (cap == 0u) {
            return 0;
        }
        struct __k_proc_info *snap =
            kzalloc(cap * sizeof(struct __k_proc_info));
        if (!snap) {
            return -ENOMEM;
        }
        size_t count = 0u;
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        for (pcb_t *c = proc_list_head(); c && count < cap; c = c->next) {
            if (c->state == PROC_DEAD) {
                continue;
            }
            struct __k_proc_info *dst = &snap[count++];
            dst->pid = c->pid;
            dst->ppid = c->parent_pid;
            dst->pgid = c->pgid;
            dst->sid = c->sid;
            dst->uid = c->uid;
            dst->gid = c->gid;
            dst->nice = c->nice;
            dst->state =
                (c->state == PROC_ZOMBIE) ? PROC_INFO_ZOMBIE : PROC_INFO_ALIVE;
            dst->nthreads = c->nthreads;
            dst->prio = c->threads ? (uint32_t)c->threads->base_priority : 0u;
            dst->tty_kind = FD_KIND_NONE;
            for (int f = 0; f < 3; f++) {
                if (c->fds[f].kind == FD_KIND_TTY ||
                    c->fds[f].kind == FD_KIND_CONSOLE) {
                    dst->tty_kind = c->fds[f].kind;
                    break;
                }
            }
            dst->utime_ticks =
                __atomic_load_n(&c->utime_ticks, __ATOMIC_RELAXED);
            dst->stime_ticks =
                __atomic_load_n(&c->stime_ticks, __ATOMIC_RELAXED);
            memcpy(dst->comm, c->comm, sizeof(dst->comm));
            dst->comm[PROC_COMM_MAX - 1u] = '\0';
            memcpy(dst->args, c->args, sizeof(dst->args));
            dst->args[PROC_ARGS_MAX - 1u] = '\0';
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);

        long res = (long)count;
        if (count > 0u &&
            copy_to_user(a1, snap, count * sizeof(struct __k_proc_info)) < 0) {
            res = -EFAULT;
        }
        kfree(snap);
        return res;
    }
    default:
        return -EINVAL;
    }
}

long sys_proc_cred(uint64_t op, uint64_t a1, uint64_t a2, uint64_t a3) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }

    switch (op) {
    case CRED_OP_GETUID: {
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        uint32_t v = p->uid;
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return (long)v;
    }
    case CRED_OP_GETEUID: {
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        uint32_t v = p->euid;
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return (long)v;
    }
    case CRED_OP_SETUID: {
        uint32_t nu = (uint32_t)a1;
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->euid == 0u) {
            p->uid = nu;
            p->euid = nu;
            p->suid = nu;
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return 0;
        }
        if (nu == p->uid || nu == p->suid) {
            p->euid = nu;
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return 0;
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EPERM;
    }
    case CRED_OP_SETEUID: {
        uint32_t nu = (uint32_t)a1;
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->euid == 0u || nu == p->uid || nu == p->suid) {
            p->euid = nu;
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return 0;
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EPERM;
    }
    case CRED_OP_GETGID: {
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        uint32_t v = p->gid;
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return (long)v;
    }
    case CRED_OP_GETEGID: {
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        uint32_t v = p->egid;
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return (long)v;
    }
    case CRED_OP_SETGID: {
        uint32_t ng = (uint32_t)a1;
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->euid == 0u) {
            p->gid = ng;
            p->egid = ng;
            p->sgid = ng;
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return 0;
        }
        if (ng == p->gid || ng == p->sgid) {
            p->egid = ng;
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return 0;
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EPERM;
    }
    case CRED_OP_SETEGID: {
        uint32_t ng = (uint32_t)a1;
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->euid == 0u || ng == p->gid || ng == p->sgid) {
            p->egid = ng;
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return 0;
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EPERM;
    }
    case CRED_OP_GETGROUPS: {
        int64_t sz = (int64_t)a1;
        if (sz < 0) {
            return -EINVAL;
        }
        uint32_t kgrps[PROC_NGROUPS_MAX];
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        uint32_t n = p->ngroups;
        if (n > PROC_NGROUPS_MAX) {
            n = PROC_NGROUPS_MAX;
        }
        memcpy(kgrps, p->groups, (size_t)n * sizeof(uint32_t));
        spin_unlock_irqrestore(&g_proc_lock, pf);
        if (sz == 0) {
            return (long)n;
        }
        if ((uint64_t)sz < (uint64_t)n) {
            return -EINVAL;
        }
        if (a2 == 0u) {
            return -EFAULT;
        }
        if (n > 0u &&
            copy_to_user(a2, kgrps, (size_t)n * sizeof(uint32_t)) < 0) {
            return -EFAULT;
        }
        return (long)n;
    }
    case CRED_OP_SETGROUPS: {
        if (a1 > PROC_NGROUPS_MAX) {
            return -EINVAL;
        }
        uint32_t kgrps[PROC_NGROUPS_MAX];
        memset(kgrps, 0, sizeof(kgrps));
        if (a1 > 0u) {
            if (a2 == 0u) {
                return -EFAULT;
            }
            if (copy_from_user(kgrps, a2, (size_t)a1 * sizeof(uint32_t)) < 0) {
                return -EFAULT;
            }
        }
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        if (p->euid != 0u) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EPERM;
        }
        p->ngroups = (uint32_t)a1;
        memcpy(p->groups, kgrps, sizeof(p->groups));
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return 0;
    }
    case CRED_OP_GETNICE: {
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        pcb_t *t = (a1 == 0u) ? p : proc_find_locked((uint32_t)a1);
        if (!t) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -ESRCH;
        }
        int32_t nv = t->nice;
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return (long)(nv + 20);
    }
    case CRED_OP_SETNICE: {
        int64_t arg = (int64_t)a2;
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        pcb_t *t = (a1 == 0u) ? p : proc_find_locked((uint32_t)a1);
        if (!t) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -ESRCH;
        }
        if (p->euid != 0u && t->uid != p->uid && t->uid != p->euid) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EPERM;
        }
        int64_t next = (a3 != 0u) ? ((int64_t)t->nice + arg) : arg;
        if (next < -20) {
            next = -20;
        } else if (next > 19) {
            next = 19;
        }
        if (next < (int64_t)t->nice && p->euid != 0u) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EPERM;
        }
        t->nice = (int32_t)next;
        uint8_t new_prio = (uint8_t)(100 - t->nice);
        spin_lock(&g_sched_lock);
        for (tcb_t *th = t->threads; th; th = th->proc_next) {
            if (!th->is_idle && th->state != THREAD_ZOMBIE) {
                th->base_priority = new_prio;
                if (th->effective_priority < new_prio) {
                    if (th->state == THREAD_READY) {
                        sched_remove_ready_locked(th);
                        th->effective_priority = new_prio;
                        sched_ready_locked(th);
                    } else {
                        th->effective_priority = new_prio;
                    }
                }
            }
        }
        spin_unlock(&g_sched_lock);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return (long)(next + 20);
    }
    default:
        return -EINVAL;
    }
}

long sys_proc_rlimit(uint64_t resource, uint64_t new_va, uint64_t old_va) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (resource >= PROC_RLIMIT_COUNT) {
        return -EINVAL;
    }
    struct __k_rlimit nr = {0, 0};
    if (new_va != 0u) {
        if (copy_from_user(&nr, new_va, sizeof(nr)) < 0) {
            return -EFAULT;
        }
        if (nr.rlim_cur > nr.rlim_max) {
            return -EINVAL;
        }
    }

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    struct __k_rlimit old_lim = p->rlimits[resource];
    if (new_va != 0u) {
        if (nr.rlim_max > old_lim.rlim_max && p->euid != 0u) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EPERM;
        }
        p->rlimits[resource] = nr;
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);

    if (old_va != 0u && copy_to_user(old_va, &old_lim, sizeof(old_lim)) < 0) {
        return -EFAULT;
    }
    return 0;
}

long sys_proc_fork(const trapframe_t *tf) {
    pcb_t *parent = proc_current();
    tcb_t *cur = sched_current();
    if (!parent || !cur || !tf) {
        return -EPERM;
    }

    pcb_t *child = proc_create(parent);
    if (!child) {
        return -ENOMEM;
    }

    if (vmm_clone_user_cow(parent->pml4_phys, child->pml4_phys) < 0) {
        child->state = PROC_DEAD;
        vmm_destroy_user_pml4(child->pml4_phys);
        child->pml4_phys = 0;
        return -ENOMEM;
    }

    child->brk = parent->brk;
    child->pgid = parent->pgid;
    child->sid = parent->sid;
    child->has_execed = 0;
    child->next_mmap_va = parent->next_mmap_va;
    child->mlock_flags = 0u; /* POSIX: memory locks not inherited on fork */
    child->next_stack_va = parent->next_stack_va;
    child->caps = parent->caps;
    memcpy(child->comm, parent->comm, sizeof(child->comm));
    memcpy(child->args, parent->args, sizeof(child->args));
    memcpy(child->sig_handlers, parent->sig_handlers,
           sizeof(child->sig_handlers));
    memcpy(child->sig_masks, parent->sig_masks, sizeof(child->sig_masks));
    memcpy(child->sig_flags, parent->sig_flags, sizeof(child->sig_flags));
    child->sig_restorer = parent->sig_restorer;
#ifdef MICROKERNEL_COVERAGE
    child->cov = parent->cov;
    child->cov_registered = parent->cov_registered;
    child->cov_dumped = 0;
#endif
    memcpy(child->fds, parent->fds, sizeof(child->fds));
    for (int i = 0; i < PROC_MAX_FDS; i++) {
        if (child->fds[i].kind == FD_KIND_SHM) {
            shm_acquire_id(child->fds[i].srv_handle);
        }
    }

    tcb_t *ct = thread_create_embryo(child, cur->base_priority);
    ct->blocked_signals = cur->blocked_signals;
    ct->pending_signals = 0;
    ct->iopl = cur->iopl;
    ct->fs_base = cur->fs_base;
    thread_arm_fork(ct, tf);

    return (long)child->pid;
}

static long copy_user_cstr(uint64_t va, char *dst, size_t max_len) {
    size_t len = 0;
    for (; len < max_len; len++) {
        char ch = 0;
        if (copy_from_user(&ch, va + len, 1) < 0) {
            return -EFAULT;
        }
        dst[len] = ch;
        if (ch == '\0') {
            return (long)len;
        }
    }
    return -ENAMETOOLONG;
}

static long copy_user_str_array(uint64_t arr_va, char *pool, size_t pool_cap,
                                const char **ptrs, int *count_out) {
    *count_out = 0;
    if (arr_va == 0) {
        return 0;
    }
    size_t used = 0;
    for (int i = 0; i < EXEC_MAX_ARGS; i++) {
        uint64_t str_va = 0;
        if (copy_from_user(&str_va, arr_va + (uint64_t)i * sizeof(uint64_t),
                           sizeof(str_va)) < 0) {
            return -EFAULT;
        }
        if (str_va == 0) {
            *count_out = i;
            return 0;
        }
        size_t rem = pool_cap - used;
        if (rem < 2) {
            return -E2BIG;
        }
        long len = copy_user_cstr(str_va, pool + used, rem);
        if (len == -ENAMETOOLONG) {
            return -E2BIG;
        }
        if (len < 0) {
            return len;
        }
        ptrs[i] = pool + used;
        used += (size_t)len + 1;
    }
    /* Check whether an extra non-NULL pointer exists past EXEC_MAX_ARGS. */
    uint64_t extra = 0;
    if (copy_from_user(&extra,
                       arr_va + (uint64_t)EXEC_MAX_ARGS * sizeof(uint64_t),
                       sizeof(extra)) < 0) {
        return -EFAULT;
    }
    if (extra != 0) {
        return -E2BIG;
    }
    *count_out = EXEC_MAX_ARGS;
    return 0;
}

long sys_proc_execve(trapframe_t *tf, uint64_t path_va, uint64_t argv_va,
                     uint64_t envp_va) {
    pcb_t *p = proc_current();
    tcb_t *cur = sched_current();
    if (!p || !cur || !tf) {
        return -EPERM;
    }

    char path[64];
    long plen = copy_user_cstr(path_va, path, sizeof(path));
    if (plen < 0) {
        return plen;
    }
    if (plen == 0) {
        return -ENOENT;
    }

    const void *image = NULL;
    uint64_t image_size = 0;
    if (find_bootfs_image(g_bootinfo.boot_tar_phys, g_bootinfo.boot_tar_size,
                          path, &image, &image_size) != 0) {
        return -ENOENT;
    }

    char *arg_pool = kzalloc(EXEC_MAX_STR);
    char *env_pool = kzalloc(EXEC_MAX_STR);
    if (!arg_pool || !env_pool) {
        if (arg_pool) {
            kfree(arg_pool);
        }
        if (env_pool) {
            kfree(env_pool);
        }
        return -ENOMEM;
    }

    const char *argv_ptrs[EXEC_MAX_ARGS];
    const char *envp_ptrs[EXEC_MAX_ARGS];
    int argc = 0;
    int envc = 0;

    long rc =
        copy_user_str_array(argv_va, arg_pool, EXEC_MAX_STR, argv_ptrs, &argc);
    if (rc == 0) {
        rc = copy_user_str_array(envp_va, env_pool, EXEC_MAX_STR, envp_ptrs,
                                 &envc);
    }
    if (rc < 0) {
        kfree(arg_pool);
        kfree(env_pool);
        return rc;
    }
    if (argc == 0) {
        argv_ptrs[0] = path;
        argc = 1;
    }

    uint64_t new_pml4 = vmm_create_user_pml4();
    if (!new_pml4) {
        kfree(arg_pool);
        kfree(env_pool);
        return -ENOMEM;
    }

    elf_load_info_t ei;
    if (proc_load_image(new_pml4, image, image_size, g_bootinfo.boot_tar_phys,
                        g_bootinfo.boot_tar_size, &ei) != 0) {
        vmm_destroy_user_pml4(new_pml4);
        kfree(arg_pool);
        kfree(env_pool);
        return -ENOEXEC;
    }

    char new_comm[PROC_COMM_MAX];
    char new_args[PROC_ARGS_MAX];
    proc_format_comm_args(new_comm, new_args, path, argv_ptrs, argc);

    uint64_t user_rsp = build_user_stack_argv(
        new_pml4, argv_ptrs, argc, envp_ptrs, envc, &g_bootinfo, &ei);
    kfree(arg_pool);
    kfree(env_pool);
    if (!user_rsp) {
        vmm_destroy_user_pml4(new_pml4);
        return -ENOMEM;
    }

    /* Point of no return: commit the new process image. */
    uint64_t comm_pf = spin_lock_irqsave(&g_proc_lock);
    memcpy(p->comm, new_comm, sizeof(p->comm));
    memcpy(p->args, new_args, sizeof(p->args));
    spin_unlock_irqrestore(&g_proc_lock, comm_pf);
    cov_dump_proc(p);
#ifdef MICROKERNEL_COVERAGE
    p->cov_registered = 0;
    p->cov_dumped = 0;
#endif
    proc_kill_other_threads(p, cur);
    for (tcb_t *x = p->threads; x; x = x->proc_next) {
        x->user_stack_base = 0;
    }
    proc_reap_zombie_threads(p);
    time_cleanup_proc(p->pid);
    proc_close_fds(p, 1);
    p->mlock_flags = 0u;

    /* Reset caught signal handlers to SIG_DFL; keep SIG_IGN and masks. */
    for (int s = 1; s < 32; s++) {
        if (p->sig_handlers[s] != 0ULL && p->sig_handlers[s] != 1ULL) {
            p->sig_handlers[s] = 0ULL;
            p->sig_masks[s] = 0;
            p->sig_flags[s] = 0;
        }
    }

    uint64_t old_pml4 = p->pml4_phys;
    p->pml4_phys = new_pml4;
    p->brk = ei.brk;
    p->has_execed = 1;
    p->next_mmap_va = 0;
    p->next_stack_va = 0;
    vmm_set_cr3(new_pml4);
    vmm_destroy_user_pml4(old_pml4);

    cur->fs_base = MAIN_STACK_TOP - MAIN_STACK_SIZE;
    arch_set_tls_base(cur->fs_base);
    sched_reset_current_fpu();

    memset(tf, 0, sizeof(*tf));
    arch_tf_init_user(tf, ei.start, 0, user_rsp, cur->iopl);
    return 0;
}

long sys_proc_create(uint64_t name_va, uint64_t prio, uint64_t flags) {
    pcb_t *parent = proc_current();
    tcb_t *cur = sched_current();
    if (!parent || !cur) {
        return -EPERM;
    }
    if (flags != 0 || prio > 255) {
        return -EINVAL;
    }
    char name[64];
    long len = copy_user_cstr(name_va, name, sizeof(name));
    if (len < 0) {
        return (len == -ENAMETOOLONG) ? -EINVAL : len;
    }
    if (len == 0) {
        return -EINVAL;
    }
    uint8_t child_prio = prio ? (uint8_t)prio : cur->base_priority;
    int err = -ENOMEM;
    pcb_t *child = proc_spawn_from_bootfs(parent, g_bootinfo.boot_tar_phys,
                                          g_bootinfo.boot_tar_size, name,
                                          child_prio, 0, &err);
    return child ? (long)child->pid : (long)err;
}

long sys_proc_wait(int64_t pid, uint64_t status_va, uint64_t options) {
    pcb_t *parent = proc_current();
    tcb_t *cur = sched_current();
    if (!parent || !cur) {
        return -EPERM;
    }
    if (options &
        ~(uint64_t)(PROC_WAIT_NOHANG | PROC_WAIT_STOP | PROC_WAIT_CONT |
                    PROC_WAIT_EXT | PROC_WAIT_NOWAIT | PROC_WAIT_EXITED)) {
        return -EINVAL;
    }
    /* Preserve legacy ProcWait callers that pass 0 or NOHANG without EXITED. */
    uint64_t want_exited =
        (options & PROC_WAIT_EXT) ? (options & PROC_WAIT_EXITED) : 1ULL;

    for (;;) {
        int has_matching = 0;
        pcb_t *zombie = NULL;
        pcb_t *ev_proc = NULL;
        int ev_kind = 0; /* 1 = stopped, 2 = continued */
        int ev_status = 0;
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        for (pcb_t *c = proc_list_head(); c; c = c->next) {
            if (c->parent_pid != parent->pid || c->state == PROC_DEAD) {
                continue;
            }
            if (pid > 0 && c->pid != (uint32_t)pid) {
                continue;
            }
            if (pid == 0 && c->pgid != parent->pgid) {
                continue;
            }
            if (pid < -1 && c->pgid != (uint32_t)(-pid)) {
                continue;
            }
            has_matching = 1;
            if (want_exited && c->state == PROC_ZOMBIE) {
                zombie = c;
                if (!(options & PROC_WAIT_NOWAIT)) {
                    zombie->state = PROC_DEAD;
                }
                break;
            }
            if (!ev_kind && (options & PROC_WAIT_STOP) && c->stopped &&
                c->stop_sig) {
                ev_kind = 1;
                ev_proc = c;
                ev_status = (int)(((uint32_t)c->stop_sig << 8) | 0x7Fu);
            }
            if (!ev_kind && (options & PROC_WAIT_CONT) && c->cont_pending) {
                ev_kind = 2;
                ev_proc = c;
                ev_status = 0xFFFF;
            }
        }
        if (!has_matching) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -ECHILD;
        }
        if (zombie) {
            int wstatus = zombie->exit_signal
                              ? (zombie->exit_signal & 0x7F)
                              : ((zombie->exit_code & 0xFF) << 8);
            uint32_t reaped_pid = zombie->pid;
            uint32_t reaped_uid = zombie->uid;
            spin_unlock_irqrestore(&g_proc_lock, pf);
            if (status_va != 0) {
                int copy_err = 0;
                if (options & PROC_WAIT_EXT) {
                    struct __k_wait_ext ext = {.status = wstatus,
                                               .pid = reaped_pid,
                                               .uid = reaped_uid,
                                               .pad = 0u};
                    copy_err = copy_to_user(status_va, &ext, sizeof(ext)) < 0;
                } else {
                    copy_err =
                        copy_to_user(status_va, &wstatus, sizeof(wstatus)) < 0;
                }
                if (copy_err) {
                    if (!(options & PROC_WAIT_NOWAIT)) {
                        pf = spin_lock_irqsave(&g_proc_lock);
                        zombie->state = PROC_ZOMBIE;
                        spin_unlock_irqrestore(&g_proc_lock, pf);
                    }
                    return -EFAULT;
                }
            }
            if (!(options & PROC_WAIT_NOWAIT)) {
                __atomic_fetch_add(&parent->cutime_ticks,
                                   zombie->utime_ticks + zombie->cutime_ticks,
                                   __ATOMIC_RELAXED);
                __atomic_fetch_add(&parent->cstime_ticks,
                                   zombie->stime_ticks + zombie->cstime_ticks,
                                   __ATOMIC_RELAXED);
                /* Ensure zombie threads have exited their CPU before
                 * destroying the zombie's PML4. */
                for (tcb_t *zt = zombie->threads; zt; zt = zt->proc_next) {
                    while (__atomic_load_n(&zt->on_cpu, __ATOMIC_ACQUIRE) !=
                           0u) {
                        arch_cpu_relax();
                    }
                }
                vmm_destroy_user_pml4(zombie->pml4_phys);
                zombie->pml4_phys = 0;
                proc_reap_zombie(zombie);
            }
            return (long)reaped_pid;
        }
        if (ev_proc) {
            uint32_t ev_pid = ev_proc->pid;
            uint32_t ev_uid = ev_proc->uid;
            if (!(options & PROC_WAIT_NOWAIT)) {
                if (ev_kind == 1) {
                    ev_proc->stop_sig = 0u;
                } else {
                    ev_proc->cont_pending = 0u;
                }
            }
            spin_unlock_irqrestore(&g_proc_lock, pf);
            if (status_va != 0) {
                if (options & PROC_WAIT_EXT) {
                    struct __k_wait_ext ext = {.status = ev_status,
                                               .pid = ev_pid,
                                               .uid = ev_uid,
                                               .pad = 0u};
                    if (copy_to_user(status_va, &ext, sizeof(ext)) < 0) {
                        return -EFAULT;
                    }
                } else if (copy_to_user(status_va, &ev_status,
                                        sizeof(ev_status)) < 0) {
                    return -EFAULT;
                }
            }
            return (long)ev_pid;
        }
        if (options & PROC_WAIT_NOHANG) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return 0;
        }
        spin_lock(&g_sched_lock);
        if (__atomic_load_n(&cur->pending_signals, __ATOMIC_ACQUIRE) &
            ~cur->blocked_signals) {
            spin_unlock(&g_sched_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EINTR;
        }
        cur->state = THREAD_WAIT_BLOCKED;
        spin_unlock(&g_proc_lock);
        schedule_with_lock(pf);
        if (__atomic_load_n(&cur->pending_signals, __ATOMIC_ACQUIRE) &
            ~cur->blocked_signals) {
            return -EINTR;
        }
    }
}

#define USER_STACK_BYTES 0x10000ULL /* 64 KiB per spawned thread */
#define USER_STACK_BASE 0x50000000ULL
#define USER_STACK_LIMIT 0x80000000ULL

/* Allocate a fresh zeroed 64 KiB user stack in p's address space, reusing any
 * unmapped slots in [USER_STACK_BASE, USER_STACK_LIMIT); returns top VA. */
uint64_t proc_user_stack_alloc(pcb_t *p) {
    uint64_t base = 0;
    for (uint64_t va = USER_STACK_BASE;
         va + USER_STACK_BYTES <= USER_STACK_LIMIT; va += USER_STACK_BYTES) {
        int free_slot = 1;
        for (uint64_t off = 0; off < USER_STACK_BYTES; off += 4096ULL) {
            if (vmm_get_pte_to(p->pml4_phys, va + off) & VMM_PRESENT) {
                free_slot = 0;
                break;
            }
        }
        if (free_slot) {
            base = va;
            break;
        }
    }
    if (!base) {
        return 0;
    }
    if (base + USER_STACK_BYTES > p->next_stack_va) {
        p->next_stack_va = base + USER_STACK_BYTES;
    }

    for (uint64_t va = base; va < base + USER_STACK_BYTES; va += 4096) {
        uint64_t frame = pmm_alloc_frame();
        if (!frame) {
            for (uint64_t u = base; u < va; u += 4096) {
                uint64_t phys = vmm_unmap_page_to(p->pml4_phys, u);
                if (phys) {
                    pmm_free_frame(phys);
                }
            }
            return 0;
        }
        memset(phys_to_virt(frame), 0, 4096);
        if (vmm_map_page_to(p->pml4_phys, va, frame,
                            VMM_USER | VMM_PRESENT | VMM_WRITE | VMM_NX) != 0) {
            pmm_free_frame(frame);
            for (uint64_t u = base; u < va; u += 4096) {
                uint64_t phys = vmm_unmap_page_to(p->pml4_phys, u);
                if (phys) {
                    pmm_free_frame(phys);
                }
            }
            return 0;
        }
    }
    return base + USER_STACK_BYTES;
}

long sys_sys_stats(uint64_t ubuf) {
    pcb_t *cur = proc_current();
    if (!cur) {
        return -EPERM;
    }
    if (!ubuf) {
        return -EFAULT;
    }
    proc_reap_zombie_threads(cur);

    struct __k_sys_stats st = {0};
    st.pmm_total_bytes = pmm_total_bytes();
    st.pmm_free_bytes = pmm_free_bytes();
    st.active_shm_objs = shm_active_count();
    st.online_cpus = smp_online_cpus();

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    for (pcb_t *p = proc_list_head(); p; p = p->next) {
        if (p->state == PROC_ALIVE) {
            st.live_procs++;
            for (tcb_t *t = p->threads; t; t = t->proc_next) {
                if (t->state != THREAD_ZOMBIE) {
                    st.live_threads++;
                }
            }
        }
    }
    for (int i = 0; i < PROC_MAX_FDS; i++) {
        if (cur->fds[i].kind != FD_KIND_NONE) {
            st.open_fds++;
        }
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);
    return copy_to_user(ubuf, &st, sizeof(st)) < 0 ? -EFAULT : 0;
}

long sys_proc_pgid(uint64_t op, int64_t pid, int64_t pgid) {
    pcb_t *cur = proc_current();
    if (!cur) {
        return -EPERM;
    }
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    long rc = -EINVAL;
    switch (op) {
    case PGID_OP_GETPGID: {
        if (pid < 0) {
            rc = -EINVAL;
            break;
        }
        pcb_t *target = (pid == 0) ? cur : proc_find_locked((uint32_t)pid);
        if (!target || target->state == PROC_DEAD) {
            rc = -ESRCH;
            break;
        }
        rc = (long)target->pgid;
        break;
    }
    case PGID_OP_GETSID: {
        if (pid < 0) {
            rc = -EINVAL;
            break;
        }
        pcb_t *target = (pid == 0) ? cur : proc_find_locked((uint32_t)pid);
        if (!target || target->state == PROC_DEAD) {
            rc = -ESRCH;
            break;
        }
        rc = (long)target->sid;
        break;
    }
    case PGID_OP_SETSID: {
        int conflict = 0;
        for (pcb_t *p = proc_list_head(); p; p = p->next) {
            if (p->state == PROC_ALIVE && p->pgid == cur->pid) {
                conflict = 1;
                break;
            }
        }
        if (conflict) {
            rc = -EPERM;
            break;
        }
        cur->sid = cur->pid;
        cur->pgid = cur->pid;
        rc = (long)cur->sid;
        break;
    }
    case PGID_OP_SETPGID: {
        if (pid < 0 || pgid < 0) {
            rc = -EINVAL;
            break;
        }
        pcb_t *target = (pid == 0) ? cur : proc_find_locked((uint32_t)pid);
        if (!target || target->state == PROC_DEAD) {
            rc = -ESRCH;
            break;
        }
        if (target != cur && target->parent_pid != cur->pid) {
            rc = -ESRCH;
            break;
        }
        if (target != cur && target->has_execed) {
            rc = -EACCES;
            break;
        }
        if (target->pid == target->sid) {
            rc = -EPERM;
            break;
        }
        uint32_t new_pgid = (pgid == 0) ? target->pid : (uint32_t)pgid;
        if (new_pgid != target->pid) {
            int exists_in_sid = 0;
            for (pcb_t *p = proc_list_head(); p; p = p->next) {
                if (p->state != PROC_DEAD && p->sid == cur->sid &&
                    p->pgid == new_pgid) {
                    exists_in_sid = 1;
                    break;
                }
            }
            if (!exists_in_sid) {
                rc = -EPERM;
                break;
            }
        }
        target->pgid = new_pgid;
        rc = 0;
        break;
    }
    default:
        rc = -EINVAL;
        break;
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);
    return rc;
}

void proc_spawn_init(void) {
    size_t nmods;
    const mb_module_t *mods = mb_modules(&nmods);
    uint64_t tar_phys = 0, tar_size = 0;
    if (find_boot_module(mods, nmods, &tar_phys, &tar_size) != 0) {
        panic("proc: no bootfs module loaded by GRUB");
    }

    const acpi_info_t *ai = acpi_info();
    g_bootinfo.magic = BOOTINFO_MAGIC;
    g_bootinfo.boot_tar_phys = tar_phys;
    g_bootinfo.boot_tar_size = tar_size;
    g_bootinfo.rsdp_phys = ai->rsdp_phys ? ai->rsdp_phys : find_rsdp();
    g_bootinfo.mem_total_bytes = pmm_total_bytes();
    g_bootinfo.ioapic_phys = ai->ioapic_phys;
    g_bootinfo.serial_port = ai->serial_port;
    g_bootinfo.serial_gsi = ai->serial_gsi;
    g_bootinfo.rtc_gsi = ai->rtc_gsi;
    g_bootinfo.cpu_features = arch_cpu_features();
    g_bootinfo.cpu_count = smp_online_cpus();
    const char *cmd = mb_cmdline();
    memcpy(g_bootinfo.cmdline, cmd,
           strlen(cmd) < sizeof(g_bootinfo.cmdline) - 1
               ? strlen(cmd) + 1
               : sizeof(g_bootinfo.cmdline) - 1);

    /* PID 1: system process manager / pathname manager. Higher boot priority
     * guarantees sysproc creates chanid 1 before other Ring 3 servers run. */
    pcb_t *p1 = spawn_image(tar_phys, tar_size, "sysproc", 110, 1);
    if (!p1) {
        panic("proc: failed to spawn sysproc");
    }
    p1->caps |= CAP_SYS_INTR;

    /* Ring 3 serial TTY server (/dev/tty) parameterized from ACPI discovery.
     * Priority 105 (below sysproc 110, above apps 100) lets ttyserv finish its
     * 1-byte UART loopback IRQ check before any user process writes to COM1. */
    char tty_port_arg[16];
    char tty_gsi_arg[16];
    fmt_hex_u32(tty_port_arg, ai->serial_port);
    fmt_dec_u32(tty_gsi_arg, ai->serial_gsi);
    const char *tty_argv[] = {"ttyserv", "-p", tty_port_arg, "-i", tty_gsi_arg};
    pcb_t *tty =
        spawn_image_argv(tar_phys, tar_size, "ttyserv", tty_argv, 5, 105, 0);
    if (tty) {
        tty->caps |= CAP_SYS_INTR;
    }

    /* Ring 3 pipe, socket, VFS, and reference IPC servers. */
    spawn_image(tar_phys, tar_size, "pipeserv", 100, 0);
    spawn_image(tar_phys, tar_size, "sockserv", 100, 0);
    pcb_t *vfs = spawn_image(tar_phys, tar_size, "vfsserv", 100, 0);
    if (vfs) {
        map_bootfs_readonly(vfs, tar_phys, tar_size);
    }
    spawn_image(tar_phys, tar_size, "ipcserv", 100, 0);

    /* Ring 3 CMOS RTC driver parameterized from ACPI MADT GSI discovery. */
    char rtc_gsi_arg[16];
    fmt_dec_u32(rtc_gsi_arg, ai->rtc_gsi);
    const char *rtc_argv[] = {"rtcdrv", "-p", "0x70", "-i", rtc_gsi_arg};
    pcb_t *drv =
        spawn_image_argv(tar_phys, tar_size, "rtcdrv", rtc_argv, 5, 100, 0);
    if (drv) {
        drv->caps |= CAP_SYS_INTR;
    }

    /* Ring 3 PCI virtio-blk storage driver (/dev/blk) with CAP_SYS_INTR. */
    pcb_t *blk = spawn_image(tar_phys, tar_size, "blkdrv", 102, 0);
    if (blk) {
        blk->caps |= CAP_SYS_INTR;
    }

    /* Ring 3 PCI virtio-net driver and TCP/IP server (/dev/net). */
    pcb_t *net = spawn_image(tar_phys, tar_size, "netserv", 101, 0);
    if (net) {
        net->caps |= CAP_SYS_INTR;
    }

    if (kstrstr(cmd, "init=sh") != NULL) {
        printk("[OK] interactive boot mode (init=sh): spawning /boot/sh\n");
        spawn_image(tar_phys, tar_size, "sh", 95, 0);
        sched_activate();
        return;
    }

    spawn_image(tar_phys, tar_size, "ipccli", 100, 0);
    /* Milestone 4 demo: threads + signal delivery (worker thread, caught
     * SIGUSR1, then terminates itself with an unhandled SIGUSR2). */
    spawn_image(tar_phys, tar_size, "sigtest", 100, 0);
    /* Milestone 5 demo: ProcCreate, VMMap/VMUnmap, VMCloneCow, and waitpid. */
    spawn_image(tar_phys, tar_size, "proctest", 100, 0);
    /* Milestone 6 demo: unprivileged client testing /dev/rtc and EPERM. */
    spawn_image(tar_phys, tar_size, "drvtest", 100, 0);
    /* POSIX.1-2024 CLOCK_MONOTONIC/CLOCK_REALTIME and interval timers test. */
    spawn_image(tar_phys, tar_size, "timetest", 100, 0);
    /* POSIX.1-2024 pthreads, semaphores, and FS.base TLS test. */
    spawn_image(tar_phys, tar_size, "pthreadtest", 100, 0);
    /* POSIX.1-2024 fork, execve, pipe2, dup3, and ppoll test. */
    spawn_image(tar_phys, tar_size, "forkpipetest", 100, 0);
    /* POSIX.1-2024 asynchronous I/O (<aio.h>) test. */
    spawn_image(tar_phys, tar_size, "aiotest", 100, 0);
    /* POSIX.1-2024 thread cancellation points and stdio stream locking test. */
    spawn_image(tar_phys, tar_size, "canceltest", 100, 0);
    /* POSIX.1-2024 Base functions and C17 headers test. */
    spawn_image(tar_phys, tar_size, "libctest", 100, 0);
    /* POSIX.1-2024 mmap-backed thread-safe heap allocator test. */
    spawn_image(tar_phys, tar_size, "heaptest", 100, 0);
    /* POSIX.1-2024 Ring 3 VFS (bootfs/tmpfs, stat, dirent) test. */
    spawn_image(tar_phys, tar_size, "vfstest", 100, 0);
    /* POSIX.1-2024 option-gated SHM, ML, and MLR memory extensions test. */
    spawn_image(tar_phys, tar_size, "shmtest", 100, 0);
    /* POSIX.1-2024 serial TTY, termios, and multi-server ppoll test. */
    spawn_image(tar_phys, tar_size, "ttytest", 100, 0);
    /* POSIX.1-2024 buffered stdio, env, execvp, and core C17 headers test. */
    spawn_image(tar_phys, tar_size, "stdiotest", 100, 0);
    /* POSIX.1-2024 AF_UNIX local sockets and accept4 test. */
    spawn_image(tar_phys, tar_size, "socktest", 100, 0);
    /* Command shell (sh) and core utilities test (POSIX.1-2024 XCU target). */
    spawn_image(tar_phys, tar_size, "shtest", 100, 0);
    /* Additional POSIX.1-2024 Base headers and standard C utilities test. */
    spawn_image(tar_phys, tar_size, "utilstest", 100, 0);
    /* TTY job control, ISIG Ctrl-C, process groups, and core utilities test. */
    spawn_image(tar_phys, tar_size, "jobtest", 90, 0);
    /* x86-64-v3 AVX2/FMA/BMI and preemptive XSAVE/XRSTOR isolation test. */
    spawn_image(tar_phys, tar_size, "simdtest", 100, 0);
    /* POSIX.1-2024 <fenv.h> and <math.h> (FMA/trig/exp/log/pow) test. */
    spawn_image(tar_phys, tar_size, "mathtest", 100, 0);
    /* Ring 3 CAP_SYS_INTR MMIO/DMA/PCI EPERM and /dev/blk persistence test. */
    spawn_image(tar_phys, tar_size, "blktest", 98, 0);
    /* Ring 3 PCI virtio-net driver and TCP/IP/loopback network test. */
    spawn_image(tar_phys, tar_size, "nettest", 96, 0);
    /* Ring 3 /mnt ext2 writable on-disk filesystem persistence test. */
    spawn_image(tar_phys, tar_size, "ext2test", 95, 0);
    /* Ring 3 sysroot zlib (libz.a) compression, gzip, and checksum test. */
    spawn_image(tar_phys, tar_size, "zlibtest", 95, 0);
    /* Ring 3 sysroot OpenSSL (libcrypto/libssl) crypto and TLS 1.3 test. */
    spawn_image(tar_phys, tar_size, "openssltest", 95, 0);
    /* Ring 3 sysroot IANA tzcode (zic, zdump, tzselect) and tzdata test. */
    spawn_image(tar_phys, tar_size, "tztest", 95, 0);
    /* /boot/ld.so ldd trace mode and /usr/sbin/ldconfig symlink test. */
    spawn_image(tar_phys, tar_size, "lddtest", 95, 0);
    /* Multi-core SMP concurrency (parallelism, futex, PMM/VMM/COW, IPC) test.
     */
    spawn_image(tar_phys, tar_size, "smptest", 94, 0);
    /* Ring 3 dynamic linker (PT_INTERP /boot/ld.so, SysV auxv, DT_NEEDED over
     * /dev/vfs with LD_LIBRARY_PATH search) test. */
    spawn_image(tar_phys, tar_size, "dlsoctest", 100, 0);
    /* Shared libc.so dynamic executable (PT_INTERP, dlopen, pthreads, TLS
     * errno, signals, fork/execve) test. */
    spawn_image(tar_phys, tar_size, "dynlibctest", 100, 0);
    /* Cross-subsystem select/pselect and leak-freedom stress test (low prio).
     */
    spawn_image(tar_phys, tar_size, "stresstest", 85, 0);

    sched_activate();
}
