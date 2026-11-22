/**
 * @file     smp.h
 * @brief    Portable per-CPU state and symmetric multiprocessing interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: COMP_PROC_0008 */

#include <stddef.h>
#include <stdint.h>

#define SMP_MAX_CPUS 16u

struct tcb;

/* Per-CPU state block referenced via GS.base on x86-64 (TPIDR_EL1 on arm64).
 * Assembly in switch.S reads syscall_kstack (offset 8) and syscall_rsp_tmp
 * (offset 16), and arch_get_cpu_local() reads self (offset 0). */
typedef struct cpu_local {
    struct cpu_local *self;   /* offset 0:  self pointer (%gs:0) */
    uint64_t syscall_kstack;  /* offset 8:  active thread kernel stack top */
    uint64_t syscall_rsp_tmp; /* offset 16: syscall user RSP scratch */
    struct tcb *current;      /* offset 24: active TCB on this CPU */
    struct tcb *idle_tcb;     /* offset 32: private idle thread for this CPU */
    struct tcb *prev_switch; /* offset 40: outgoing TCB finalized post-switch */
    uint64_t sched_irq_flags; /* offset 48: saved IRQ state across switch_to */
    uint32_t cpu_index;       /* 0 = BSP, 1..N-1 = APs */
    uint32_t hw_id;           /* hardware CPU identifier (LAPIC ID) */
    volatile uint8_t online;  /* 1 once AP has completed bring-up */
    volatile uint8_t is_idle; /* 1 while executing idle_tcb */
    uint64_t ticks;           /* per-CPU timer tick counter */
} __attribute__((aligned(64))) cpu_local_t;

void smp_init_bsp(void);
void smp_boot_secondary_cpus(void);
uint32_t smp_online_cpus(void);
uint32_t smp_cpu_count(void);
cpu_local_t *smp_cpu(uint32_t cpu_index);
cpu_local_t *smp_current_cpu(void);
