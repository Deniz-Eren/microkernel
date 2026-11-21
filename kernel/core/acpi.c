/**
 * @file     acpi.c
 * @brief    ACPI RSDP, RSDT/XSDT, MADT (IOAPIC/ISO), and SPCR table parser
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/acpi.h>
#include <kernel/printk.h>
#include <kernel/string.h>
#include <kernel/vmm.h>

/* need:: COMP_BOOT_0003 */

#define HHDM_PHYS_LIMIT 0x100000000ULL

typedef struct __attribute__((packed)) {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} acpi_rsdp_t;

typedef struct __attribute__((packed)) {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_hdr_t;

typedef struct __attribute__((packed)) {
    acpi_sdt_hdr_t hdr;
    uint32_t lapic_addr;
    uint32_t flags;
} acpi_madt_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
} madt_entry_hdr_t;

typedef struct __attribute__((packed)) {
    madt_entry_hdr_t hdr;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_addr;
    uint32_t gsi_base;
} madt_ioapic_t;

typedef struct __attribute__((packed)) {
    madt_entry_hdr_t hdr;
    uint8_t bus;
    uint8_t source;
    uint32_t gsi;
    uint16_t flags;
} madt_iso_t;

typedef struct __attribute__((packed)) {
    uint8_t addr_space_id;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} acpi_gas_t;

typedef struct __attribute__((packed)) {
    acpi_sdt_hdr_t hdr;
    uint8_t interface_type;
    uint8_t reserved1[3];
    acpi_gas_t base_addr;
    uint8_t irq_type;
    uint8_t pc_irq;
    uint32_t gsi;
    uint8_t baud_rate;
    uint8_t parity;
    uint8_t stop_bits;
    uint8_t flow_ctrl;
    uint8_t term_type;
    uint8_t reserved2;
    uint16_t pci_did;
    uint16_t pci_vid;
    uint8_t pci_bus;
    uint8_t pci_dev;
    uint8_t pci_fn;
    uint32_t pci_flags;
    uint8_t pci_seg;
    uint32_t reserved3;
} acpi_spcr_t;

static acpi_info_t g_acpi;

static int checksum_valid(const uint8_t *p, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum == 0u;
}

