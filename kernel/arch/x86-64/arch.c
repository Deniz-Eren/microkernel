/**
 * @file     arch.c
 * @brief    x86-64 implementation of the kernel Hardware Abstraction Layer
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/acpi.h>
#include <kernel/arch.h>
#include <kernel/kheap.h>
#include <kernel/pmm.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/smp.h>
#include <kernel/string.h>
#include <kernel/vmm.h>
#include <sys/bootinfo.h>
#include "apic.h"
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "msr.h"
#include "serial.h"

/* need:: COMP_BOOT_0004 COMP_BOOT_0005 COMP_PROC_0008 */

#define MSR_FS_BASE 0xC0000100U
#define MSR_GS_BASE 0xC0000101U
#define MSR_KERNEL_GS_BASE 0xC0000102U

#define XCR0_X87 (1ULL << 0)
#define XCR0_SSE (1ULL << 1)
#define XCR0_AVX (1ULL << 2)
#define XCR0_OPMASK (1ULL << 5)
#define XCR0_ZMM_HI256 (1ULL << 6)
#define XCR0_HI16_ZMM (1ULL << 7)
#define XCR0_AVX512_MASK (XCR0_OPMASK | XCR0_ZMM_HI256 | XCR0_HI16_ZMM)
#define XCR0_V3_MASK (XCR0_X87 | XCR0_SSE | XCR0_AVX)

#define XSAVE_MIN_SIZE 576u
#define XSAVE_ALIGN 64u

#define AP_TRAMP_PHYS 0x8000ULL
#define AP_TRAMP_VECTOR 0x08u
#define AP_MAILBOX_PHYS 0x8F00ULL

/* Early CPUID gate functions must stay baseline x86-64 so legacy CPUs print
 * the boot rejection diagnostic instead of faulting with #UD. */
#define ARCH_EARLY_FN                                                          \
    __attribute__((target("arch=x86-64,no-mmx,no-sse,no-sse2,no-avx,"          \
                          "no-avx2,no-fma,no-f16c,no-bmi,no-bmi2,no-lzcnt,"    \
                          "no-popcnt,no-movbe")))

extern void syscall_entry(void);
extern const uint8_t ap_trampoline_start[];
extern const uint8_t ap_trampoline_end[];

/* Referenced by switch.S for XSAVE/XRSTOR RFBM masks. */
uint32_t g_xsave_mask_lo = (uint32_t)XCR0_V3_MASK;
uint32_t g_xsave_mask_hi = 0u;

static uint32_t g_xsave_size = XSAVE_MIN_SIZE;
static uint32_t g_cpu_features;
static int g_has_rdrand;
static int g_has_rdseed;
static int g_has_avx512;
static int g_gs_ready;
static void (*g_ap_entry_cb)(uint32_t cpu_index);

typedef struct {
    uint64_t cr3;
    uint64_t stack_top;
    uint64_t entry;
    uint64_t cpu_index;
} ap_mailbox_t;

ARCH_EARLY_FN static void cpuid_query(uint32_t leaf, uint32_t subleaf,
                                      uint32_t *eax, uint32_t *ebx,
                                      uint32_t *ecx, uint32_t *edx) {
    uint32_t a = leaf;
    uint32_t b = 0u;
    uint32_t c = subleaf;
    uint32_t d = 0u;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "+d"(d));
    *eax = a;
    *ebx = b;
    *ecx = c;
    *edx = d;
}

ARCH_EARLY_FN static void early_puts(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    serial_write(s, n);
}

ARCH_EARLY_FN static void early_check_bit(uint32_t reg, uint32_t bit,
                                          const char *name, int *missing) {
    if ((reg & (1u << bit)) == 0u) {
        if (*missing == 0) {
            early_puts("[FAIL] unsupported CPU: x86-64-v3 baseline required"
                       " (missing:");
        }
        early_puts(" ");
        early_puts(name);
        (*missing)++;
    }
}

