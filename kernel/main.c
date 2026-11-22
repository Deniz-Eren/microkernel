/**
 * @file     main.c
 * @brief    Higher-half kernel entry point and SMP subsystem bring-up
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdint.h>
#include <kernel/acpi.h>
#include <kernel/arch.h>
#include <kernel/kheap.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/pmm.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/smp.h>
#include <kernel/vmm.h>

/* need:: COMP_BOOT_0001 COMP_BOOT_0004 COMP_PROC_0008 */

void kmain64(uint64_t mbi_phys) {
    arch_early_init();
    printk("\n[microkernel] Microkernel (%s, higher-half) booting\n",
           MICROKERNEL_ARCH);
    printk("[microkernel] Multiboot2 MBI at phys %p\n", (void *)mbi_phys);

    arch_cpu_init();
    smp_init_bsp();

    multiboot_parse(mbi_phys);
    printk("[microkernel] cmdline: \"%s\"\n", mb_cmdline());
    printk("[microkernel] usable RAM: %llu KiB\n",
           (unsigned long long)(mb_total_usable() / 1024));

    pmm_init();
    printk("[OK] PMM initialized: %llu KiB free of %llu KiB\n",
           (unsigned long long)(pmm_free_bytes() / 1024),
           (unsigned long long)(pmm_total_bytes() / 1024));

    vmm_init();
    kheap_init();
    acpi_init();

    size_t nmod;
    const mb_module_t *mods = mb_modules(&nmod);
    for (size_t i = 0; i < nmod; i++) {
        printk("[microkernel] boot module %llu: %s [%llx..%llx)\n",
               (unsigned long long)i, mods[i].cmdline ? mods[i].cmdline : "?",
               (unsigned long long)mods[i].start,
               (unsigned long long)mods[i].end);
    }

    arch_timer_init();
    printk("[OK] PMM/VMM/APIC initialized\n");

    sched_init();
    arch_user_entry_init();
    smp_boot_secondary_cpus();

    proc_spawn_init();
    printk("[OK] dispatching to PID 1 (sysproc) in Ring 3\n");
    sched_idle_loop();
}
