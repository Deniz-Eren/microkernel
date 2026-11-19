#include <kernel/elf.h>
#include <kernel/pmm.h>
#include <kernel/printk.h>
#include <kernel/vmm.h>
#include <kernel/string.h>

typedef struct {
    uint8_t e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} elf64_ehdr_t;

typedef struct {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} elf64_phdr_t;

#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4

int elf_load_user(uint64_t pml4_phys, const void *image, size_t size,
                  uint64_t *out_entry, uint64_t *out_brk) {
    if (size < sizeof(elf64_ehdr_t)) {
        return -1;
    }
    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)image;
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' || eh->e_ident[2] != 'L' ||
        eh->e_ident[3] != 'F' || eh->e_ident[4] != 2 /* ELFCLASS64 */ ||
        eh->e_ident[5] != 1 /* little endian */ || eh->e_machine != 62 /* x86-64 */) {
        return -1;
    }
    if (eh->e_phentsize < sizeof(elf64_phdr_t) ||
        (uint64_t)eh->e_phoff + (uint64_t)eh->e_phnum * sizeof(elf64_phdr_t) > size) {
        return -1;
    }

    const elf64_phdr_t *ph =
        (const elf64_phdr_t *)((const uint8_t *)image + eh->e_phoff);
    uint64_t max_end = 0;

    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) {
            continue;
        }
        if ((uint64_t)ph[i].p_offset + ph[i].p_filesz > size) {
            return -1;
        }
        if (ph[i].p_vaddr < 0x400000ULL || ph[i].p_vaddr + ph[i].p_memsz > 0x80000000ULL) {
            return -1; /* keep user images inside the private user region */
        }

        uint64_t flags = VMM_USER;
        if (ph[i].p_flags & PF_X) {
            flags |= 0; /* executable: no NX */
        } else {
            flags |= VMM_NX;
        }
        if (ph[i].p_flags & PF_W) {
            flags |= VMM_WRITE;
        }

        uint64_t start = ph[i].p_vaddr & ~0xFFFULL;
        uint64_t end = (ph[i].p_vaddr + ph[i].p_memsz + 0xFFF) & ~0xFFFULL;

        for (uint64_t va = start; va < end; va += 4096) {
            /* Segments may share a page (e.g. text tail + rodata start).
             * Reuse the existing frame and union the permissions. */
            uint64_t old = vmm_get_pte_to(pml4_phys, va);
            uint64_t frame;
            if (old & VMM_PRESENT) {
                frame = old & 0x000FFFFFFFFFF000ULL;
                if (!(old & VMM_NX)) {
                    flags &= ~VMM_NX; /* page already executable */
                }
                if (old & VMM_WRITE) {
                    flags |= VMM_WRITE;
                }
            } else {
                frame = pmm_alloc_frame();
                if (!frame) {
                    return -1;
                }
                memset(phys_to_virt(frame), 0, 4096);
            }

            /* File-backed bytes for this page: VA range
             * [max(va, p_vaddr), min(va+4096, p_vaddr+p_filesz)), copied to
             * the same intra-page offset. */
            uint64_t lo = va > ph[i].p_vaddr ? va : ph[i].p_vaddr;
            uint64_t hi = va + 4096;
            uint64_t fend = ph[i].p_vaddr + ph[i].p_filesz;
            if (hi > fend) {
                hi = fend;
            }
            if (lo < hi) {
                uint8_t *kv = (uint8_t *)phys_to_virt(frame);
                memcpy(kv + (lo & 0xFFF),
                       (const uint8_t *)image + ph[i].p_offset + (lo - ph[i].p_vaddr),
                       hi - lo);
            }
            if (vmm_map_page_to(pml4_phys, va, frame, flags) != 0) {
                return -1;
            }
        }
        if (ph[i].p_vaddr + ph[i].p_memsz > max_end) {
            max_end = ph[i].p_vaddr + ph[i].p_memsz;
        }
        printk("[elf] load seg va=%llx filesz=%llu memsz=%llu flags=%c%c%c\n",
               (unsigned long long)ph[i].p_vaddr,
               (unsigned long long)ph[i].p_filesz,
               (unsigned long long)ph[i].p_memsz,
               (ph[i].p_flags & PF_R) ? 'r' : '-',
               (ph[i].p_flags & PF_W) ? 'w' : '-',
               (ph[i].p_flags & PF_X) ? 'x' : '-');
    }

    if (out_entry) {
        *out_entry = eh->e_entry;
    }
    if (out_brk) {
        *out_brk = (max_end + 0xFFF) & ~0xFFFULL;
    }
    return 0;
}