ARCH_EARLY_FN static void verify_x86_64_v3_baseline(void) {
    uint32_t max_basic = 0u;
    uint32_t max_ext = 0u;
    uint32_t eax = 0u;
    uint32_t ebx = 0u;
    uint32_t ecx = 0u;
    uint32_t edx = 0u;

    cpuid_query(0u, 0u, &max_basic, &ebx, &ecx, &edx);
    cpuid_query(0x80000000u, 0u, &max_ext, &ebx, &ecx, &edx);

    if (max_basic < 0x0Du || max_ext < 0x80000001u) {
        early_puts("[FAIL] unsupported CPU: x86-64-v3 baseline required"
                   " (CPUID leaves < 0x0D)\n");
        early_puts("[FAIL] system halted\n");
        arch_cpu_halt();
    }

    uint32_t f1_ecx = 0u;
    uint32_t f1_edx = 0u;
    cpuid_query(1u, 0u, &eax, &ebx, &f1_ecx, &f1_edx);

    uint32_t f7_ebx = 0u;
    cpuid_query(7u, 0u, &eax, &f7_ebx, &ecx, &edx);

    uint32_t ext1_ecx = 0u;
    cpuid_query(0x80000001u, 0u, &eax, &ebx, &ext1_ecx, &edx);

    int missing = 0;
    early_check_bit(f1_edx, 24u, "FXSR", &missing);
    early_check_bit(f1_edx, 25u, "SSE", &missing);
    early_check_bit(f1_edx, 26u, "SSE2", &missing);
    early_check_bit(f1_ecx, 0u, "SSE3", &missing);
    early_check_bit(f1_ecx, 9u, "SSSE3", &missing);
    early_check_bit(f1_ecx, 12u, "FMA", &missing);
    early_check_bit(f1_ecx, 13u, "CMPXCHG16B", &missing);
    early_check_bit(f1_ecx, 19u, "SSE4.1", &missing);
    early_check_bit(f1_ecx, 20u, "SSE4.2", &missing);
    early_check_bit(f1_ecx, 22u, "MOVBE", &missing);
    early_check_bit(f1_ecx, 23u, "POPCNT", &missing);
    early_check_bit(f1_ecx, 26u, "XSAVE", &missing);
    early_check_bit(f1_ecx, 28u, "AVX", &missing);
    early_check_bit(f1_ecx, 29u, "F16C", &missing);
    early_check_bit(f7_ebx, 3u, "BMI1", &missing);
    early_check_bit(f7_ebx, 5u, "AVX2", &missing);
    early_check_bit(f7_ebx, 8u, "BMI2", &missing);
    early_check_bit(ext1_ecx, 0u, "LAHF_LM", &missing);
    early_check_bit(ext1_ecx, 5u, "LZCNT", &missing);

    if (missing > 0) {
        early_puts(")\n[FAIL] system halted\n");
        arch_cpu_halt();
    }

    g_has_rdrand = (f1_ecx & (1u << 30)) != 0u;
    g_has_rdseed = (f7_ebx & (1u << 18)) != 0u;
    g_has_avx512 = (f7_ebx & (1u << 16)) != 0u;
    g_cpu_features = BOOT_CPU_X86_64_V3;
}

static void enable_xsave_cr_bits(void) {
    /* Configure CR0 for native x87/SIMD execution (MP=1, EM=0, TS=0, NE=1). */
    uint64_t cr0 = 0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 = (cr0 & ~((1ULL << 2) | (1ULL << 3))) | (1ULL << 1) | (1ULL << 5);
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");

    /* Enable OSFXSR (bit 9), OSXMMEXCPT (bit 10), and OSXSAVE (bit 18). */
    uint64_t cr4 = 0;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10) | (1ULL << 18);
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");
}

static void init_xsave_state(void) {
    enable_xsave_cr_bits();

    /* Query supported XCR0 components from CPUID leaf 0x0D subleaf 0. */
    uint32_t sup_lo = 0u;
    uint32_t ebx = 0u;
    uint32_t ecx = 0u;
    uint32_t sup_hi = 0u;
    cpuid_query(0x0Du, 0u, &sup_lo, &ebx, &ecx, &sup_hi);
    uint64_t supported_xcr0 = ((uint64_t)sup_hi << 32) | sup_lo;

    if ((supported_xcr0 & XCR0_V3_MASK) != XCR0_V3_MASK) {
        early_puts("[FAIL] unsupported CPU: XCR0 lacks x87/SSE/AVX support\n");
        early_puts("[FAIL] system halted\n");
        arch_cpu_halt();
    }

    uint64_t xcr0 = XCR0_V3_MASK;
    if (g_has_avx512 &&
        (supported_xcr0 & XCR0_AVX512_MASK) == XCR0_AVX512_MASK) {
        xcr0 |= XCR0_AVX512_MASK;
        g_cpu_features |= BOOT_CPU_AVX512;
    } else {
        g_has_avx512 = 0;
    }

    g_xsave_mask_lo = (uint32_t)(xcr0 & 0xFFFFFFFFULL);
    g_xsave_mask_hi = (uint32_t)(xcr0 >> 32);

    __asm__ volatile("xsetbv"
                     :
                     : "c"(0u), "a"(g_xsave_mask_lo), "d"(g_xsave_mask_hi)
                     : "memory");

    /* Re-query leaf 0x0D subleaf 0: EBX gives the XSAVE area size for XCR0. */
    uint32_t area_size = 0u;
    cpuid_query(0x0Du, 0u, &sup_lo, &area_size, &ecx, &sup_hi);
    if (area_size < XSAVE_MIN_SIZE) {
        area_size = XSAVE_MIN_SIZE;
    }
    g_xsave_size = area_size;

    printk("[OK] CPU x86-64-v3 verified: XSAVE=%u bytes XCR0=0x%llx"
           " avx512=%s\n",
           g_xsave_size, (unsigned long long)xcr0,
           g_has_avx512 ? "enabled" : "absent");
}

