/**
 * @file     dlfcn.c
 * @brief    Ring 3 ELF64 ET_DYN loader and POSIX.1-2024 <dlfcn.h> APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "tls_internal.h"

/* need:: COMP_LIBC_0016 */

#define DL_PAGE_SIZE 4096u
#define DL_MAX_LOAD_SEGS 16u
#define DL_MAX_DEPS 16u

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define ELFCLASS64 2u
#define ELFDATA2LSB 1u
#define ET_DYN 3u
#define EM_X86_64 62u

#define PT_LOAD 1u
#define PT_DYNAMIC 2u
#define PF_X 1u
#define PF_W 2u
#define PF_R 4u

#define DT_NULL 0
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_STRSZ 10
#define DT_SYMENT 11
#define DT_INIT 12
#define DT_FINI 13
#define DT_JMPREL 23
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_GNU_HASH 0x6FFFFEF5

#define STB_LOCAL 0u
#define STB_GLOBAL 1u
#define STB_WEAK 2u
#define SHN_UNDEF 0u

#define R_X86_64_NONE 0u
#define R_X86_64_64 1u
#define R_X86_64_GLOB_DAT 6u
#define R_X86_64_JUMP_SLOT 7u
#define R_X86_64_RELATIVE 8u

typedef struct {
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
} dl_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} dl_phdr_t;

typedef struct {
    int64_t d_tag;
    uint64_t d_val;
} dl_dyn_t;

typedef struct {
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} dl_sym_t;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
} dl_rela_t;

typedef void (*dl_init_fn_t)(void);

struct dl_seg {
    uintptr_t start;
    size_t len;
    int prot;
};

struct dl_object {
    char *path;
    uint8_t *map_base;
    size_t map_size;
    uintptr_t load_bias;
    int refcount;
    int global;
    const dl_sym_t *symtab;
    uint32_t nsyms;
    const char *strtab;
    size_t strsz;
    dl_init_fn_t init_fn;
    dl_init_fn_t fini_fn;
    const dl_init_fn_t *init_array;
    size_t init_array_count;
    const dl_init_fn_t *fini_array;
    size_t fini_array_count;
    struct dl_object *deps[DL_MAX_DEPS];
    size_t ndeps;
    struct dl_object *next;
};

struct host_sym {
    const char *name;
    void *addr;
};

static const struct host_sym k_host_syms[] = {
    {"calloc", (void *)(uintptr_t)calloc},
    {"free", (void *)(uintptr_t)free},
    {"malloc", (void *)(uintptr_t)malloc},
    {"memcmp", (void *)(uintptr_t)memcmp},
    {"memcpy", (void *)(uintptr_t)memcpy},
    {"memmove", (void *)(uintptr_t)memmove},
    {"memset", (void *)(uintptr_t)memset},
    {"printf", (void *)(uintptr_t)printf},
    {"puts", (void *)(uintptr_t)puts},
    {"realloc", (void *)(uintptr_t)realloc},
    {"snprintf", (void *)(uintptr_t)snprintf},
    {"strcmp", (void *)(uintptr_t)strcmp},
    {"strdup", (void *)(uintptr_t)strdup},
    {"strlen", (void *)(uintptr_t)strlen},
    {"strncmp", (void *)(uintptr_t)strncmp},
};

static pthread_mutex_t g_dl_lock = PTHREAD_MUTEX_INITIALIZER;
static struct dl_object *g_dl_head = NULL;
static int g_main_sentinel = 0;
static int g_fallback_err_pending = 0;
static char g_fallback_err_buf[128] = {0};

static void dl_set_error(const char *fmt, ...) {
    char tmp[128];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    __pthread_tls_t *tls = __libc_tls_self();
    if (tls) {
        (void)strlcpy(tls->dlerror_buf, tmp, sizeof(tls->dlerror_buf));
        tls->dlerror_pending = 1;
    } else {
        (void)strlcpy(g_fallback_err_buf, tmp, sizeof(g_fallback_err_buf));
        g_fallback_err_pending = 1;
    }
}

