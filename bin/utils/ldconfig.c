/**
 * @file     ldconfig.c
 * @brief    Native /usr/sbin/ldconfig that maintains DT_SONAME symlinks
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: COMP_LIBC_0036 */

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LDCONF_PATH "/etc/ld.so.conf"
#define LDCONF_MAX_DIRS 16u
#define LDCONF_NAME_MAX 128u

/* Minimal ELF 64-bit little-endian definitions for reading DT_SONAME. */
#define ELF_MAGIC0 0x7f
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define PT_LOAD 1u
#define PT_DYNAMIC 2u
#define DT_NULL 0
#define DT_STRTAB 5
#define DT_STRSZ 10
#define DT_SONAME 14

struct elf64_ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct elf64_dyn {
    int64_t d_tag;
    uint64_t d_val;
};

static uint8_t *read_file(const char *path, size_t *size_out) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size_out = (size_t)sz;
    return buf;
}

static int vaddr_to_off(const struct elf64_phdr *ph, uint16_t phnum,
                        uint64_t va, size_t size, uint64_t *off_out) {
    for (uint16_t i = 0u; i < phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_filesz == 0u) {
            continue;
        }
        if (va >= ph[i].p_vaddr && va < ph[i].p_vaddr + ph[i].p_filesz) {
            uint64_t off = ph[i].p_offset + (va - ph[i].p_vaddr);
            if (off < (uint64_t)size) {
                *off_out = off;
                return 1;
            }
        }
    }
    return 0;
}

/* Return the DT_SONAME string of an in-memory ELF image, or NULL when absent.
 * The result is copied into a static buffer so it stays valid after free(). */
static const char *elf_soname(const uint8_t *buf, size_t size) {
    static char soname_buf[LDCONF_NAME_MAX];
    if (size < sizeof(struct elf64_ehdr)) {
        return NULL;
    }
    const struct elf64_ehdr *eh = (const struct elf64_ehdr *)buf;
    if (eh->e_ident[0] != ELF_MAGIC0 || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F' ||
        eh->e_ident[4] != ELFCLASS64 || eh->e_ident[5] != ELFDATA2LSB) {
        return NULL;
    }
    if (eh->e_phnum == 0u || eh->e_phentsize != sizeof(struct elf64_phdr) ||
        eh->e_phoff + (uint64_t)eh->e_phnum * sizeof(struct elf64_phdr) >
            size) {
        return NULL;
    }
    const struct elf64_phdr *ph =
        (const struct elf64_phdr *)(buf + eh->e_phoff);
    uint64_t dyn_off = 0u;
    uint64_t dyn_sz = 0u;
    for (uint16_t i = 0u; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_DYNAMIC) {
            dyn_off = ph[i].p_offset;
            dyn_sz = ph[i].p_filesz;
            break;
        }
    }
    if (dyn_off == 0u || dyn_sz < sizeof(struct elf64_dyn) ||
        dyn_off + dyn_sz > (uint64_t)size) {
        return NULL;
    }
    const struct elf64_dyn *dyn = (const struct elf64_dyn *)(buf + dyn_off);
    size_t max_dyn = (size_t)(dyn_sz / sizeof(struct elf64_dyn));
    uint64_t strtab_va = 0u;
    uint64_t strsz = 0u;
    int has_soname = 0;
    uint64_t soname_off = 0u;
    for (size_t i = 0u; i < max_dyn && dyn[i].d_tag != DT_NULL; i++) {
        if (dyn[i].d_tag == DT_STRTAB) {
            strtab_va = dyn[i].d_val;
        } else if (dyn[i].d_tag == DT_STRSZ) {
            strsz = dyn[i].d_val;
        } else if (dyn[i].d_tag == DT_SONAME) {
            has_soname = 1;
            soname_off = dyn[i].d_val;
        }
    }
    if (!has_soname || strtab_va == 0u) {
        return NULL;
    }
    uint64_t strtab_off = 0u;
    if (!vaddr_to_off(ph, eh->e_phnum, strtab_va, size, &strtab_off)) {
        return NULL;
    }
    if (strsz != 0u && soname_off >= strsz) {
        return NULL;
    }
    if (strtab_off + soname_off >= (uint64_t)size) {
        return NULL;
    }
    const char *soname = (const char *)(buf + strtab_off + soname_off);
    size_t avail = (size_t)((uint64_t)size - (strtab_off + soname_off));
    size_t n = 0u;
    while (n < avail && soname[n] != '\0' && n + 1u < sizeof(soname_buf)) {
        soname_buf[n] = soname[n];
        n++;
    }
    if (n == 0u || (n < avail && soname[n] != '\0')) {
        return NULL;
    }
    soname_buf[n] = '\0';
    return soname_buf;
}