static void init_syscall_msrs(void) {
    uint64_t efer = rdmsr(0xC0000080);
    wrmsr(0xC0000080, efer | 1ULL); /* IA32_EFER.SCE */
    /* STAR: kernel CS/SS = 0x08/0x10; user base 0x10 -> sysret CS=0x20 SS=0x18
     */
    wrmsr(0xC0000081, ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32));
    wrmsr(0xC0000082, (uint64_t)(uintptr_t)syscall_entry); /* LSTAR */
    wrmsr(0xC0000084, 0x45700ULL); /* SFMASK: IF|TF|DF|NT|AC */
}

ARCH_EARLY_FN void arch_early_init(void) {
    serial_init();
    verify_x86_64_v3_baseline();
}

void arch_cpu_init(void) {
    gdt_init();
    idt_init();
    init_xsave_state();
}

void arch_timer_init(void) {
    apic_init();
}

void arch_user_entry_init(void) {
    tss_init();
    init_syscall_msrs();
    printk("[syscall] MSR fast syscalls enabled (LSTAR=%p)\n", syscall_entry);
}

void arch_set_cpu_local(cpu_local_t *cpu) {
    wrmsr(MSR_GS_BASE, (uint64_t)(uintptr_t)cpu);
    wrmsr(MSR_KERNEL_GS_BASE, 0ULL);
    __atomic_store_n(&g_gs_ready, 1, __ATOMIC_RELEASE);
}

cpu_local_t *arch_get_cpu_local(void) {
    if (!__atomic_load_n(&g_gs_ready, __ATOMIC_ACQUIRE)) {
        return NULL;
    }
    cpu_local_t *cpu = NULL;
    __asm__ volatile("movq %%gs:0, %0" : "=r"(cpu));
    return cpu;
}

uint32_t arch_cpu_index(void) {
    cpu_local_t *cpu = arch_get_cpu_local();
    return cpu ? cpu->cpu_index : 0u;
}

uint32_t arch_cpu_hw_id(void) {
    return (lapic_read(0x020) >> 24) & 0xFFu;
}

uint64_t arch_irq_save_disable(void) {
    uint64_t flags = 0;
    __asm__ volatile("pushfq\n"
                     "popq %0\n"
                     "cli\n"
                     : "=r"(flags)
                     :
                     : "memory");
    return flags;
}

void arch_irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) {
        __asm__ volatile("sti" : : : "memory");
    }
}

void arch_ap_cpu_init(uint32_t cpu_index, cpu_local_t *cpu) {
    /* Switch off the temporary trampoline PML4 onto the real kernel PML4. */
    vmm_set_cr3(vmm_kernel_pml4());

    /* Enable SMEP and SMAP on this AP to match the BSP. */
    uint64_t cr4 = 0;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 20) | (1ULL << 21);
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");

    /* Load this AP's private GDT and TSS, shared IDT, and GS.base. */
    gdt_init_cpu(cpu_index);
    tss_init_cpu(cpu_index);
    idt_load_ap();
    arch_set_cpu_local(cpu);

    /* Match BSP x86-64-v3 XSAVE/XCR0 state (ADR_0026). */
    enable_xsave_cr_bits();
    __asm__ volatile("xsetbv"
                     :
                     : "c"(0u), "a"(g_xsave_mask_lo), "d"(g_xsave_mask_hi)
                     : "memory");

    init_syscall_msrs();
    apic_init_ap();
}

static void arch_ap_tramp_entry(uint64_t cpu_index) {
    if (g_ap_entry_cb) {
        g_ap_entry_cb((uint32_t)cpu_index);
    }
    arch_cpu_halt();
}

