#include <stdint.h>
#include <kernel/printk.h>
#include <kernel/panic.h>
#include <kernel/multiboot.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/kheap.h>
#include <kernel/sched.h>
#include "../arch/x86_64/gdt.h"
#include "../arch/x86_64/idt.h"
#include "../arch/x86_64/serial.h"
#include "../arch/x86_64/apic.h"
#include "../arch/x86_64/msr.h"

extern void syscall_entry(void);

static void syscall_init(void) {
    uint64_t efer = rdmsr(0xC0000080);
    wrmsr(0xC0000080, efer | 1); /* IA32_EFER.SCE */
    /* STAR: kernel CS/SS = 0x08/0x10; user base 0x10 -> sysret CS=0x20 SS=0x18 */
    wrmsr(0xC0000081, ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32));
    wrmsr(0xC0000082, (uint64_t)(uintptr_t)syscall_entry); /* LSTAR */
    wrmsr(0xC0000084, 0x45700); /* SFMASK: IF|TF|DF|NT|AC */
    printk("[syscall] MSR fast syscalls enabled (LSTAR=%p)\n", syscall_entry);
}

void kmain64(uint64_t mbi_phys) {
    serial_init();
    printk("\n[microkernel] Microkernel microkernel (x86_64, higher-half) booting\n");
    printk("[microkernel] Multiboot2 MBI at phys %p\n", (void *)mbi_phys);

    gdt_init();
    idt_init();

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

    size_t nmod;
    const mb_module_t *mods = mb_modules(&nmod);
    for (size_t i = 0; i < nmod; i++) {
        printk("[microkernel] boot module %llu: %s [%llx..%llx)\n",
               (unsigned long long)i, mods[i].cmdline ? mods[i].cmdline : "?",
               (unsigned long long)mods[i].start,
               (unsigned long long)mods[i].end);
    }

    apic_init();
    printk("[OK] PMM/VMM/APIC initialized\n");

    tss_init();
    sched_init();
    syscall_init();
    proc_spawn_init();
    printk("[OK] dispatching to PID 1 (sysproc) in Ring 3\n");
    schedule();
    panic("kmain: schedule returned");
}
