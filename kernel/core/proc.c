// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#include <kernel/sched.h>
#include <kernel/elf.h>
#include <kernel/multiboot.h>
#include <kernel/printk.h>
#include <kernel/panic.h>
#include <kernel/vmm.h>
#include <kernel/kheap.h>
#include <sys/bootinfo.h>
#include <kernel/ustar.h>
#include <kernel/string.h>
#include <kernel/pmm.h>
#include "channel.h"

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

static int find_boot_module(const mb_module_t *mods, size_t nmods, uint64_t *phys,
                            uint64_t *size) {
    for (size_t i = 0; i < nmods; i++) {
        if (mods[i].cmdline && kstrstr(mods[i].cmdline, "bootfs")) {
            *phys = mods[i].start;
            *size = mods[i].end - mods[i].start;
            return 0;
        }
    }
    /* Fall back to the first module. */
    if (nmods) {
        *phys = mods[0].start;
        *size = mods[0].end - mods[0].start;
        return 0;
    }
    return -1;
}

/* Build the initial user stack: argv/envp/auxv per SysV ABI. */
static uint64_t build_user_stack(pcb_t *p, const char *progname,
                                 const boot_info_t *bi) {
    const uint64_t stack_top = 0x700000000000ULL;
    const uint64_t stack_size = 64 * 1024;

    /* Allocate, zero and map the stack pages (all writes go via the HHDM: the
     * user address space is not active while the kernel PML4 is loaded). */
    for (uint64_t va = stack_top - stack_size; va < stack_top; va += 4096) {
        uint64_t frame = pmm_alloc_frame();
        if (!frame) {
            panic("proc: out of frames for user stack");
        }
        memset(phys_to_virt(frame), 0, 4096);
        if (vmm_map_page_to(p->pml4_phys, va, frame,
                            VMM_USER | VMM_PRESENT | VMM_WRITE | VMM_NX) != 0) {
            panic("proc: failed to map user stack");
        }
    }

    /*
     * Compose the argument block in a kernel buffer, laid out downward from
     * stack_top, then copy it into the top stack page(s) through the HHDM.
     * Runtime layout (ascending addresses, SysV ABI): argc, argv[], NULL,
     * envp[], NULL, auxv[], AT_NULL, then the argv[0] string at the very top.
     * Everything the ABI parses sits at or above the initial RSP; the string
     * is topmost so user frames growing down from RSP can never clobber it.
     */
    uint8_t block[256];
    uint64_t off = sizeof(block);
    memset(block, 0, sizeof(block));

    off -= 32; /* argv[0] string, 32 bytes (topmost) */
    uint64_t argv0_va = stack_top - (sizeof(block) - off);
    memcpy(block + off, progname, strlen(progname) + 1);

    off -= 48; /* auxv: AT_PAGESZ, AT_MICROKERNEL_BOOTINFO, AT_NULL */
    unsigned long *auxv = (unsigned long *)(block + off);
    auxv[0] = 6; /* AT_PAGESZ */
    auxv[1] = 4096;
    auxv[2] = AT_MICROKERNEL_BOOTINFO;
    auxv[3] = BOOTINFO_VA;
    auxv[4] = 0; /* AT_NULL */
    auxv[5] = 0;

    off -= 32; /* argc, argv[0], argv[1]=NULL, envp[0]=NULL */
    uint64_t sp = stack_top - (sizeof(block) - off); /* initial user RSP */
    *(uint64_t *)(block + off + 0) = 1;             /* argc */
    *(uint64_t *)(block + off + 8) = argv0_va;      /* argv[0] */
    *(uint64_t *)(block + off + 16) = 0;            /* argv[1] = NULL */
    *(uint64_t *)(block + off + 24) = 0;            /* envp[0] = NULL */

    uint64_t block_size = sizeof(block) - off;
    uint64_t block_va = stack_top - block_size; /* 16-byte aligned */

    for (uint64_t done = 0; done < block_size;) {
        uint64_t va = block_va + done;
        uint64_t page = va & ~0xFFFULL;
        uint64_t pgoff = va & 0xFFF;
        uint64_t n = 4096 - pgoff;
        if (n > block_size - done) {
            n = block_size - done;
        }
        uint64_t frame = vmm_get_phys_to(p->pml4_phys, page);
        if (!frame) {
            panic("proc: stack frame missing during arg copy");
        }
        memcpy(phys_to_virt(frame) + pgoff, block + off + done, n);
        done += n;
    }

    /* Boot info page, mapped read-only into the user address space. */
    boot_info_t *kbi = kzalloc(sizeof(boot_info_t));
    kbi->magic = BOOTINFO_MAGIC;
    kbi->boot_tar_phys = bi->boot_tar_phys;
    kbi->boot_tar_size = bi->boot_tar_size;
    kbi->rsdp_phys = bi->rsdp_phys;
    kbi->mem_total_bytes = bi->mem_total_bytes;
    memcpy(kbi->cmdline, bi->cmdline, sizeof(kbi->cmdline) - 1);
    for (uint64_t va = BOOTINFO_VA; va < BOOTINFO_VA + 4096; va += 4096) {
        uint64_t frame = pmm_alloc_frame();
        if (!frame) {
            panic("proc: out of frames for bootinfo page");
        }
        uint64_t idx = va - BOOTINFO_VA;
        uint64_t n = sizeof(boot_info_t) - idx;
        if (n > 4096) {
            n = 4096;
        }
        memset(phys_to_virt(frame), 0, 4096);
        memcpy(phys_to_virt(frame), (uint8_t *)kbi + idx, n);
        vmm_map_page_to(p->pml4_phys, va, frame,
                        VMM_USER | VMM_PRESENT | VMM_NX);
    }
    kfree(kbi);

    return sp; /* initial user RSP (points at argc) */
}

