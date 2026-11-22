/**
 * @file     arch.c
 * @brief    x86-64 implementation of the kernel Hardware Abstraction Layer
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/arch.h>
#include <kernel/printk.h>
#include "apic.h"
#include "gdt.h"
#include "idt.h"
#include "msr.h"
#include "serial.h"

/* need:: COMP_BOOT_0004 */

#define MSR_FS_BASE 0xC0000100U

extern void syscall_entry(void);

static int g_rdrand_checked;
static int g_has_rdrand;

void arch_early_init(void) {
    serial_init();
}

void arch_cpu_init(void) {
    gdt_init();
    idt_init();
}

void arch_timer_init(void) {
    apic_init();
}

void arch_user_entry_init(void) {
    tss_init();
    uint64_t efer = rdmsr(0xC0000080);
    wrmsr(0xC0000080, efer | 1ULL); /* IA32_EFER.SCE */
    /* STAR: kernel CS/SS = 0x08/0x10; user base 0x10 -> sysret CS=0x20 SS=0x18
     */
    wrmsr(0xC0000081, ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32));
    wrmsr(0xC0000082, (uint64_t)(uintptr_t)syscall_entry); /* LSTAR */
    wrmsr(0xC0000084, 0x45700ULL); /* SFMASK: IF|TF|DF|NT|AC */
    printk("[syscall] MSR fast syscalls enabled (LSTAR=%p)\n", syscall_entry);
}

void arch_console_putc(char c) {
    serial_putc(c);
}

void arch_console_write(const char *s, size_t n) {
    serial_write(s, n);
}

void arch_set_kernel_stack(uint64_t kstack_top) {
    tss_set_rsp0(kstack_top);
}

void arch_set_tls_base(uint64_t base) {
    wrmsr(MSR_FS_BASE, base);
}

uint64_t arch_entropy_hw_sample(void) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t sample = ((uint64_t)hi << 32) | lo;

    if (!g_rdrand_checked) {
        uint32_t eax = 1u;
        uint32_t ebx = 0u;
        uint32_t ecx = 0u;
        uint32_t edx = 0u;
        __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
        g_has_rdrand = (ecx & (1u << 30)) != 0u;
        g_rdrand_checked = 1;
    }
    if (g_has_rdrand) {
        for (int i = 0; i < 10; i++) {
            uint64_t v = 0;
            unsigned char ok = 0;
            __asm__ volatile("rdrand %0; setc %1" : "=r"(v), "=qm"(ok));
            if (ok) {
                sample ^= v;
                break;
            }
        }
    }
    return sample;
}

void arch_fpu_init_template(uint8_t *fpu_area) {
    __asm__ volatile("fxsave %0" : : "m"(*fpu_area) : "memory");
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10); /* OSFXSR | OSXMMEXCPT */
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");
}

void arch_fpu_save_current(uint8_t *fpu_area) {
    __asm__ volatile("fxsave %0" : : "m"(*fpu_area) : "memory");
}

void arch_irq_eoi(void) {
    apic_eoi();
}

uint32_t arch_bsp_cpu_id(void) {
    return (lapic_read(0x020) >> 24) & 0xFFu;
}

void arch_cpu_idle_wait(void) {
    __asm__ volatile("sti; hlt; cli" : : : "memory");
}

void arch_first_switch(uint64_t kernel_rsp) {
    __asm__ volatile("mov %0, %%rsp\n"
                     "popq %%r15\n"
                     "popq %%r14\n"
                     "popq %%r13\n"
                     "popq %%r12\n"
                     "popq %%rbp\n"
                     "popq %%rbx\n"
                     "ret\n" ::"r"(kernel_rsp)
                     : "memory");
    __builtin_unreachable();
}

void arch_cpu_halt(void) {
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
