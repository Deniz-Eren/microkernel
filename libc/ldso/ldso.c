/**
 * @file     ldso.c
 * @brief    Freestanding Ring 3 dynamic linker (/boot/ld.so)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/vfs.h>
#include <sys/vm.h>
#include <time.h>

/* need:: COMP_LIBC_0031 */

#define LSO_PAGE_SIZE 4096u
#define LSO_MAX_LOAD_SEGS 16u
#define LSO_MAX_DEPS 16u
#define LSO_MAX_PATH_DIRS 16u
#define LSO_MAX_DEPTH 8u
#define LSO_MAX_FILE (16u * 1024u * 1024u)
#define LSO_ARENA_BYTES (256u * 1024u)
#define LSO_NAME_MAX 64u

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
#define PT_PHDR 6u
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
#define DT_JMPREL 23
#define DT_INIT_ARRAY 25
#define DT_INIT_ARRAYSZ 27
#define DT_GNU_HASH 0x6FFFFEF5

#define AT_NULL 0UL
#define AT_PHDR 3UL
#define AT_PHENT 4UL
#define AT_PHNUM 5UL
#define AT_BASE 7UL
#define AT_ENTRY 9UL

#define STB_LOCAL 0u
#define STB_GLOBAL 1u
#define STB_WEAK 2u
#define SHN_UNDEF 0u

#define R_X86_64_NONE 0u
#define R_X86_64_64 1u
#define R_X86_64_GLOB_DAT 6u
#define R_X86_64_JUMP_SLOT 7u
#define R_X86_64_RELATIVE 8u

/* Distinctive process exit codes for headless QEMU failure diagnosis. */
#define LSO_EXIT_AUXV 101
#define LSO_EXIT_BIAS 102
#define LSO_EXIT_MAIN 103
#define LSO_EXIT_VFS 104
#define LSO_EXIT_OPEN 105
#define LSO_EXIT_ELF 106
#define LSO_EXIT_MMAP 107
#define LSO_EXIT_SYMBOL 108
#define LSO_EXIT_PROT 109
#define LSO_EXIT_RELOC 110
#define LSO_EXIT_MEMORY 111

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
} ls_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} ls_phdr_t;

/* Ring 3 entry trampoline (libc/ldso/start.S): jump to AT_ENTRY with the
 * original kernel-built stack. This call never returns. */
void ldso_jump(uint64_t entry, long *stack) __attribute__((noreturn));

typedef struct {
    int64_t d_tag;
    uint64_t d_val;
} ls_dyn_t;

typedef struct {
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} ls_sym_t;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
} ls_rela_t;

typedef void (*ls_init_fn_t)(void);

typedef struct ls_obj {
    uint64_t bias;
    const ls_sym_t *symtab;
    const char *strtab;
    uint64_t strsz;
    uint32_t nsyms;
    ls_init_fn_t init_fn;
    const ls_init_fn_t *init_array;
    size_t init_array_count;
    char name[LSO_NAME_MAX];
    struct ls_obj *next;
} ls_obj_t;

static ls_obj_t *g_objs;
static ls_obj_t g_main;
static int g_vfs_conn = -1;
static const char *g_path_dirs[LSO_MAX_PATH_DIRS];
static size_t g_path_dir_len[LSO_MAX_PATH_DIRS];
static size_t g_path_dir_count;

static uint8_t *g_arena;
static size_t g_arena_off;

static void ldso_exit(int code) __attribute__((noreturn));

static void ldso_exit(int code) {
    __sys1(SYS_PROC_EXIT, code);
    for (;;) {
    }
}

static void *ldso_alloc(size_t n) {
    n = (n + 15u) & ~(size_t)15u;
    if (!g_arena) {
        long va = VMMap(0, LSO_ARENA_BYTES, VM_PROT_READ | VM_PROT_WRITE,
                        VM_MAP_PRIVATE | VM_MAP_ANON);
        if (va < 0) {
            ldso_exit(LSO_EXIT_MEMORY);
        }
        g_arena = (uint8_t *)(uintptr_t)va;
    }
    if (n > LSO_ARENA_BYTES || g_arena_off + n > LSO_ARENA_BYTES) {
        ldso_exit(LSO_EXIT_MEMORY);
    }
    void *p = g_arena + g_arena_off;
    g_arena_off += n;
    return p;
}