static uintptr_t page_down(uintptr_t v) {
    return v & ~(uintptr_t)(DL_PAGE_SIZE - 1u);
}

static uintptr_t page_up(uintptr_t v) {
    return (v + (uintptr_t)(DL_PAGE_SIZE - 1u)) &
           ~(uintptr_t)(DL_PAGE_SIZE - 1u);
}

static void *lookup_host_sym(const char *name) {
    for (size_t i = 0u; i < sizeof(k_host_syms) / sizeof(k_host_syms[0]); i++) {
        if (strcmp(k_host_syms[i].name, name) == 0) {
            return k_host_syms[i].addr;
        }
    }
    return NULL;
}

static uint32_t count_gnu_hash_syms(const uint32_t *gh) {
    uint32_t nbuckets = gh[0];
    uint32_t symoffset = gh[1];
    uint32_t bloom_size = gh[2];
    const uint64_t *bloom = (const uint64_t *)(gh + 4);
    const uint32_t *buckets = (const uint32_t *)(bloom + bloom_size);
    const uint32_t *chains = buckets + nbuckets;

    uint32_t max_sym = symoffset;
    for (uint32_t i = 0u; i < nbuckets; i++) {
        if (buckets[i] >= max_sym) {
            max_sym = buckets[i];
        }
    }
    if (max_sym < symoffset) {
        return symoffset;
    }
    while ((chains[max_sym - symoffset] & 1u) == 0u) {
        max_sym++;
    }
    return max_sym + 1u;
}

static void *lookup_in_obj(const struct dl_object *obj, const char *name) {
    if (!obj || !obj->symtab || !obj->strtab) {
        return NULL;
    }
    for (uint32_t i = 1u; i < obj->nsyms; i++) {
        const dl_sym_t *sym = &obj->symtab[i];
        if (sym->st_shndx == SHN_UNDEF) {
            continue;
        }
        unsigned bind = (unsigned)(sym->st_info >> 4);
        if (bind != STB_GLOBAL && bind != STB_WEAK) {
            continue;
        }
        if ((size_t)sym->st_name >= obj->strsz) {
            continue;
        }
        if (strcmp(obj->strtab + sym->st_name, name) == 0) {
            return (void *)(obj->load_bias + sym->st_value);
        }
    }
    return NULL;
}

static void *lookup_global_locked(const struct dl_object *self_obj,
                                  const char *name) {
    for (struct dl_object *o = g_dl_head; o; o = o->next) {
        if (o->global) {
            void *p = lookup_in_obj(o, name);
            if (p) {
                return p;
            }
        }
    }
    if (self_obj) {
        void *p = lookup_in_obj(self_obj, name);
        if (p) {
            return p;
        }
        for (size_t i = 0u; i < self_obj->ndeps; i++) {
            p = lookup_in_obj(self_obj->deps[i], name);
            if (p) {
                return p;
            }
        }
    }
    return lookup_host_sym(name);
}

static int resolve_file_path(const char *file, char *out, size_t out_sz) {
    struct stat st;
    if (strchr(file, '/') != NULL) {
        if (stat(file, &st) == 0 && S_ISREG(st.st_mode)) {
            (void)strlcpy(out, file, out_sz);
            return 0;
        }
        return -1;
    }
    const char *ldp = getenv("LD_LIBRARY_PATH");
    if (ldp && ldp[0] != '\0') {
        const char *p = ldp;
        while (*p != '\0') {
            const char *colon = strchr(p, ':');
            size_t len = colon ? (size_t)(colon - p) : strlen(p);
            if (len > 0u && len + strlen(file) + 2u < out_sz) {
                memcpy(out, p, len);
                out[len] = '/';
                (void)strlcpy(out + len + 1u, file, out_sz - len - 1u);
                if (stat(out, &st) == 0 && S_ISREG(st.st_mode)) {
                    return 0;
                }
            }
            if (!colon) {
                break;
            }
            p = colon + 1;
        }
    }
    static const char *const k_dirs[] = {"/boot", "/lib", "/usr/lib", "."};
    for (size_t i = 0u; i < sizeof(k_dirs) / sizeof(k_dirs[0]); i++) {
        (void)snprintf(out, out_sz, "%s/%s", k_dirs[i], file);
        if (stat(out, &st) == 0 && S_ISREG(st.st_mode)) {
            return 0;
        }
    }
    return -1;
}