static void wait_ms(uint64_t ms) {
    uint64_t start = timer_ticks();
    while (timer_ticks() - start < ms) {
        arch_cpu_relax();
    }
}

uint32_t arch_smp_boot_aps(void (*ap_entry)(uint32_t cpu_index)) {
    const acpi_info_t *ai = acpi_info();
    uint32_t bsp_id = arch_cpu_hw_id();
    if (!ai->madt_found || ai->cpu_count <= 1u) {
        return 0u;
    }

    size_t tramp_sz = (size_t)(ap_trampoline_end - ap_trampoline_start);
    if (tramp_sz == 0u || tramp_sz > 0x0E00u) {
        printk("[smp] AP trampoline too large (%llu bytes)\n",
               (unsigned long long)tramp_sz);
        return 0u;
    }

    /* Build a temporary 4-level PML4 mapping 0x8000 RWX + kernel higher half.
     */
    uint64_t tpml4_p = pmm_alloc_frame();
    uint64_t tpdpt_p = pmm_alloc_frame();
    uint64_t tpd_p = pmm_alloc_frame();
    uint64_t tpt_p = pmm_alloc_frame();
    if (!tpml4_p || !tpdpt_p || !tpd_p || !tpt_p) {
        if (tpml4_p) {
            pmm_free_frame(tpml4_p);
        }
        if (tpdpt_p) {
            pmm_free_frame(tpdpt_p);
        }
        if (tpd_p) {
            pmm_free_frame(tpd_p);
        }
        if (tpt_p) {
            pmm_free_frame(tpt_p);
        }
        return 0u;
    }

    uint64_t *tpml4 = (uint64_t *)phys_to_virt(tpml4_p);
    uint64_t *tpdpt = (uint64_t *)phys_to_virt(tpdpt_p);
    uint64_t *tpd = (uint64_t *)phys_to_virt(tpd_p);
    uint64_t *tpt = (uint64_t *)phys_to_virt(tpt_p);
    memset(tpml4, 0, 4096);
    memset(tpdpt, 0, 4096);
    memset(tpd, 0, 4096);
    memset(tpt, 0, 4096);

    const uint64_t *kpml4 = (const uint64_t *)phys_to_virt(vmm_kernel_pml4());
    for (int i = 256; i < 512; i++) {
        tpml4[i] = kpml4[i];
    }
    tpml4[0] = tpdpt_p | VMM_PRESENT | VMM_WRITE;
    tpdpt[0] = tpd_p | VMM_PRESENT | VMM_WRITE;
    tpd[0] = tpt_p | VMM_PRESENT | VMM_WRITE;
    tpt[AP_TRAMP_PHYS >> 12] = AP_TRAMP_PHYS | VMM_PRESENT | VMM_WRITE;

    memcpy(phys_to_virt(AP_TRAMP_PHYS), ap_trampoline_start, tramp_sz);
    g_ap_entry_cb = ap_entry;

    volatile ap_mailbox_t *mb =
        (volatile ap_mailbox_t *)phys_to_virt(AP_MAILBOX_PHYS);
    uint32_t started = 0u;

    for (uint32_t i = 0; i < ai->cpu_count && (1u + started) < SMP_MAX_CPUS;
         i++) {
        uint32_t lapic_id = ai->cpu_lapic_ids[i];
        if (lapic_id == bsp_id) {
            continue;
        }
        uint32_t cpu_idx = 1u + started;
        uint8_t *kstack = kmalloc(KERNEL_STACK_SIZE);
        if (!kstack) {
            break;
        }
        cpu_local_t *cpu = smp_cpu(cpu_idx);
        mb->cr3 = tpml4_p;
        mb->stack_top = (uint64_t)(uintptr_t)(kstack + KERNEL_STACK_SIZE);
        mb->entry = (uint64_t)(uintptr_t)arch_ap_tramp_entry;
        mb->cpu_index = (uint64_t)cpu_idx;
        __atomic_thread_fence(__ATOMIC_SEQ_CST);

        /* Intel MP INIT-SIPI-SIPI sequence. */
        apic_send_init(lapic_id);
        wait_ms(10u);
        apic_send_sipi(lapic_id, AP_TRAMP_VECTOR);

        /* Poll briefly before sending the optional second SIPI. */
        uint64_t deadline = timer_ticks() + 20u;
        while (!__atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE) &&
               timer_ticks() < deadline) {
            arch_cpu_relax();
        }
        if (!__atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE)) {
            apic_send_sipi(lapic_id, AP_TRAMP_VECTOR);
            deadline = timer_ticks() + 500u;
            while (!__atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE) &&
                   timer_ticks() < deadline) {
                arch_cpu_relax();
            }
        }

        if (__atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE)) {
            printk("[smp] AP cpu=%u lapic_id=%u online\n", cpu_idx, lapic_id);
            started++;
        } else {
            printk("[smp] AP cpu=%u lapic_id=%u timed out\n", cpu_idx,
                   lapic_id);
            kfree(kstack);
        }
    }

    pmm_free_frame(tpt_p);
    pmm_free_frame(tpd_p);
    pmm_free_frame(tpdpt_p);
    pmm_free_frame(tpml4_p);
    return started;
}