static size_t ldso_strlen(const char *s) {
    size_t n = 0u;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static int ldso_streq(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static int ldso_strneq(const char *a, const char *b, size_t n) {
    for (size_t i = 0u; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static uintptr_t ldso_page_down(uintptr_t v) {
    return v & ~(uintptr_t)(LSO_PAGE_SIZE - 1u);
}

static uintptr_t ldso_page_up(uintptr_t v) {
    return (v + (uintptr_t)(LSO_PAGE_SIZE - 1u)) &
           ~(uintptr_t)(LSO_PAGE_SIZE - 1u);
}

/*------------------------------------------------------------------------------
 * VFS access over /dev/vfs (nameserver lookup + synchronous IPC).
 *---------------------------------------------------------------------------*/

static long ldso_vfs_rpc(const vfs_req_t *req, vfs_rep_t *rep) {
    if (g_vfs_conn < 0) {
        return -ENXIO;
    }
    long r = SendMsg(g_vfs_conn, req, sizeof(*req), rep, sizeof(*rep));
    if (r < 0) {
        return r;
    }
    return rep->err;
}

static void ldso_vfs_connect(void) {
    for (int tries = 0; tries < 200 && g_vfs_conn < 0; tries++) {
        long ns = OpenConnection(1, 1);
        if (ns >= 0) {
            name_msg_t q = {0};
            q.type = NAME_MSG_QUERY;
            q.path[0] = '/';
            q.path[1] = 'd';
            q.path[2] = 'e';
            q.path[3] = 'v';
            q.path[4] = '/';
            q.path[5] = 'v';
            q.path[6] = 'f';
            q.path[7] = 's';
            q.path[8] = '\0';
            name_reply_t rep = {0};
            long r = SendMsg((int)ns, &q, sizeof(q), &rep, sizeof(rep));
            CloseConnection((int)ns);
            if (r >= 0 && rep.err == 0 && rep.pid > 0 && rep.chanid > 0) {
                long conn =
                    OpenConnection((uint32_t)rep.pid, (uint32_t)rep.chanid);
                if (conn >= 0) {
                    g_vfs_conn = (int)conn;
                    break;
                }
            }
        }
        struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
        __sys4(SYS_CLOCK_NANOSLEEP, CLOCK_MONOTONIC, 0, (long)(uintptr_t)&d, 0);
    }
    if (g_vfs_conn < 0) {
        ldso_exit(LSO_EXIT_VFS);
    }
}

static long ldso_vfs_open(const char *path) {
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_OPEN;
    req.flags = 0u; /* O_RDONLY */
    size_t i = 0u;
    while (i + 1u < sizeof(req.u.path) && path[i] != '\0') {
        req.u.path[i] = path[i];
        i++;
    }
    req.u.path[i] = '\0';
    long r = ldso_vfs_rpc(&req, &rep);
    if (r < 0) {
        return r;
    }
    return (long)rep.handle;
}

static long ldso_vfs_size(uint32_t handle) {
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_FSTAT;
    req.handle = handle;
    long r = ldso_vfs_rpc(&req, &rep);
    if (r < 0) {
        return r;
    }
    return (long)rep.u.st.st_size;
}

static long ldso_vfs_read(uint32_t handle, uint8_t *buf, size_t n) {
    size_t done = 0u;
    while (done < n) {
        vfs_req_t req = {0};
        vfs_rep_t rep = {0};
        req.op = VFS_OP_READ;
        req.handle = handle;
        req.len = (uint32_t)(n - done > VFS_DATA_MAX ? VFS_DATA_MAX : n - done);
        long r = ldso_vfs_rpc(&req, &rep);
        if (r < 0) {
            return r;
        }
        if (rep.len > 0u) {
            for (uint32_t i = 0u; i < rep.len; i++) {
                buf[done + i] = rep.u.data[i];
            }
            done += rep.len;
        }
        if (rep.len < req.len) {
            break;
        }
    }
    return (long)done;
}

static void ldso_vfs_close(uint32_t handle) {
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_CLOSE;
    req.handle = handle;
    (void)ldso_vfs_rpc(&req, &rep);
}

/*------------------------------------------------------------------------------
 * ELF helpers.
 *---------------------------------------------------------------------------*/

static uint32_t ldso_hash_nchain(const uint32_t *ht) {
    return ht[1];
}

static uint32_t ldso_gnu_hash_nsyms(const uint32_t *gh) {
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

static uint32_t ldso_count_syms(uint64_t hash_va, uint64_t gnu_hash_va) {
    if (hash_va != 0u) {
        return ldso_hash_nchain((const uint32_t *)(uintptr_t)hash_va);
    }
    if (gnu_hash_va != 0u) {
        return ldso_gnu_hash_nsyms((const uint32_t *)(uintptr_t)gnu_hash_va);
    }
    return 0u;
}

static void *ldso_lookup_in_obj(const ls_obj_t *obj, const char *name) {
    if (!obj || !obj->symtab || !obj->strtab || obj->nsyms == 0u) {
        return NULL;
    }
    for (uint32_t i = 1u; i < obj->nsyms; i++) {
        const ls_sym_t *sym = &obj->symtab[i];
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
        if (ldso_streq(obj->strtab + sym->st_name, name)) {
            return (void *)(uintptr_t)(obj->bias + sym->st_value);
        }
    }
    return NULL;
}

/* Global scope: the main executable first, then libraries in load order. */
static void *ldso_lookup_global(const char *name) {
    void *p = ldso_lookup_in_obj(&g_main, name);
    if (p) {
        return p;
    }
    for (ls_obj_t *o = g_objs; o; o = o->next) {
        p = ldso_lookup_in_obj(o, name);
        if (p) {
            return p;
        }
    }
    return NULL;
}

/*------------------------------------------------------------------------------
 * Relocation.
 *---------------------------------------------------------------------------*/

static int ldso_apply_rela(ls_obj_t *obj, const ls_rela_t *rela, size_t count) {
    for (size_t i = 0u; i < count; i++) {
        uint32_t type = (uint32_t)(rela[i].r_info & 0xFFFFFFFFu);
        uint32_t sym_idx = (uint32_t)(rela[i].r_info >> 32);
        uint64_t *slot = (uint64_t *)(uintptr_t)(obj->bias + rela[i].r_offset);
        if (type == R_X86_64_NONE) {
            continue;
        }
        if (type == R_X86_64_RELATIVE) {
            *slot = obj->bias + (uint64_t)rela[i].r_addend;
            continue;
        }
        if (type == R_X86_64_64 || type == R_X86_64_GLOB_DAT ||
            type == R_X86_64_JUMP_SLOT) {
            if (!obj->symtab || sym_idx >= obj->nsyms) {
                return -1;
            }
            const ls_sym_t *sym = &obj->symtab[sym_idx];
            uint64_t sval = 0u;
            if (sym->st_shndx != SHN_UNDEF) {
                sval = obj->bias + sym->st_value;
            } else {
                const char *sname = ((size_t)sym->st_name < obj->strsz)
                                        ? (obj->strtab + sym->st_name)
                                        : "";
                void *found = ldso_lookup_global(sname);
                if (!found) {
                    unsigned bind = (unsigned)(sym->st_info >> 4);
                    if (bind == STB_WEAK) {
                        sval = 0u;
                    } else {
                        return -1;
                    }
                } else {
                    sval = (uint64_t)(uintptr_t)found;
                }
            }
            if (type == R_X86_64_64) {
                *slot = sval + (uint64_t)rela[i].r_addend;
            } else {
                *slot = sval;
            }
            continue;
        }
        return -1; /* unsupported relocation type (no TLS in Ring 3 yet) */
    }
    return 0;
}

/*------------------------------------------------------------------------------
 * Object parsing and loading.
 *---------------------------------------------------------------------------*/

struct ls_dyn_info {
    uint64_t strtab_va;
    uint64_t strsz;
    uint64_t symtab_va;
    uint64_t hash_va;
    uint64_t gnu_hash_va;
    uint64_t rela_va;
    uint64_t relasz;
    uint64_t jmprel_va;
    uint64_t pltrelsz;
    uint64_t init_va;
    uint64_t init_array_va;
    uint64_t init_array_sz;
    uint64_t needed_offs[LSO_MAX_DEPS];
    size_t n_needed;
};

static void ldso_parse_dynamic(const ls_dyn_t *dyn, struct ls_dyn_info *di) {
    for (size_t i = 0u; dyn[i].d_tag != DT_NULL; i++) {
        switch (dyn[i].d_tag) {
        case DT_STRTAB:
            di->strtab_va = dyn[i].d_val;
            break;
        case DT_STRSZ:
            di->strsz = dyn[i].d_val;
            break;
        case DT_SYMTAB:
            di->symtab_va = dyn[i].d_val;
            break;
        case DT_HASH:
            di->hash_va = dyn[i].d_val;
            break;
        case DT_GNU_HASH:
            di->gnu_hash_va = dyn[i].d_val;
            break;
        case DT_RELA:
            di->rela_va = dyn[i].d_val;
            break;
        case DT_RELASZ:
            di->relasz = dyn[i].d_val;
            break;
        case DT_JMPREL:
            di->jmprel_va = dyn[i].d_val;
            break;
        case DT_PLTRELSZ:
            di->pltrelsz = dyn[i].d_val;
            break;
        case DT_INIT:
            di->init_va = dyn[i].d_val;
            break;
        case DT_INIT_ARRAY:
            di->init_array_va = dyn[i].d_val;
            break;
        case DT_INIT_ARRAYSZ:
            di->init_array_sz = dyn[i].d_val;
            break;
        case DT_NEEDED:
            if (di->n_needed < LSO_MAX_DEPS) {
                di->needed_offs[di->n_needed++] = dyn[i].d_val;
            }
            break;
        default:
            break;
        }
    }
}

/* Resolve a DT_NEEDED name: a '/'-containing name is a direct path;
 * otherwise search LD_LIBRARY_PATH, then the default directories. */
static void ldso_resolve_path(const char *name, char *out, size_t out_sz) {
    if (out_sz == 0u) {
        ldso_exit(LSO_EXIT_OPEN);
    }
    size_t name_len = ldso_strlen(name);
    int has_slash = 0;
    for (size_t i = 0u; i < name_len; i++) {
        if (name[i] == '/') {
            has_slash = 1;
            break;
        }
    }
    if (has_slash) {
        if (name_len >= out_sz) {
            ldso_exit(LSO_EXIT_OPEN);
        }
        for (size_t i = 0u; i <= name_len; i++) {
            out[i] = name[i];
        }
        return;
    }
    for (size_t d = 0u; d < g_path_dir_count; d++) {
        if (g_path_dir_len[d] + 1u + name_len + 1u > out_sz) {
            continue;
        }
        size_t pos = 0u;
        for (size_t i = 0u; i < g_path_dir_len[d]; i++) {
            out[pos++] = g_path_dirs[d][i];
        }
        out[pos++] = '/';
        for (size_t i = 0u; i <= name_len; i++) {
            out[pos++] = name[i];
        }
        long fd = ldso_vfs_open(out);
        if (fd >= 0) {
            ldso_vfs_close((uint32_t)fd);
            return;
        }
    }
    static const char *const k_dirs[] = {"/boot", "/lib", "/usr/lib"};
    for (size_t d = 0u; d < sizeof(k_dirs) / sizeof(k_dirs[0]); d++) {
        size_t dl = ldso_strlen(k_dirs[d]);
        if (dl + 1u + name_len + 1u > out_sz) {
            continue;
        }
        size_t pos = 0u;
        for (size_t i = 0u; i < dl; i++) {
            out[pos++] = k_dirs[d][i];
        }
        out[pos++] = '/';
        for (size_t i = 0u; i <= name_len; i++) {
            out[pos++] = name[i];
        }
        long fd = ldso_vfs_open(out);
        if (fd >= 0) {
            ldso_vfs_close((uint32_t)fd);
            return;
        }
    }
    ldso_exit(LSO_EXIT_OPEN);
}

static void ldso_init_obj(ls_obj_t *obj) {
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
}

/* Load one DT_NEEDED library over /dev/vfs, then its own dependencies
 * (depth-first, so load order runs dependencies before parents). */
static void ldso_load_lib(const char *name, int depth) {
    if (depth > (int)LSO_MAX_DEPTH) {
        ldso_exit(LSO_EXIT_OPEN);
    }
    for (ls_obj_t *o = g_objs; o; o = o->next) {
        if (ldso_streq(o->name, name)) {
            return;
        }
    }
    if (ldso_streq(g_main.name, name)) {
        return;
    }

    char path[VFS_PATH_MAX];
    ldso_resolve_path(name, path, sizeof(path));
    long fd = ldso_vfs_open(path);
    if (fd < 0) {
        ldso_exit(LSO_EXIT_OPEN);
    }
    long fsz = ldso_vfs_size((uint32_t)fd);
    if (fsz < (long)sizeof(ls_ehdr_t) || fsz > (long)LSO_MAX_FILE) {
        ldso_vfs_close((uint32_t)fd);
        ldso_exit(LSO_EXIT_ELF);
    }
    long map_len = ldso_page_up((uintptr_t)fsz);
    long fva = VMMap(0, (size_t)map_len, VM_PROT_READ | VM_PROT_WRITE,
                     VM_MAP_PRIVATE | VM_MAP_ANON);
    if (fva < 0) {
        ldso_vfs_close((uint32_t)fd);
        ldso_exit(LSO_EXIT_MMAP);
    }
    uint8_t *fbuf = (uint8_t *)(uintptr_t)fva;
    if (ldso_vfs_read((uint32_t)fd, fbuf, (size_t)fsz) != fsz) {
        ldso_vfs_close((uint32_t)fd);
        VMUnmap((uintptr_t)fva, (size_t)map_len);
        ldso_exit(LSO_EXIT_ELF);
    }
    ldso_vfs_close((uint32_t)fd);

    const ls_ehdr_t *eh = (const ls_ehdr_t *)fbuf;
    if (eh->e_ident[EI_MAG0] != 0x7Fu || eh->e_ident[EI_MAG1] != 'E' ||
        eh->e_ident[EI_MAG2] != 'L' || eh->e_ident[EI_MAG3] != 'F' ||
        eh->e_ident[EI_CLASS] != ELFCLASS64 ||
        eh->e_ident[EI_DATA] != ELFDATA2LSB || eh->e_type != ET_DYN ||
        eh->e_machine != EM_X86_64 || eh->e_phnum == 0u ||
        eh->e_phentsize != sizeof(ls_phdr_t) ||
        eh->e_phoff + (uint64_t)eh->e_phnum * sizeof(ls_phdr_t) >
            (uint64_t)fsz) {
        VMUnmap((uintptr_t)fva, (size_t)map_len);
        ldso_exit(LSO_EXIT_ELF);
    }

    const ls_phdr_t *ph = (const ls_phdr_t *)(fbuf + eh->e_phoff);
    uintptr_t min_va = (uintptr_t)-1;
    uintptr_t max_va = 0u;
    uint64_t dyn_off = 0u;
    size_t nload = 0u;
    struct {
        uintptr_t start;
        size_t len;
        unsigned flags;
    } segs[LSO_MAX_LOAD_SEGS];

    for (uint16_t i = 0u; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_DYNAMIC) {
            dyn_off = ph[i].p_offset;
            continue;
        }
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0u) {
            continue;
        }
        if (nload >= LSO_MAX_LOAD_SEGS || ph[i].p_filesz > ph[i].p_memsz ||
            ph[i].p_offset + ph[i].p_filesz > (uint64_t)fsz ||
            ((ph[i].p_flags & PF_W) && (ph[i].p_flags & PF_X))) {
            VMUnmap((uintptr_t)fva, (size_t)map_len);
            ldso_exit(LSO_EXIT_ELF);
        }
        uintptr_t s = ldso_page_down((uintptr_t)ph[i].p_vaddr);
        uintptr_t e = ldso_page_up((uintptr_t)(ph[i].p_vaddr + ph[i].p_memsz));
        if (s < min_va) {
            min_va = s;
        }
        if (e > max_va) {
            max_va = e;
        }
        segs[nload].start = s;
        segs[nload].len = (size_t)(e - s);
        segs[nload].flags = ph[i].p_flags;
        nload++;
    }
    if (nload == 0u || max_va <= min_va || dyn_off == 0u) {
        VMUnmap((uintptr_t)fva, (size_t)map_len);
        ldso_exit(LSO_EXIT_ELF);
    }

    size_t map_size = (size_t)(max_va - min_va);
    long va = VMMap(0, map_size, VM_PROT_READ | VM_PROT_WRITE,
                    VM_MAP_PRIVATE | VM_MAP_ANON);
    if (va < 0) {
        VMUnmap((uintptr_t)fva, (size_t)map_len);
        ldso_exit(LSO_EXIT_MMAP);
    }
    uint64_t bias = (uint64_t)va - min_va;
    for (uint16_t i = 0u; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_LOAD && ph[i].p_filesz > 0u) {
            uint8_t *dst = (uint8_t *)(uintptr_t)(bias + ph[i].p_vaddr);
            for (uint64_t b = 0u; b < ph[i].p_filesz; b++) {
                dst[b] = fbuf[ph[i].p_offset + b];
            }
        }
    }
    VMUnmap((uintptr_t)fva, (size_t)map_len);

    ls_obj_t *obj = ldso_alloc(sizeof(*obj));
    obj->bias = bias;
    size_t nl = 0u;
    while (nl + 1u < sizeof(obj->name) && name[nl] != '\0') {
        obj->name[nl] = name[nl];
        nl++;
    }
    obj->name[nl] = '\0';

    const ls_dyn_t *dyn = (const ls_dyn_t *)(uintptr_t)(bias + dyn_off);
    struct ls_dyn_info di = {0};
    ldso_parse_dynamic(dyn, &di);
    if (di.strtab_va == 0u || di.symtab_va == 0u) {
        ldso_exit(LSO_EXIT_ELF);
    }
    obj->strtab = (const char *)(uintptr_t)(bias + di.strtab_va);
    obj->strsz = di.strsz;
    obj->symtab = (const ls_sym_t *)(uintptr_t)(bias + di.symtab_va);
    obj->nsyms = ldso_count_syms(di.hash_va ? bias + di.hash_va : 0,
                                 di.gnu_hash_va ? bias + di.gnu_hash_va : 0);
    obj->init_fn =
        di.init_va ? (ls_init_fn_t)(uintptr_t)(bias + di.init_va) : NULL;
    obj->init_array =
        di.init_array_va
            ? (const ls_init_fn_t *)(uintptr_t)(bias + di.init_array_va)
            : NULL;
    obj->init_array_count = di.init_array_sz / sizeof(ls_init_fn_t);

    obj->next = g_objs;
    g_objs = obj;

    for (size_t i = 0u; i < di.n_needed; i++) {
        if ((size_t)di.needed_offs[i] >= di.strsz) {
            ldso_exit(LSO_EXIT_ELF);
        }
        ldso_load_lib(obj->strtab + di.needed_offs[i], depth + 1);
    }

    if (di.rela_va != 0u && di.relasz > 0u &&
        ldso_apply_rela(obj, (const ls_rela_t *)(uintptr_t)(bias + di.rela_va),
                        (size_t)(di.relasz / sizeof(ls_rela_t))) != 0) {
        ldso_exit(LSO_EXIT_RELOC);
    }
    if (di.jmprel_va != 0u && di.pltrelsz > 0u &&
        ldso_apply_rela(obj,
                        (const ls_rela_t *)(uintptr_t)(bias + di.jmprel_va),
                        (size_t)(di.pltrelsz / sizeof(ls_rela_t))) != 0) {
        ldso_exit(LSO_EXIT_RELOC);
    }

    /* Enforce final W^X segment protections before any code runs. */
    for (size_t i = 0u; i < nload; i++) {
        unsigned prot = 0u;
        if (segs[i].flags & PF_R) {
            prot |= VM_PROT_READ;
        }
        if (segs[i].flags & PF_W) {
            prot |= VM_PROT_WRITE;
        }
        if (segs[i].flags & PF_X) {
            prot |= VM_PROT_EXEC;
        }
        if (VMProtect((uintptr_t)(bias + segs[i].start), segs[i].len, prot) !=
            0) {
            ldso_exit(LSO_EXIT_PROT);
        }
    }
}

/* Locate the main executable load bias from AT_PHDR: prefer the PT_PHDR
 * vaddr, else scan the first mapped page for the ELF identity. */
static uint64_t ldso_main_bias(uint64_t at_phdr, const ls_phdr_t *ph,
                               uint64_t phnum) {
    for (uint64_t i = 0u; i < phnum; i++) {
        if (ph[i].p_type == PT_PHDR) {
            return at_phdr - ph[i].p_vaddr;
        }
    }
    uint64_t page = at_phdr & ~0xFFFULL;
    const ls_ehdr_t *eh = (const ls_ehdr_t *)(uintptr_t)page;
    if (eh->e_ident[EI_MAG0] == 0x7Fu && eh->e_ident[EI_MAG1] == 'E' &&
        eh->e_ident[EI_MAG2] == 'L' && eh->e_ident[EI_MAG3] == 'F') {
        return at_phdr - eh->e_phoff;
    }
    ldso_exit(LSO_EXIT_BIAS);
}

void ldso_main(long *stack) __attribute__((noreturn));

void ldso_main(long *stack) {
    long argc = stack[0];
    char **argv = (char **)(stack + 1);
    char **envp = argv + argc + 1;
    char **e = envp;
    while (*e) {
        e++;
    }
    const unsigned long *auxv = (const unsigned long *)(e + 1);

    unsigned long at_base = 0;
    unsigned long at_entry = 0;
    unsigned long at_phdr = 0;
    unsigned long at_phnum = 0;
    unsigned long at_phent = 0;
    for (size_t i = 0u; auxv[i] != AT_NULL; i += 2u) {
        switch (auxv[i]) {
        case AT_BASE:
            at_base = auxv[i + 1u];
            break;
        case AT_ENTRY:
            at_entry = auxv[i + 1u];
            break;
        case AT_PHDR:
            at_phdr = auxv[i + 1u];
            break;
        case AT_PHENT:
            at_phent = auxv[i + 1u];
            break;
        case AT_PHNUM:
            at_phnum = auxv[i + 1u];
            break;
        default:
            break;
        }
    }
    if (at_base == 0u || at_entry == 0u || at_phdr == 0u ||
        at_phent != sizeof(ls_phdr_t) || at_phnum == 0u) {
        ldso_exit(LSO_EXIT_AUXV);
    }

    /* Self-relocate from the interpreter's own program headers. */
    const ls_ehdr_t *self_eh = (const ls_ehdr_t *)(uintptr_t)at_base;
    const ls_phdr_t *self_ph =
        (const ls_phdr_t *)(uintptr_t)(at_base + self_eh->e_phoff);
    uint64_t self_dyn_va = 0u;
    for (uint64_t i = 0u; i < self_eh->e_phnum; i++) {
        if (self_ph[i].p_type == PT_DYNAMIC) {
            self_dyn_va = self_ph[i].p_vaddr;
            break;
        }
    }
    if (self_dyn_va == 0u) {
        ldso_exit(LSO_EXIT_AUXV);
    }
    ls_obj_t self = {0};
    self.bias = at_base;
    const ls_dyn_t *self_dyn =
        (const ls_dyn_t *)(uintptr_t)(at_base + self_dyn_va);
    struct ls_dyn_info sdi = {0};
    ldso_parse_dynamic(self_dyn, &sdi);
    if (sdi.strtab_va != 0u && sdi.symtab_va != 0u) {
        self.strtab = (const char *)(uintptr_t)(at_base + sdi.strtab_va);
        self.strsz = sdi.strsz;
        self.symtab = (const ls_sym_t *)(uintptr_t)(at_base + sdi.symtab_va);
        self.nsyms =
            ldso_count_syms(sdi.hash_va ? at_base + sdi.hash_va : 0,
                            sdi.gnu_hash_va ? at_base + sdi.gnu_hash_va : 0);
        if (ldso_apply_rela(
                &self, (const ls_rela_t *)(uintptr_t)(at_base + sdi.rela_va),
                (size_t)(sdi.relasz / sizeof(ls_rela_t))) != 0) {
            ldso_exit(LSO_EXIT_RELOC);
        }
        if (ldso_apply_rela(
                &self, (const ls_rela_t *)(uintptr_t)(at_base + sdi.jmprel_va),
                (size_t)(sdi.pltrelsz / sizeof(ls_rela_t))) != 0) {
            ldso_exit(LSO_EXIT_RELOC);
        }
    }

    /* Split LD_LIBRARY_PATH from the process environment. */
    for (char **v = envp; *v; v++) {
        if (ldso_strneq(*v, "LD_LIBRARY_PATH=", 16u)) {
            const char *p = *v + 16u;
            while (*p != '\0' && g_path_dir_count < LSO_MAX_PATH_DIRS) {
                if (*p == ':') {
                    p++;
                    continue;
                }
                const char *colon = p;
                while (*colon != '\0' && *colon != ':') {
                    colon++;
                }
                g_path_dirs[g_path_dir_count] = p;
                g_path_dir_len[g_path_dir_count] = (size_t)(colon - p);
                g_path_dir_count++;
                p = colon;
            }
            break;
        }
    }

    /* Parse the main executable and load its DT_NEEDED libraries. */
    ldso_vfs_connect();
    const ls_phdr_t *main_ph = (const ls_phdr_t *)(uintptr_t)at_phdr;
    g_main.bias = ldso_main_bias(at_phdr, main_ph, at_phnum);
    uint64_t main_dyn_va = 0u;
    for (uint64_t i = 0u; i < at_phnum; i++) {
        if (main_ph[i].p_type == PT_DYNAMIC) {
            main_dyn_va = main_ph[i].p_vaddr;
            break;
        }
    }
    if (main_dyn_va == 0u) {
        ldso_exit(LSO_EXIT_MAIN);
    }
    const ls_dyn_t *main_dyn =
        (const ls_dyn_t *)(uintptr_t)(g_main.bias + main_dyn_va);
    struct ls_dyn_info mdi = {0};
    ldso_parse_dynamic(main_dyn, &mdi);
    if (mdi.strtab_va == 0u || mdi.symtab_va == 0u) {
        ldso_exit(LSO_EXIT_MAIN);
    }
    g_main.strtab = (const char *)(uintptr_t)(g_main.bias + mdi.strtab_va);
    g_main.strsz = mdi.strsz;
    g_main.symtab = (const ls_sym_t *)(uintptr_t)(g_main.bias + mdi.symtab_va);
    g_main.nsyms =
        ldso_count_syms(mdi.hash_va ? g_main.bias + mdi.hash_va : 0,
                        mdi.gnu_hash_va ? g_main.bias + mdi.gnu_hash_va : 0);

    for (size_t i = 0u; i < mdi.n_needed; i++) {
        if ((size_t)mdi.needed_offs[i] >= mdi.strsz) {
            ldso_exit(LSO_EXIT_MAIN);
        }
        ldso_load_lib(g_main.strtab + mdi.needed_offs[i], 0);
    }

    if (mdi.rela_va != 0u && mdi.relasz > 0u &&
        ldso_apply_rela(
            &g_main, (const ls_rela_t *)(uintptr_t)(g_main.bias + mdi.rela_va),
            (size_t)(mdi.relasz / sizeof(ls_rela_t))) != 0) {
        ldso_exit(LSO_EXIT_RELOC);
    }
    if (mdi.jmprel_va != 0u && mdi.pltrelsz > 0u &&
        ldso_apply_rela(
            &g_main,
            (const ls_rela_t *)(uintptr_t)(g_main.bias + mdi.jmprel_va),
            (size_t)(mdi.pltrelsz / sizeof(ls_rela_t))) != 0) {
        ldso_exit(LSO_EXIT_RELOC);
    }

    /* Run constructors: libraries in dependency order, then the main
     * executable's DT_INIT and DT_INIT_ARRAY. */
    for (ls_obj_t *o = g_objs; o; o = o->next) {
        ldso_init_obj(o);
    }
    if (mdi.init_va != 0u) {
        ((ls_init_fn_t)(uintptr_t)(g_main.bias + mdi.init_va))();
    }
    if (mdi.init_array_va != 0u) {
        const ls_init_fn_t *arr =
            (const ls_init_fn_t *)(uintptr_t)(g_main.bias + mdi.init_array_va);
        for (size_t i = 0u; i < mdi.init_array_sz / sizeof(ls_init_fn_t); i++) {
            if (arr[i]) {
                arr[i]();
            }
        }
    }

    ldso_jump(at_entry, stack);
}
