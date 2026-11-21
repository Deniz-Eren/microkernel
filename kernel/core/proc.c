/**
 * @file     proc.c
 * @brief    User process creation, fork, execve, FD table, and bootfs spawning
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <arch/x86_64/gdt.h>
#include <arch/x86_64/msr.h>
#include <fcntl.h>
#include <kernel/elf.h>
#include <kernel/ipc.h>
#include <kernel/kheap.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/pmm.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
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
#include "channel.h"

/* need:: COMP_BOOT_0002 COMP_PROC_0002 */

#define MSR_FS_BASE 0xC0000100U
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

/* Build the initial user stack with argv/envp/auxv per SysV ABI. */
static uint64_t build_user_stack_argv(uint64_t pml4_phys,
                                      const char *const *argv, int argc,
                                      const char *const *envp, int envc,
                                      const boot_info_t *bi) {
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
    uint64_t nwords = (uint64_t)(1 + argc + 1 + envc + 1 + 6);
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
    kbi->magic = BOOTINFO_MAGIC;
    kbi->boot_tar_phys = bi->boot_tar_phys;
    kbi->boot_tar_size = bi->boot_tar_size;
    kbi->rsdp_phys = bi->rsdp_phys;
    kbi->mem_total_bytes = bi->mem_total_bytes;
    memcpy(kbi->cmdline, bi->cmdline, sizeof(kbi->cmdline) - 1);
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

/* Load one ustar bootfs image into a fresh process and make it runnable. */
static pcb_t *proc_spawn_from_bootfs(pcb_t *parent, uint64_t tar_phys,
                                     uint64_t tar_size, const char *name,
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
    uint64_t entry = 0, brk = 0;
    if (elf_load_user(p->pml4_phys, image, image_size, &entry, &brk) != 0) {
        printk("[proc] failed to load '%s' ELF\n", name);
        p->state = PROC_DEAD;
        vmm_destroy_user_pml4(p->pml4_phys);
        p->pml4_phys = 0;
        if (err_out) {
            *err_out = -ENOEXEC;
        }
        return NULL;
    }
    p->brk = brk;

    const char *argv_one[1] = {name};
    uint64_t user_rsp =
        build_user_stack_argv(p->pml4_phys, argv_one, 1, NULL, 0, &g_bootinfo);
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
        channel_create(p->pid);
    }
    tcb_t *main_t = thread_create(p, entry, user_rsp, prio);
    /* Pre-point FS.base at the zeroed bottom of the main stack so %fs:0 reads
     * NULL safely before __libc_start_main installs the TLS block. */
    main_t->fs_base = MAIN_STACK_TOP - MAIN_STACK_SIZE;
    printk("[proc] spawned '%s' pid=%u ppid=%u entry=%llx rsp=%llx prio=%u\n",
           name, p->pid, p->parent_pid, (unsigned long long)entry,
           (unsigned long long)user_rsp, prio);
    return p;
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

void proc_close_fds(pcb_t *p, int cloexec_only) {
    if (!p) {
        return;
    }
    for (int i = 0; i < PROC_MAX_FDS; i++) {
        if (p->fds[i].kind == FD_KIND_NONE) {
            continue;
        }
        if (cloexec_only && !(p->fds[i].fd_flags & FD_FLAG_CLOEXEC)) {
            continue;
        }
        if ((p->fds[i].kind == FD_KIND_SERVER ||
             p->fds[i].kind == FD_KIND_VFS || p->fds[i].kind == FD_KIND_SOCK) &&
            p->fds[i].srv_pid && p->fds[i].srv_chanid) {
            ipc_send_pulse_kernel(p->fds[i].srv_pid, p->fds[i].srv_chanid,
                                  PULSE_CODE_FD_CLOSE, p->fds[i].srv_handle);
        } else if (p->fds[i].kind == FD_KIND_SHM) {
            shm_release_id(p->fds[i].srv_handle);
        }
        memset(&p->fds[i], 0, sizeof(p->fds[i]));
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
        for (int i = (int)min_fd; i < PROC_MAX_FDS; i++) {
            if (p->fds[i].kind == FD_KIND_NONE) {
                p->fds[i] = e;
                return i;
            }
        }
        return -EMFILE;
    }
    case FD_OP_GET: {
        if (a1 >= PROC_MAX_FDS || p->fds[a1].kind == FD_KIND_NONE) {
            return -EBADF;
        }
        if (a2 != 0 && copy_to_user(a2, &p->fds[a1], sizeof(p->fds[a1])) < 0) {
            return -EFAULT;
        }
        return 0;
    }
    case FD_OP_CLOSE: {
        if (a1 >= PROC_MAX_FDS || p->fds[a1].kind == FD_KIND_NONE) {
            return -EBADF;
        }
        struct __k_fd_entry old = p->fds[a1];
        memset(&p->fds[a1], 0, sizeof(p->fds[a1]));
        if (old.kind == FD_KIND_SHM) {
            shm_release_id(old.srv_handle);
        }
        if (a2 != 0 && copy_to_user(a2, &old, sizeof(old)) < 0) {
            return -EFAULT;
        }
        return 0;
    }
    case FD_OP_DUP: {
        if (a1 >= PROC_MAX_FDS || p->fds[a1].kind == FD_KIND_NONE) {
            return -EBADF;
        }
        if (a2 >= PROC_MAX_FDS) {
            return -EINVAL;
        }
        for (int i = (int)a2; i < PROC_MAX_FDS; i++) {
            if (p->fds[i].kind == FD_KIND_NONE) {
                p->fds[i] = p->fds[a1];
                p->fds[i].fd_flags = (uint8_t)(a3 & FD_FLAG_CLOEXEC);
                if (p->fds[i].kind == FD_KIND_SHM) {
                    shm_acquire_id(p->fds[i].srv_handle);
                }
                return i;
            }
        }
        return -EMFILE;
    }
    case FD_OP_DUP3: {
        if (a1 >= PROC_MAX_FDS || p->fds[a1].kind == FD_KIND_NONE) {
            return -EBADF;
        }
        if (a2 >= PROC_MAX_FDS) {
            return -EBADF;
        }
        struct __k_fd_entry replaced = p->fds[a2];
        p->fds[a2] = p->fds[a1];
        p->fds[a2].fd_flags = (uint8_t)(a3 & FD_FLAG_CLOEXEC);
        if (p->fds[a2].kind == FD_KIND_SHM) {
            shm_acquire_id(p->fds[a2].srv_handle);
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
        if (a1 >= PROC_MAX_FDS || p->fds[a1].kind == FD_KIND_NONE) {
            return -EBADF;
        }
        p->fds[a1].fd_flags = (uint8_t)(a2 & FD_FLAG_CLOEXEC);
        return 0;
    }
    case FD_OP_SETFL: {
        if (a1 >= PROC_MAX_FDS || p->fds[a1].kind == FD_KIND_NONE) {
            return -EBADF;
        }
        uint16_t acc = (uint16_t)(p->fds[a1].status_flags & O_ACCMODE);
        uint16_t mut = (uint16_t)(a2 & (uint64_t)(O_NONBLOCK | O_APPEND));
        p->fds[a1].status_flags = (uint16_t)(acc | mut);
        return 0;
    }
    case FD_OP_LIST: {
        uint64_t max_n = a2;
        if (max_n > PROC_MAX_FDS) {
            max_n = PROC_MAX_FDS;
        }
        if (max_n > 0 &&
            copy_to_user(a1, p->fds, (size_t)max_n * sizeof(p->fds[0])) < 0) {
            return -EFAULT;
        }
        return (long)max_n;
    }
    default:
        return -EINVAL;
    }
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
    child->next_mmap_va = parent->next_mmap_va;
    child->mlock_flags = 0u; /* POSIX: memory locks not inherited on fork */
    child->next_stack_va = parent->next_stack_va;
    child->caps = parent->caps;
    memcpy(child->sig_handlers, parent->sig_handlers,
           sizeof(child->sig_handlers));
    memcpy(child->sig_masks, parent->sig_masks, sizeof(child->sig_masks));
    memcpy(child->sig_flags, parent->sig_flags, sizeof(child->sig_flags));
    child->sig_restorer = parent->sig_restorer;
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

    uint64_t entry = 0, brk = 0;
    if (elf_load_user(new_pml4, image, image_size, &entry, &brk) != 0) {
        vmm_destroy_user_pml4(new_pml4);
        kfree(arg_pool);
        kfree(env_pool);
        return -ENOEXEC;
    }

    uint64_t user_rsp = build_user_stack_argv(new_pml4, argv_ptrs, argc,
                                              envp_ptrs, envc, &g_bootinfo);
    kfree(arg_pool);
    kfree(env_pool);
    if (!user_rsp) {
        vmm_destroy_user_pml4(new_pml4);
        return -ENOMEM;
    }

    /* Point of no return: commit the new process image. */
    proc_kill_other_threads(p, cur);
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
    p->brk = brk;
    p->next_mmap_va = 0;
    p->next_stack_va = 0;
    vmm_set_cr3(new_pml4);
    vmm_destroy_user_pml4(old_pml4);

    cur->fs_base = MAIN_STACK_TOP - MAIN_STACK_SIZE;
    wrmsr(MSR_FS_BASE, cur->fs_base);

    memset(tf, 0, sizeof(*tf));
    tf->rip = entry;
    tf->rsp = user_rsp;
    tf->cs = GDT_UCODE | 3;
    tf->ss = GDT_UDATA | 3;
    tf->rflags = 0x202ULL | (((uint64_t)cur->iopl & 3ULL) << 12);
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
    if (options & ~(uint64_t)PROC_WAIT_NOHANG) {
        return -EINVAL;
    }
    if (pid == 0 || pid < -1) {
        return -ENOSYS;
    }

    for (;;) {
        int has_matching = 0;
        pcb_t *zombie = NULL;
        for (pcb_t *c = proc_list_head(); c; c = c->next) {
            if (c->parent_pid != parent->pid || c->state == PROC_DEAD) {
                continue;
            }
            if (pid > 0 && c->pid != (uint32_t)pid) {
                continue;
            }
            has_matching = 1;
            if (c->state == PROC_ZOMBIE) {
                zombie = c;
                break;
            }
        }
        if (!has_matching) {
            return -ECHILD;
        }
        if (zombie) {
            int wstatus = zombie->exit_signal
                              ? (zombie->exit_signal & 0x7F)
                              : ((zombie->exit_code & 0xFF) << 8);
            if (status_va != 0 &&
                copy_to_user(status_va, &wstatus, sizeof(wstatus)) < 0) {
                return -EFAULT;
            }
            uint32_t reaped_pid = zombie->pid;
            zombie->state = PROC_DEAD;
            vmm_destroy_user_pml4(zombie->pml4_phys);
            zombie->pml4_phys = 0;
            return (long)reaped_pid;
        }
        if (options & PROC_WAIT_NOHANG) {
            return 0;
        }
        sched_block_current(THREAD_WAIT_BLOCKED);
        if (cur->pending_signals & ~cur->blocked_signals) {
            return -EINTR;
        }
    }
}

#define USER_STACK_BYTES 0x10000ULL /* 64 KiB per spawned thread */
#define USER_STACK_BASE 0x50000000ULL
#define USER_STACK_LIMIT 0x80000000ULL

/* Allocate a fresh zeroed 64 KiB user stack in p's address space (bump
 * allocator over the 0x50000000..0x80000000 region); returns the top VA. */
uint64_t proc_user_stack_alloc(pcb_t *p) {
    if (!p->next_stack_va) {
        p->next_stack_va = USER_STACK_BASE;
    }
    uint64_t base = p->next_stack_va;
    if (base + USER_STACK_BYTES > USER_STACK_LIMIT) {
        return 0;
    }
    p->next_stack_va = base + USER_STACK_BYTES;

    for (uint64_t va = base; va < base + USER_STACK_BYTES; va += 4096) {
        uint64_t frame = pmm_alloc_frame();
        if (!frame) {
            return 0;
        }
        memset(phys_to_virt(frame), 0, 4096);
        if (vmm_map_page_to(p->pml4_phys, va, frame,
                            VMM_USER | VMM_PRESENT | VMM_WRITE | VMM_NX) != 0) {
            return 0;
        }
    }
    return base + USER_STACK_BYTES;
}

void proc_spawn_init(void) {
    size_t nmods;
    const mb_module_t *mods = mb_modules(&nmods);
    uint64_t tar_phys = 0, tar_size = 0;
    if (find_boot_module(mods, nmods, &tar_phys, &tar_size) != 0) {
        panic("proc: no bootfs module loaded by GRUB");
    }

    g_bootinfo.magic = BOOTINFO_MAGIC;
    g_bootinfo.boot_tar_phys = tar_phys;
    g_bootinfo.boot_tar_size = tar_size;
    g_bootinfo.rsdp_phys = find_rsdp();
    g_bootinfo.mem_total_bytes = pmm_total_bytes();
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
    /* Ring 3 serial TTY server (/dev/tty) with COM1 IRQ 4 and IOPL=3. Priority
     * 105 (below sysproc 110, above tests 100) lets ttyserv complete its 1-byte
     * UART loopback IRQ 4 check before any test writes to COM1. */
    pcb_t *tty = spawn_image(tar_phys, tar_size, "ttyserv", 105, 0);
    if (tty) {
        tty->caps |= CAP_SYS_INTR;
    }
    /* Ring 3 POSIX pipe and poll server (/dev/pipe). */
    spawn_image(tar_phys, tar_size, "pipeserv", 100, 0);
    /* Ring 3 POSIX AF_UNIX local sockets server (/dev/sock). */
    spawn_image(tar_phys, tar_size, "sockserv", 100, 0);
    /* Ring 3 VFS server (/dev/vfs) with read-only bootfs mapping. */
    pcb_t *vfs = spawn_image(tar_phys, tar_size, "vfsserv", 100, 0);
    if (vfs) {
        map_bootfs_readonly(vfs, tar_phys, tar_size);
    }
    spawn_image(tar_phys, tar_size, "ipcserv", 100, 0);
    spawn_image(tar_phys, tar_size, "ipccli", 100, 0);
    /* Milestone 4 demo: threads + signal delivery (worker thread, caught
     * SIGUSR1, then terminates itself with an unhandled SIGUSR2). */
    spawn_image(tar_phys, tar_size, "sigtest", 100, 0);
    /* Milestone 5 demo: ProcCreate, VMMap/VMUnmap, VMCloneCow, and waitpid. */
    spawn_image(tar_phys, tar_size, "proctest", 100, 0);
    /* Milestone 6 demo: privileged Ring 3 RTC driver + unprivileged client. */
    pcb_t *drv = spawn_image(tar_phys, tar_size, "rtcdrv", 100, 0);
    if (drv) {
        drv->caps |= CAP_SYS_INTR;
    }
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
    /* POSIX.1-2024 XCU shell (sh) and core utilities test. */
    spawn_image(tar_phys, tar_size, "shtest", 100, 0);

    sched_activate();
}