void arch_smp_send_resched_ipi(uint32_t cpu_index) {
    cpu_local_t *cpu = smp_cpu(cpu_index);
    if (!cpu || !__atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE)) {
        return;
    }
    apic_send_ipi(cpu->hw_id, (uint8_t)IPI_RESCHED_VECTOR);
}

void arch_smp_send_tlb_shootdown_ipi(void) {
    apic_send_ipi_others((uint8_t)IPI_TLB_VECTOR);
}

void arch_console_putc(char c) {
    serial_putc(c);
}

void arch_console_write(const char *s, size_t n) {
    serial_write(s, n);
}

void arch_console_write_raw(const char *s, size_t n) {
    serial_write_raw(s, n);
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

    if (g_has_rdseed) {
        for (int i = 0; i < 10; i++) {
            uint64_t v = 0;
            unsigned char ok = 0;
            __asm__ volatile("rdseed %0; setc %1" : "=r"(v), "=qm"(ok));
            if (ok) {
                sample ^= v;
                break;
            }
        }
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

uint32_t arch_cpu_features(void) {
    return g_cpu_features;
}

size_t arch_fpu_state_size(void) {
    return (size_t)g_xsave_size;
}

size_t arch_fpu_state_align(void) {
    return (size_t)XSAVE_ALIGN;
}

void arch_fpu_init_template(uint8_t *fpu_area) {
    /* Zero full XSAVE area (clears xcomp_bv and reserved header bytes). */
    memset(fpu_area, 0, g_xsave_size);
    uint32_t mxcsr = 0x1F80u; /* IEEE-754 default: all exceptions masked, RN */
    __asm__ volatile("fninit\n"
                     "ldmxcsr %0\n"
                     :
                     : "m"(mxcsr)
                     : "memory");
    arch_fpu_save_current(fpu_area);
}

void arch_fpu_save_current(uint8_t *fpu_area) {
    __asm__ volatile("xsave (%0)"
                     :
                     : "r"(fpu_area), "a"(g_xsave_mask_lo), "d"(g_xsave_mask_hi)
                     : "memory");
}

void arch_fpu_restore_current(const uint8_t *fpu_area) {
    __asm__ volatile("xrstor (%0)"
                     :
                     : "r"(fpu_area), "a"(g_xsave_mask_lo), "d"(g_xsave_mask_hi)
                     : "memory");
}

void arch_irq_eoi(void) {
    apic_eoi();
}

uint32_t arch_bsp_cpu_id(void) {
    return arch_cpu_hw_id();
}

#define PCI_CFG_ADDR_PORT 0x0CF8u
#define PCI_CFG_DATA_PORT 0x0CFCu

static uint32_t pci_cfg_addr(uint8_t bus, uint8_t slot, uint8_t func,
                             uint8_t offset) {
    return 0x80000000u | ((uint32_t)bus << 16) |
           (((uint32_t)slot & 0x1Fu) << 11) | (((uint32_t)func & 0x7u) << 8) |
           ((uint32_t)offset & 0xFCu);
}

uint32_t arch_pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func,
                             uint8_t offset) {
    outd(PCI_CFG_ADDR_PORT, pci_cfg_addr(bus, slot, func, offset));
    return ind(PCI_CFG_DATA_PORT);
}

void arch_pci_cfg_write32(uint8_t bus, uint8_t slot, uint8_t func,
                          uint8_t offset, uint32_t val) {
    outd(PCI_CFG_ADDR_PORT, pci_cfg_addr(bus, slot, func, offset));
    outd(PCI_CFG_DATA_PORT, val);
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

ARCH_EARLY_FN void arch_cpu_halt(void) {
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
