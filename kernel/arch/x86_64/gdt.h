#pragma once
#include <stdint.h>

/* Note the user-order: SYSRET loads SS = STAR[63:48]+8 and
 * CS = STAR[63:48]+16, so UDATA must precede UCODE in the GDT. */
#define GDT_KCODE 0x08
#define GDT_KDATA 0x10
#define GDT_UDATA 0x18
#define GDT_UCODE 0x20
#define GDT_TSS   0x28

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

void gdt_init(void);
void gdt_install_tss(uint64_t tss_vaddr, uint32_t tss_limit);
void tss_init(void);
void tss_set_rsp0(uint64_t rsp0);