/* Return 1 when name looks like lib*.so or lib*.so.<ver>. */
static int is_shared_lib_name(const char *name) {
    if (strncmp(name, "lib", 3u) != 0) {
        return 0;
    }
    const char *p = strstr(name + 3, ".so");
    if (!p) {
        return 0;
    }
    return (p[3] == '\0' || p[3] == '.');
}

/* Scan one library directory and create dir/<soname> -> <name> symlinks for
 * each shared object whose DT_SONAME differs from its file name. */
static int process_dir(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) {
        return 0;
    }
    int created = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        if (!is_shared_lib_name(name)) {
            continue;
        }
        char path[256];
        int n = snprintf(path, sizeof(path), "%s/%s", dir, name);
        if (n < 0 || (size_t)n >= sizeof(path)) {
            continue;
        }
        struct stat st;
        if (lstat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }
        size_t size = 0u;
        uint8_t *buf = read_file(path, &size);
        if (!buf) {
            continue;
        }
        const char *soname = elf_soname(buf, size);
        free(buf);
        if (!soname || strcmp(soname, name) == 0) {
            continue;
        }
        char link[256];
        int m = snprintf(link, sizeof(link), "%s/%s", dir, soname);
        if (m < 0 || (size_t)m >= sizeof(link)) {
            continue;
        }
        if (lstat(link, &st) == 0) {
            continue;
        }
        if (symlink(name, link) != 0) {
            continue;
        }
        printf("ldconfig: created %s -> %s\n", link, name);
        created++;
    }
    closedir(d);
    return created;
}

static void add_dir(char dirs[][LDCONF_NAME_MAX], size_t *ndirs,
                    const char *path) {
    size_t len = strlen(path);
    if (len == 0u || len >= LDCONF_NAME_MAX || *ndirs >= LDCONF_MAX_DIRS) {
        return;
    }
    for (size_t i = 0u; i < *ndirs; i++) {
        if (strcmp(dirs[i], path) == 0) {
            return;
        }
    }
    memcpy(dirs[*ndirs], path, len + 1u);
    (*ndirs)++;
}

int main(int argc, char **argv) {
    char dirs[LDCONF_MAX_DIRS][LDCONF_NAME_MAX];
    size_t ndirs = 0u;
    int cli_dirs = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            continue; /* accept standard build-script flags (-n, -v, -N, -X) */
        }
        add_dir(dirs, &ndirs, argv[i]);
        cli_dirs = 1;
    }

    if (!cli_dirs) {
        FILE *f = fopen(LDCONF_PATH, "r");
        if (f) {
            char line[LDCONF_NAME_MAX];
            while (fgets(line, sizeof(line), f)) {
                char *p = line;
                while (*p == ' ' || *p == '\t') {
                    p++;
                }
                if (*p == '#' || *p == '\0' || *p == '\n') {
                    continue;
                }
                size_t len = strlen(p);
                while (len > 0u &&
                       (p[len - 1u] == '\n' || p[len - 1u] == '\r' ||
                        p[len - 1u] == ' ' || p[len - 1u] == '\t')) {
                    p[--len] = '\0';
                }
                add_dir(dirs, &ndirs, p);
            }
            fclose(f);
        }
        static const char *defaults[] = {"/boot", "/lib", "/usr/lib"};
        for (size_t i = 0u; i < sizeof(defaults) / sizeof(defaults[0]); i++) {
            add_dir(dirs, &ndirs, defaults[i]);
        }
    }

    int created = 0;
    for (size_t i = 0u; i < ndirs; i++) {
        created += process_dir(dirs[i]);
    }
    printf("ldconfig: scanned %zu directories, created %d symlinks\n", ndirs,
           created);
    return 0;
}