static int apply_rela_table(struct dl_object *obj, const dl_rela_t *rela,
                            size_t count) {
    for (size_t i = 0u; i < count; i++) {
        uint32_t type = (uint32_t)(rela[i].r_info & 0xFFFFFFFFu);
        uint32_t sym_idx = (uint32_t)(rela[i].r_info >> 32);
        uintptr_t target = obj->load_bias + rela[i].r_offset;
        if (target < (uintptr_t)obj->map_base ||
            target + sizeof(uint64_t) >
                (uintptr_t)obj->map_base + obj->map_size) {
            dl_set_error("dlopen: relocation target out of range");
            return -1;
        }
        uint64_t *slot = (uint64_t *)target;
        if (type == R_X86_64_NONE) {
            continue;
        }
        if (type == R_X86_64_RELATIVE) {
            *slot = (uint64_t)((int64_t)obj->load_bias + rela[i].r_addend);
            continue;
        }
        if (type == R_X86_64_64 || type == R_X86_64_GLOB_DAT ||
            type == R_X86_64_JUMP_SLOT) {
            if (!obj->symtab || sym_idx >= obj->nsyms) {
                dl_set_error("dlopen: invalid relocation symbol index");
                return -1;
            }
            const dl_sym_t *sym = &obj->symtab[sym_idx];
            uint64_t sval = 0u;
            if (sym->st_shndx != SHN_UNDEF) {
                sval = (uint64_t)(obj->load_bias + sym->st_value);
            } else {
                const char *sname = ((size_t)sym->st_name < obj->strsz)
                                        ? (obj->strtab + sym->st_name)
                                        : "";
                void *found = lookup_global_locked(obj, sname);
                if (!found) {
                    unsigned bind = (unsigned)(sym->st_info >> 4);
                    if (bind == STB_WEAK) {
                        sval = 0u;
                    } else {
                        dl_set_error("dlopen: undefined symbol '%s'", sname);
                        return -1;
                    }
                } else {
                    sval = (uint64_t)(uintptr_t)found;
                }
            }
            if (type == R_X86_64_64) {
                *slot = (uint64_t)((int64_t)sval + rela[i].r_addend);
            } else {
                *slot = sval;
            }
            continue;
        }
        dl_set_error("dlopen: unsupported relocation type %u", type);
        return -1;
    }
    return 0;
}

static struct dl_object *dlopen_locked(const char *file, int mode);
static int dlclose_locked(struct dl_object *obj);

static void destroy_obj_locked(struct dl_object *obj) {
    if (!obj) {
        return;
    }
    for (size_t i = 0u; i < obj->ndeps; i++) {
        (void)dlclose_locked(obj->deps[i]);
    }
    if (obj->map_base && obj->map_size > 0u) {
        (void)munmap(obj->map_base, obj->map_size);
    }
    free(obj->path);
    free(obj);
}

static struct dl_object *dlopen_locked(const char *file, int mode) {
    char path[PATH_MAX];
    if (resolve_file_path(file, path, sizeof(path)) != 0) {
        dl_set_error("dlopen: cannot find '%s'", file);
        return NULL;
    }

