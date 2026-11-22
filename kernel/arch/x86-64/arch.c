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
#include <kernel/string.h>
#include <sys/bootinfo.h>
#include "apic.h"
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "msr.h"
#include "serial.h"

/* need:: COMP_BOOT_0004 COMP_BOOT_0005 */

#define MSR_FS_BASE 0xC0000100U

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

/* Early CPUID gate functions must stay baseline x86-64 so legacy CPUs print
 * the boot rejection diagnostic instead of faulting with #UD. */
#define ARCH_EARLY_FN                                                          \
    __attribute__((target("arch=x86-64,no-mmx,no-sse,no-sse2,no-avx,"          \
                          "no-avx2,no-fma,no-f16c,no-bmi,no-bmi2,no-lzcnt,"    \
                          "no-popcnt,no-movbe")))

extern void syscall_entry(void);

/* Referenced by switch.S for XSAVE/XRSTOR RFBM masks. */
uint32_t g_xsave_mask_lo = (uint32_t)XCR0_V3_MASK;
uint32_t g_xsave_mask_hi = 0u;

static uint32_t g_xsave_size = XSAVE_MIN_SIZE;
static uint32_t g_cpu_features;
static int g_has_rdrand;
static int g_has_rdseed;
static int g_has_avx512;

ARCH_EARLY_FN static void cpuid_query(uint32_t leaf, uint32_t subleaf,
                                      uint32_t *eax, uint32_t *ebx,
                                      uint32_t *ecx, uint32_t *edx) {
    uint32_t a = leaf;
    uint32_t b = 0u;
    uint32_t c = subleaf;
    uint32_t d = 0u;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
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

static void init_xsave_state(void) {
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
    return (lapic_read(0x020) >> 24) & 0xFFu;
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