static int sig_eq(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static uint64_t scan_rsdp_range(uint64_t start, uint64_t end) {
    for (uint64_t a = start; a + 20ULL <= end; a += 16ULL) {
        const acpi_rsdp_t *r = (const acpi_rsdp_t *)phys_to_virt(a);
        if (!sig_eq(r->signature, "RSD PTR ", 8)) {
            continue;
        }
        if (!checksum_valid((const uint8_t *)r, 20)) {
            continue;
        }
        if (r->revision >= 2u && !checksum_valid((const uint8_t *)r, 36)) {
            continue;
        }
        return a;
    }
    return 0;
}

static uint64_t locate_rsdp(void) {
    uint16_t ebda_seg = *(const uint16_t *)phys_to_virt(0x40EULL);
    uint64_t ebda_phys = (uint64_t)ebda_seg << 4;
    if (ebda_phys >= 0x80000ULL && ebda_phys < 0xA0000ULL) {
        uint64_t found = scan_rsdp_range(ebda_phys, ebda_phys + 1024ULL);
        if (found) {
            return found;
        }
    }
    return scan_rsdp_range(0xE0000ULL, 0x100000ULL);
}

static void parse_madt(const acpi_sdt_hdr_t *hdr) {
    if (hdr->length < sizeof(acpi_madt_t)) {
        return;
    }
    const acpi_madt_t *madt = (const acpi_madt_t *)hdr;
    g_acpi.madt_found = 1u;
    if (madt->lapic_addr != 0u) {
        g_acpi.lapic_phys = (uint64_t)madt->lapic_addr;
    }

    int have_ioapic = 0;
    const uint8_t *base = (const uint8_t *)hdr;
    uint32_t off = (uint32_t)sizeof(acpi_madt_t);
    while (off + sizeof(madt_entry_hdr_t) <= hdr->length) {
        const madt_entry_hdr_t *eh = (const madt_entry_hdr_t *)(base + off);
        if (eh->length < sizeof(madt_entry_hdr_t) ||
            off + eh->length > hdr->length) {
            break;
        }
        if (eh->type == 1u && eh->length >= sizeof(madt_ioapic_t)) {
            const madt_ioapic_t *io = (const madt_ioapic_t *)eh;
            if (!have_ioapic || io->gsi_base == 0u) {
                g_acpi.ioapic_phys = (uint64_t)io->ioapic_addr;
                g_acpi.ioapic_gsi_base = io->gsi_base;
                have_ioapic = 1;
            }
        } else if (eh->type == 2u && eh->length >= sizeof(madt_iso_t)) {
            const madt_iso_t *iso = (const madt_iso_t *)eh;
            if (iso->bus == 0u && iso->source < ACPI_MAX_ISA_IRQS) {
                g_acpi.irq_gsi[iso->source] = iso->gsi;
                g_acpi.irq_flags[iso->source] = iso->flags;
                g_acpi.iso_count++;
            }
        }
        off += eh->length;
    }
}

static void parse_spcr(const acpi_sdt_hdr_t *hdr) {
    if (hdr->length < sizeof(acpi_spcr_t)) {
        return;
    }
    const acpi_spcr_t *spcr = (const acpi_spcr_t *)hdr;
    g_acpi.spcr_found = 1u;
    if (spcr->base_addr.addr_space_id == 1u && spcr->base_addr.address != 0u &&
        spcr->base_addr.address <= 0xFFFFULL) {
        g_acpi.serial_port = (uint16_t)spcr->base_addr.address;
    }
    if ((spcr->irq_type & 0x02u) != 0u && spcr->gsi > 0u) {
        g_acpi.serial_gsi = spcr->gsi;
    } else if (spcr->pc_irq > 0u && spcr->pc_irq < ACPI_MAX_ISA_IRQS) {
        g_acpi.serial_gsi = g_acpi.irq_gsi[spcr->pc_irq];
    }
}

static void inspect_sdt(uint64_t phys) {
    if (phys == 0ULL || phys >= HHDM_PHYS_LIMIT - sizeof(acpi_sdt_hdr_t)) {
        return;
    }
    const acpi_sdt_hdr_t *hdr = (const acpi_sdt_hdr_t *)phys_to_virt(phys);
    if (hdr->length < sizeof(acpi_sdt_hdr_t) || hdr->length > 65536u ||
        phys + hdr->length > HHDM_PHYS_LIMIT) {
        return;
    }
    if (!checksum_valid((const uint8_t *)hdr, hdr->length)) {
        return;
    }
    if (sig_eq(hdr->signature, "APIC", 4)) {
        parse_madt(hdr);
    } else if (sig_eq(hdr->signature, "SPCR", 4)) {
        parse_spcr(hdr);
    }
}

void acpi_init(void) {
    memset(&g_acpi, 0, sizeof(g_acpi));
    g_acpi.lapic_phys = 0xFEE00000ULL;
    g_acpi.ioapic_phys = 0xFEC00000ULL;
    g_acpi.ioapic_gsi_base = 0u;
    for (uint32_t i = 0; i < ACPI_MAX_ISA_IRQS; i++) {
        g_acpi.irq_gsi[i] = i;
        g_acpi.irq_flags[i] = 0u;
    }
    g_acpi.serial_port = 0x3F8u;
    g_acpi.serial_gsi = 4u;
    g_acpi.rtc_gsi = 8u;

    uint64_t rsdp_phys = locate_rsdp();
    g_acpi.rsdp_phys = rsdp_phys;
    if (rsdp_phys == 0ULL) {
        printk("[acpi] RSDP not found; using ISA defaults\n");
        return;
    }

    const acpi_rsdp_t *rsdp = (const acpi_rsdp_t *)phys_to_virt(rsdp_phys);
    int use_xsdt = (rsdp->revision >= 2u && rsdp->xsdt_addr != 0ULL &&
                    rsdp->xsdt_addr < HHDM_PHYS_LIMIT);
    uint64_t root_phys = use_xsdt ? rsdp->xsdt_addr : (uint64_t)rsdp->rsdt_addr;

    if (root_phys != 0ULL && root_phys < HHDM_PHYS_LIMIT) {
        const acpi_sdt_hdr_t *root =
            (const acpi_sdt_hdr_t *)phys_to_virt(root_phys);
        if (root->length >= sizeof(acpi_sdt_hdr_t) && root->length <= 65536u &&
            checksum_valid((const uint8_t *)root, root->length)) {
            uint32_t entry_sz = use_xsdt ? 8u : 4u;
            uint32_t count =
                (root->length - (uint32_t)sizeof(acpi_sdt_hdr_t)) / entry_sz;
            const uint8_t *ents =
                (const uint8_t *)root + sizeof(acpi_sdt_hdr_t);
            for (uint32_t i = 0; i < count; i++) {
                uint64_t tphys = 0ULL;
                if (use_xsdt) {
                    memcpy(&tphys, ents + i * 8u, sizeof(uint64_t));
                } else {
                    uint32_t t32 = 0u;
                    memcpy(&t32, ents + i * 4u, sizeof(uint32_t));
                    tphys = (uint64_t)t32;
                }
                inspect_sdt(tphys);
            }
        }
    }

    if (!g_acpi.spcr_found) {
        g_acpi.serial_gsi = g_acpi.irq_gsi[4];
    }
    g_acpi.rtc_gsi = g_acpi.irq_gsi[8];

    if (g_acpi.madt_found) {
        printk("[OK] ACPI MADT discovered IOAPIC @ 0x%llx (%u ISOs, "
               "serial=0x%x gsi=%u, rtc gsi=%u, spcr=%u)\n",
               (unsigned long long)g_acpi.ioapic_phys, g_acpi.iso_count,
               (unsigned)g_acpi.serial_port, g_acpi.serial_gsi, g_acpi.rtc_gsi,
               (unsigned)g_acpi.spcr_found);
    } else {
        printk("[acpi] RSDP @ 0x%llx without MADT; using defaults\n",
               (unsigned long long)rsdp_phys);
    }
}

const acpi_info_t *acpi_info(void) {
    return &g_acpi;
}

uint16_t acpi_gsi_flags(uint32_t gsi) {
    for (uint32_t i = 0; i < ACPI_MAX_ISA_IRQS; i++) {
        if (g_acpi.irq_gsi[i] == gsi && g_acpi.irq_flags[i] != 0u) {
            return g_acpi.irq_flags[i];
        }
    }
    return 0u;
}