static boot_info_t g_bootinfo;

/* Load one ustar bootfs image into a fresh process and make it runnable. */
static pcb_t *spawn_image(uint64_t tar_phys, uint64_t tar_size,
                          const char *name, uint8_t prio, int precreate_chan) {
    ustar_iter_t it;
    ustar_open(&it, tar_phys, tar_size);
    const void *image = NULL;
    uint64_t image_size = 0;
    while (ustar_next(&it)) {
        if (kstrcmp(it.name, name) == 0) {
            image = ustar_data(&it);
            image_size = it.size;
            break;
        }
    }
    if (!image) {
        printk("[proc] '%s' not found in bootfs (skipped)\n", name);
        return NULL;
    }

    pcb_t *p = proc_create(NULL);
    if (!p) {
        return NULL;
    }
    uint64_t entry = 0, brk = 0;
    if (elf_load_user(p->pml4_phys, image, image_size, &entry, &brk) != 0) {
        printk("[proc] failed to load '%s' ELF\n", name);
        return NULL;
    }
    p->brk = brk;

    uint64_t user_rsp = build_user_stack(p, name, &g_bootinfo);
    if (precreate_chan) {
        channel_create(p->pid);
    }
    thread_create(p, entry, user_rsp, prio);
    printk("[proc] spawned '%s' pid=%u entry=%llx rsp=%llx prio=%u\n", name,
           p->pid, (unsigned long long)entry, (unsigned long long)user_rsp,
           prio);
    return p;
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
    memcpy(g_bootinfo.cmdline, cmd, strlen(cmd) < sizeof(g_bootinfo.cmdline) - 1
                                        ? strlen(cmd) + 1
                                        : sizeof(g_bootinfo.cmdline) - 1);

    /* PID 1: system process manager / pathname manager. Its syscall-created
     * channel (chanid 1) is the name registry endpoint. */
    pcb_t *p1 = spawn_image(tar_phys, tar_size, "sysproc", 100, 1);
    if (!p1) {
        panic("proc: failed to spawn sysproc");
    }
    /* Demo processes for the IPC milestone: an echo server that registers
     * itself with sysproc, and a client that resolves and exercises it. */
    spawn_image(tar_phys, tar_size, "ipcserv", 100, 0);
    spawn_image(tar_phys, tar_size, "ipccli", 100, 0);
    /* Milestone 4 demo: threads + signal delivery (worker thread, caught
     * SIGUSR1, then terminates itself with an unhandled SIGUSR2). */
    spawn_image(tar_phys, tar_size, "sigtest", 100, 0);

    sched_activate();
}
