/**
 * @file     smp.c
 * @brief    Portable SMP per-CPU registry and secondary CPU bring-up
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/smp.h>
#include <kernel/arch.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/string.h>

/* need:: COMP_PROC_0008 */

_Static_assert(offsetof(cpu_local_t, self) == 0, "cpu_local.self offset");
_Static_assert(offsetof(cpu_local_t, syscall_kstack) == 8,
               "cpu_local.syscall_kstack offset");
_Static_assert(offsetof(cpu_local_t, syscall_rsp_tmp) == 16,
               "cpu_local.syscall_rsp_tmp offset");

static cpu_local_t g_cpus[SMP_MAX_CPUS];
static volatile uint32_t g_online_cpus = 1u;
static uint32_t g_cpu_count = 1u;

void smp_init_bsp(void) {
    memset(g_cpus, 0, sizeof(g_cpus));
    g_cpus[0].self = &g_cpus[0];
    g_cpus[0].cpu_index = 0u;
    g_cpus[0].hw_id = 0u;
    g_cpus[0].online = 1u;
    g_cpus[0].is_idle = 1u;
    g_online_cpus = 1u;
    g_cpu_count = 1u;
    arch_set_cpu_local(&g_cpus[0]);
}

static void smp_ap_entry(uint32_t cpu_index) {
    cpu_local_t *cpu = &g_cpus[cpu_index];
    cpu->self = cpu;
    cpu->cpu_index = cpu_index;
    cpu->is_idle = 1u;
    arch_ap_cpu_init(cpu_index, cpu);
    cpu->hw_id = arch_cpu_hw_id();

    sched_init_cpu_idle(cpu);

    __atomic_store_n(&cpu->online, 1u, __ATOMIC_RELEASE);
    __atomic_fetch_add(&g_online_cpus, 1u, __ATOMIC_ACQ_REL);

    sched_ap_enter_idle();
}

void smp_boot_secondary_cpus(void) {
    g_cpus[0].hw_id = arch_cpu_hw_id();
    uint32_t started = arch_smp_boot_aps(smp_ap_entry);
    g_cpu_count = 1u + started;
    printk("[OK] SMP online: %u CPUs ready\n", smp_online_cpus());
}

uint32_t smp_online_cpus(void) {
    return __atomic_load_n(&g_online_cpus, __ATOMIC_ACQUIRE);
}

uint32_t smp_cpu_count(void) {
    return g_cpu_count;
}

cpu_local_t *smp_cpu(uint32_t cpu_index) {
    if (cpu_index >= SMP_MAX_CPUS) {
        return NULL;
    }
    return &g_cpus[cpu_index];
}

cpu_local_t *smp_current_cpu(void) {
    cpu_local_t *cpu = arch_get_cpu_local();
    return cpu ? cpu : &g_cpus[0];
}