    for (struct dl_object *o = g_dl_head; o; o = o->next) {
        if (strcmp(o->path, path) == 0) {
            o->refcount++;
            if (mode & RTLD_GLOBAL) {
                o->global = 1;
            }
            return o;
        }
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        dl_set_error("dlopen: open '%s' failed", path);
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < (off_t)sizeof(dl_ehdr_t) ||
        st.st_size > (off_t)(8 * 1024 * 1024)) {
        (void)close(fd);
        dl_set_error("dlopen: invalid ELF size for '%s'", path);
        return NULL;
    }
    size_t fsz = (size_t)st.st_size;
    uint8_t *fbuf = (uint8_t *)malloc(fsz);
    if (!fbuf) {
        (void)close(fd);
        dl_set_error("dlopen: out of memory");
        return NULL;
    }
    size_t got = 0u;
    while (got < fsz) {
        ssize_t r = read(fd, fbuf + got, fsz - got);
        if (r <= 0) {
            free(fbuf);
            (void)close(fd);
            dl_set_error("dlopen: read '%s' failed", path);
            return NULL;
        }
        got += (size_t)r;
    }
    (void)close(fd);

    const dl_ehdr_t *eh = (const dl_ehdr_t *)fbuf;
    if (eh->e_ident[EI_MAG0] != 0x7Fu || eh->e_ident[EI_MAG1] != 'E' ||
        eh->e_ident[EI_MAG2] != 'L' || eh->e_ident[EI_MAG3] != 'F' ||
        eh->e_ident[EI_CLASS] != ELFCLASS64 ||
        eh->e_ident[EI_DATA] != ELFDATA2LSB || eh->e_type != ET_DYN ||
        eh->e_machine != EM_X86_64 || eh->e_phnum == 0u ||
        eh->e_phentsize != sizeof(dl_phdr_t) ||
        eh->e_phoff + (uint64_t)eh->e_phnum * sizeof(dl_phdr_t) > fsz) {
        free(fbuf);
        dl_set_error("dlopen: '%s' is not a valid x86-64 ET_DYN ELF", path);
        return NULL;
    }

    const dl_phdr_t *ph = (const dl_phdr_t *)(fbuf + eh->e_phoff);
    uintptr_t min_va = (uintptr_t)-1;
    uintptr_t max_va = 0u;
    uint64_t dyn_va = 0u;
    size_t nload = 0u;
    struct dl_seg segs[DL_MAX_LOAD_SEGS];

