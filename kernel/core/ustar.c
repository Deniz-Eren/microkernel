// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#include <kernel/ustar.h>
#include <kernel/vmm.h>
#include <string.h>

/* need:: COMP_BOOT_0002 */

/* Minimal POSIX ustar reader over the Multiboot2 bootfs module (already
 * resident in physical memory, accessed through the HHDM). */
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
} ustar_hdr_t;

static uint64_t octal(const char *s, size_t n) {
    uint64_t v = 0;
    for (size_t i = 0; i < n && s[i]; i++) {
        if (s[i] < '0' || s[i] > '7') {
            break;
        }
        v = v * 8 + (uint64_t)(s[i] - '0');
    }
    return v;
}

int ustar_open(ustar_iter_t *it, uint64_t phys, uint64_t size) {
    it->phys = phys;
    it->end = phys + size;
    it->cursor = phys;
    return 0;
}

int ustar_next(ustar_iter_t *it) {
    while (it->cursor + 512 <= it->end) {
        const ustar_hdr_t *h = (const ustar_hdr_t *)phys_to_virt(it->cursor);
        if (h->name[0] == '\0') {
            return 0; /* end of archive */
        }
        if (memcmp(h->magic, "ustar", 5) != 0) {
            it->cursor += 512; /* skip garbage */
            continue;
        }
        it->name = h->name;
        it->data_phys = it->cursor + 512;
        it->size = octal(h->size, sizeof(h->size));
        it->cursor = it->data_phys + ((it->size + 511) & ~511ULL);
        return 1;
    }
    return 0;
}

const void *ustar_data(const ustar_iter_t *it) {
    return phys_to_virt(it->data_phys);
}