    for (uint16_t i = 0u; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_DYNAMIC) {
            dyn_va = ph[i].p_vaddr;
            continue;
        }
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0u) {
            continue;
        }
        if (nload >= DL_MAX_LOAD_SEGS || ph[i].p_filesz > ph[i].p_memsz ||
            ph[i].p_offset + ph[i].p_filesz > fsz) {
            free(fbuf);
            dl_set_error("dlopen: invalid PT_LOAD segment in '%s'", path);
            return NULL;
        }
        if ((ph[i].p_flags & PF_W) && (ph[i].p_flags & PF_X)) {
            free(fbuf);
            dl_set_error("dlopen: W^X violation in '%s'", path);
            return NULL;
        }
        uintptr_t s = page_down((uintptr_t)ph[i].p_vaddr);
        uintptr_t e = page_up((uintptr_t)(ph[i].p_vaddr + ph[i].p_memsz));
        if (s < min_va) {
            min_va = s;
        }
        if (e > max_va) {
            max_va = e;
        }
        int prot = 0;
        if (ph[i].p_flags & PF_R) {
            prot |= PROT_READ;
        }
        if (ph[i].p_flags & PF_W) {
            prot |= PROT_WRITE;
        }
        if (ph[i].p_flags & PF_X) {
            prot |= PROT_EXEC;
        }
        segs[nload].start = s;
        segs[nload].len = (size_t)(e - s);
        segs[nload].prot = prot;
        nload++;
    }
    if (nload == 0u || max_va <= min_va) {
        free(fbuf);
        dl_set_error("dlopen: no loadable segments in '%s'", path);
        return NULL;
    }

    size_t map_size = (size_t)(max_va - min_va);
    void *mem = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        free(fbuf);
        dl_set_error("dlopen: mmap failed for '%s'", path);
        return NULL;
    }

    uintptr_t bias = (uintptr_t)mem - min_va;
    for (uint16_t i = 0u; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_LOAD && ph[i].p_filesz > 0u) {
            memcpy((void *)(bias + (uintptr_t)ph[i].p_vaddr),
                   fbuf + ph[i].p_offset, (size_t)ph[i].p_filesz);
        }
    }
    free(fbuf);

    struct dl_object *obj = (struct dl_object *)calloc(1u, sizeof(*obj));
    if (!obj) {
        (void)munmap(mem, map_size);
        dl_set_error("dlopen: out of memory");
        return NULL;
    }
    obj->path = strdup(path);
    if (!obj->path) {
        (void)munmap(mem, map_size);
        free(obj);
        dl_set_error("dlopen: out of memory");
        return NULL;
    }
    obj->map_base = (uint8_t *)mem;
    obj->map_size = map_size;
    obj->load_bias = bias;
    obj->refcount = 1;
    obj->global = (mode & RTLD_GLOBAL) ? 1 : 0;

    uint64_t rela_va = 0u;
    uint64_t relasz = 0u;
    uint64_t jmprel_va = 0u;
    uint64_t pltrelsz = 0u;
    uint64_t hash_va = 0u;
    uint64_t gnu_hash_va = 0u;
    uint64_t needed_offs[DL_MAX_DEPS];
    size_t n_needed = 0u;

    if (dyn_va != 0u) {
        const dl_dyn_t *dyn = (const dl_dyn_t *)(bias + (uintptr_t)dyn_va);
        for (size_t i = 0u; dyn[i].d_tag != DT_NULL; i++) {
            switch (dyn[i].d_tag) {
            case DT_STRTAB:
                obj->strtab = (const char *)(bias + (uintptr_t)dyn[i].d_val);
                break;
            case DT_STRSZ:
                obj->strsz = (size_t)dyn[i].d_val;
                break;
            case DT_SYMTAB:
                obj->symtab =
                    (const dl_sym_t *)(bias + (uintptr_t)dyn[i].d_val);
                break;
            case DT_HASH:
                hash_va = dyn[i].d_val;
                break;
            case DT_GNU_HASH:
                gnu_hash_va = dyn[i].d_val;
                break;
            case DT_RELA:
                rela_va = dyn[i].d_val;
                break;
            case DT_RELASZ:
                relasz = dyn[i].d_val;
                break;
            case DT_JMPREL:
                jmprel_va = dyn[i].d_val;
                break;
            case DT_PLTRELSZ:
                pltrelsz = dyn[i].d_val;
                break;
            case DT_INIT:
                obj->init_fn =
                    (dl_init_fn_t)(uintptr_t)(bias + (uintptr_t)dyn[i].d_val);
                break;
            case DT_FINI:
                obj->fini_fn =
                    (dl_init_fn_t)(uintptr_t)(bias + (uintptr_t)dyn[i].d_val);
                break;
            case DT_INIT_ARRAY:
                obj->init_array =
                    (const dl_init_fn_t *)(bias + (uintptr_t)dyn[i].d_val);
                break;
            case DT_INIT_ARRAYSZ:
                obj->init_array_count =
                    (size_t)dyn[i].d_val / sizeof(dl_init_fn_t);
                break;
            case DT_FINI_ARRAY:
                obj->fini_array =
                    (const dl_init_fn_t *)(bias + (uintptr_t)dyn[i].d_val);
                break;
            case DT_FINI_ARRAYSZ:
                obj->fini_array_count =
                    (size_t)dyn[i].d_val / sizeof(dl_init_fn_t);
                break;
            case DT_NEEDED:
                if (n_needed < DL_MAX_DEPS) {
                    needed_offs[n_needed++] = dyn[i].d_val;
                }
                break;
            default:
                break;
            }
        }
    }

    if (hash_va != 0u) {
        const uint32_t *ht = (const uint32_t *)(bias + (uintptr_t)hash_va);
        obj->nsyms = ht[1];
    } else if (gnu_hash_va != 0u) {
        const uint32_t *gh = (const uint32_t *)(bias + (uintptr_t)gnu_hash_va);
        obj->nsyms = count_gnu_hash_syms(gh);
    } else if (obj->symtab && obj->strtab &&
               (uintptr_t)obj->strtab > (uintptr_t)obj->symtab) {
        obj->nsyms =
            (uint32_t)(((uintptr_t)obj->strtab - (uintptr_t)obj->symtab) /
                       sizeof(dl_sym_t));
    }

    for (size_t i = 0u; i < n_needed; i++) {
        if (!obj->strtab || (size_t)needed_offs[i] >= obj->strsz) {
            destroy_obj_locked(obj);
            dl_set_error("dlopen: invalid DT_NEEDED offset");
            return NULL;
        }
        const char *dep_name = obj->strtab + needed_offs[i];
        struct dl_object *dep = dlopen_locked(dep_name, mode);
        if (!dep) {
            destroy_obj_locked(obj);
            return NULL;
        }
        obj->deps[obj->ndeps++] = dep;
    }

    if (rela_va != 0u && relasz > 0u) {
        const dl_rela_t *r = (const dl_rela_t *)(bias + (uintptr_t)rela_va);
        if (apply_rela_table(obj, r, (size_t)(relasz / sizeof(dl_rela_t))) !=
            0) {
            destroy_obj_locked(obj);
            return NULL;
        }
    }
    if (jmprel_va != 0u && pltrelsz > 0u) {
        const dl_rela_t *r = (const dl_rela_t *)(bias + (uintptr_t)jmprel_va);
        if (apply_rela_table(obj, r, (size_t)(pltrelsz / sizeof(dl_rela_t))) !=
            0) {
            destroy_obj_locked(obj);
            return NULL;
        }
    }

    /* Enforce final W^X segment protections before executing constructors. */
    for (size_t i = 0u; i < nload; i++) {
        if (mprotect((void *)(bias + segs[i].start), segs[i].len,
                     segs[i].prot) != 0) {
            destroy_obj_locked(obj);
            dl_set_error("dlopen: mprotect W^X failed for '%s'", path);
            return NULL;
        }
    }

    obj->next = g_dl_head;
    g_dl_head = obj;

    if (obj->init_fn) {
        obj->init_fn();
    }
    if (obj->init_array) {
        for (size_t i = 0u; i < obj->init_array_count; i++) {
            if (obj->init_array[i]) {
                obj->init_array[i]();
            }
        }
    }
    return obj;
}

static int dlclose_locked(struct dl_object *obj) {
    struct dl_object **pp = &g_dl_head;
    while (*pp && *pp != obj) {
        pp = &(*pp)->next;
    }
    if (!*pp) {
        dl_set_error("dlclose: invalid handle");
        return -1;
    }
    if (--obj->refcount > 0) {
        return 0;
    }
    *pp = obj->next;
    if (obj->fini_array) {
        for (size_t i = obj->fini_array_count; i > 0u; i--) {
            if (obj->fini_array[i - 1u]) {
                obj->fini_array[i - 1u]();
            }
        }
    }
    if (obj->fini_fn) {
        obj->fini_fn();
    }
    destroy_obj_locked(obj);
    return 0;
}

void *dlopen(const char *file, int mode) {
    if ((mode & (RTLD_LAZY | RTLD_NOW)) == 0) {
        dl_set_error("dlopen: mode must specify RTLD_LAZY or RTLD_NOW");
        return NULL;
    }
    if (!file) {
        return &g_main_sentinel;
    }
    (void)pthread_mutex_lock(&g_dl_lock);
    struct dl_object *obj = dlopen_locked(file, mode);
    (void)pthread_mutex_unlock(&g_dl_lock);
    return (void *)obj;
}

void *dlsym(void *restrict handle, const char *restrict name) {
    if (!name || name[0] == '\0') {
        dl_set_error("dlsym: empty symbol name");
        return NULL;
    }
    void *ret_addr = __builtin_return_address(0);
    (void)pthread_mutex_lock(&g_dl_lock);
    void *sym = NULL;
    if (handle == RTLD_DEFAULT || handle == &g_main_sentinel) {
        sym = lookup_global_locked(NULL, name);
    } else if (handle == RTLD_NEXT) {
        uintptr_t ra = (uintptr_t)ret_addr;
        int passed_caller = 0;
        for (struct dl_object *o = g_dl_head; o; o = o->next) {
            if (!passed_caller) {
                uintptr_t lo = (uintptr_t)o->map_base;
                if (ra >= lo && ra < lo + o->map_size) {
                    passed_caller = 1;
                }
                continue;
            }
            if (o->global) {
                sym = lookup_in_obj(o, name);
                if (sym) {
                    break;
                }
            }
        }
        if (!sym) {
            sym = lookup_host_sym(name);
        }
    } else {
        struct dl_object *found = NULL;
        for (struct dl_object *o = g_dl_head; o; o = o->next) {
            if ((void *)o == handle) {
                found = o;
                break;
            }
        }
        if (!found) {
            (void)pthread_mutex_unlock(&g_dl_lock);
            dl_set_error("dlsym: invalid handle");
            return NULL;
        }
        sym = lookup_in_obj(found, name);
        for (size_t i = 0u; !sym && i < found->ndeps; i++) {
            sym = lookup_in_obj(found->deps[i], name);
        }
    }
    (void)pthread_mutex_unlock(&g_dl_lock);
    if (!sym) {
        dl_set_error("dlsym: symbol '%s' not found", name);
    }
    return sym;
}

int dlclose(void *handle) {
    if (!handle) {
        dl_set_error("dlclose: NULL handle");
        return -1;
    }
    if (handle == &g_main_sentinel) {
        return 0;
    }
    (void)pthread_mutex_lock(&g_dl_lock);
    int rc = dlclose_locked((struct dl_object *)handle);
    (void)pthread_mutex_unlock(&g_dl_lock);
    return rc;
}

char *dlerror(void) {
    __pthread_tls_t *tls = __libc_tls_self();
    if (tls) {
        if (!tls->dlerror_pending) {
            return NULL;
        }
        tls->dlerror_pending = 0;
        return tls->dlerror_buf;
    }
    if (!g_fallback_err_pending) {
        return NULL;
    }
    g_fallback_err_pending = 0;
    return g_fallback_err_buf;
}

int dladdr(const void *restrict addr, Dl_info_t *restrict dlip) {
    if (!addr || !dlip) {
        return 0;
    }
    uintptr_t target = (uintptr_t)addr;
    (void)pthread_mutex_lock(&g_dl_lock);
    struct dl_object *owner = NULL;
    for (struct dl_object *o = g_dl_head; o; o = o->next) {
        uintptr_t lo = (uintptr_t)o->map_base;
        if (target >= lo && target < lo + o->map_size) {
            owner = o;
            break;
        }
    }
    if (!owner) {
        (void)pthread_mutex_unlock(&g_dl_lock);
        return 0;
    }

    dlip->dli_fname = owner->path;
    dlip->dli_fbase = owner->map_base;
    dlip->dli_sname = NULL;
    dlip->dli_saddr = NULL;

    uintptr_t best_addr = 0u;
    for (uint32_t i = 1u; i < owner->nsyms; i++) {
        const dl_sym_t *sym = &owner->symtab[i];
        if (sym->st_shndx == SHN_UNDEF ||
            (size_t)sym->st_name >= owner->strsz) {
            continue;
        }
        unsigned bind = (unsigned)(sym->st_info >> 4);
        if (bind != STB_GLOBAL && bind != STB_WEAK) {
            continue;
        }
        uintptr_t saddr = owner->load_bias + sym->st_value;
        if (saddr > target) {
            continue;
        }
        if (sym->st_size > 0u && target >= saddr + sym->st_size) {
            continue;
        }
        if (!dlip->dli_sname || saddr >= best_addr) {
            best_addr = saddr;
            dlip->dli_sname = owner->strtab + sym->st_name;
            dlip->dli_saddr = (void *)saddr;
        }
    }

    (void)pthread_mutex_unlock(&g_dl_lock);
    return 1;
}
