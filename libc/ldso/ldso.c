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
#include <dlfcn.h>
#include <sys/errno.h>
#include <sys/fd.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/tty.h>
#include <sys/vfs.h>
#include <sys/vm.h>
#include <time.h>

/* need:: COMP_LIBC_0031 COMP_LIBC_0032 COMP_LIBC_0035 */

#define LSO_PAGE_SIZE 4096u
#define LSO_MAX_LOAD_SEGS 16u
#define LSO_MAX_DEPS 16u
#define LSO_MAX_PATH_DIRS 16u
#define LSO_MAX_DEPTH 8u
#define LSO_MAX_FILE (16u * 1024u * 1024u)
#define LSO_ARENA_BYTES (256u * 1024u)
#define LSO_NAME_MAX 64u
#define LSO_ERR_MAX 128u

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
#define DT_FINI 13
#define DT_JMPREL 23
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
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
    ls_init_fn_t fini_fn;
    const ls_init_fn_t *fini_array;
    size_t fini_array_count;
    char name[LSO_NAME_MAX];
    char path[LSO_NAME_MAX];
    uintptr_t map_va;
    size_t map_size;
    unsigned int refs;    /* dlopen/dlclose reference count */
    unsigned int runtime; /* 1 = loaded by ldso_dlopen, dlclose may unmap */
    unsigned int ctors_run;
    unsigned int finis_run;
    struct ls_obj *deps[LSO_MAX_DEPS];
    size_t ndeps;
    struct ls_obj *next;
} ls_obj_t;

static ls_obj_t *g_objs;
static ls_obj_t g_main;
static ls_obj_t g_self; /* the interpreter itself, searched last */
static int g_vfs_conn = -1;
static uint32_t g_tty_srv_pid;
static uint32_t g_tty_srv_chan;
static int g_tty_conn = -1;
static const char *g_path_dirs[LSO_MAX_PATH_DIRS];
static size_t g_path_dir_len[LSO_MAX_PATH_DIRS];
static size_t g_path_dir_count;

static uint8_t *g_arena;
static size_t g_arena_off;

/* Last-failure diagnostic printed over SYS_DEBUG_LOG before the process
 * exits, so headless QEMU serial logs identify the failing load step. */
static char g_diag[160];

static size_t ldso_diag_len(void) {
    size_t n = 0u;
    while (g_diag[n] != '\0') {
        n++;
    }
    return n;
}

static void ldso_diag_set(const char *msg, const char *detail) {
    size_t pos = 0u;
    const char *parts[2] = {msg, detail};
    for (size_t i = 0u; i < 2u; i++) {
        const char *p = parts[i];
        if (!p) {
            continue;
        }
        while (*p != '\0' && pos + 1u < sizeof(g_diag)) {
            g_diag[pos++] = *p++;
        }
    }
    g_diag[pos] = '\0';
}

static void ldso_diag_append(const char *detail) {
    if (!detail) {
        return;
    }
    size_t pos = ldso_diag_len();
    while (*detail != '\0' && pos + 1u < sizeof(g_diag)) {
        g_diag[pos++] = *detail++;
    }
    g_diag[pos] = '\0';
}

static int ldso_fd_write(int fd, const char *buf, size_t n);

static void ldso_exit(int code) __attribute__((noreturn));

static void ldso_exit(int code) {
    if (g_diag[0] != '\0') {
        size_t len = ldso_diag_len();
        if (ldso_fd_write(2, g_diag, len) < 0) {
            (void)__sys2(SYS_DEBUG_LOG, (long)(uintptr_t)g_diag, (long)len);
        } else if (g_diag[len - 1u] != '\n') {
            (void)ldso_fd_write(2, "\n", 1u);
        }
    }
    __sys1(SYS_PROC_EXIT, code);
    for (;;) {
    }
}

/* Concatenate error message parts into the diagnostic buffer and mirror them
 * into the caller buffer (used by the ldso_dlopen/ldso_dlsym/ ldso_dlclose
 * delegation ABI so libc keeps thread-local dlerror state;
 * see ADR_0059). Startup failures keep the diagnostic for ldso_exit. */
static void ldso_err_parts(char *err, size_t errsz, const char *const *parts,
                           size_t nparts) {
    size_t pos = 0u;
    for (size_t i = 0u; i < nparts; i++) {
        const char *p = parts[i];
        while (p != 0 && *p != '\0' && pos + 1u < sizeof(g_diag)) {
            g_diag[pos++] = *p++;
        }
    }
    g_diag[pos] = '\0';
    if (!err || errsz == 0u) {
        return;
    }
    pos = 0u;
    while (g_diag[pos] != '\0' && pos + 1u < errsz) {
        err[pos] = g_diag[pos];
        pos++;
    }
    err[pos] = '\0';
}

static void ldso_err_msg(char *err, size_t errsz, const char *msg) {
    ldso_err_parts(err, errsz, &msg, 1u);
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

/* Copy name into a fixed obj field with truncation. */
static void ldso_copy_name(char *dst, size_t dst_sz, const char *src) {
    size_t i = 0u;
    while (i + 1u < dst_sz && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
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

/* Query sysproc once for /dev/tty so ldso_fd_write can distinguish PTY server
 * handles from pipe server handles without opening an unused tty connection. */
static void ldso_tty_resolve(void) {
    if (g_tty_srv_pid != 0u && g_tty_srv_chan != 0u) {
        return;
    }
    long ns = OpenConnection(1, 1);
    if (ns < 0) {
        return;
    }
    name_msg_t q = {0};
    q.type = NAME_MSG_QUERY;
    ldso_copy_name(q.path, sizeof(q.path), "/dev/tty");
    name_reply_t rep = {0};
    long r = SendMsg((int)ns, &q, sizeof(q), &rep, sizeof(rep));
    CloseConnection((int)ns);
    if (r >= 0 && rep.err == 0 && rep.pid > 0 && rep.chanid > 0) {
        g_tty_srv_pid = (uint32_t)rep.pid;
        g_tty_srv_chan = (uint32_t)rep.chanid;
    }
}

static int ldso_tty_connect(void) {
    if (g_tty_conn >= 0) {
        return g_tty_conn;
    }
    ldso_tty_resolve();
    if (g_tty_srv_pid == 0u || g_tty_srv_chan == 0u) {
        return -1;
    }
    long conn = OpenConnection(g_tty_srv_pid, g_tty_srv_chan);
    if (conn < 0) {
        return -1;
    }
    g_tty_conn = (int)conn;
    return g_tty_conn;
}

static int ldso_tty_write(uint32_t handle, uint32_t flags, const char *buf,
                          size_t n) {
    int conn = ldso_tty_connect();
    if (conn < 0) {
        return -1;
    }
    size_t sent = 0u;
    while (sent < n) {
        size_t chunk = n - sent > TTY_DATA_MAX ? TTY_DATA_MAX : n - sent;
        tty_req_t req = {0};
        tty_rep_t rep = {0};
        req.op = TTY_OP_WRITE;
        req.handle = handle;
        req.flags = flags;
        req.len = (uint32_t)chunk;
        for (size_t i = 0u; i < chunk; i++) {
            req.u.data[i] = (uint8_t)buf[sent + i];
        }
        long r = SendMsg(conn, &req, sizeof(req), &rep, sizeof(rep));
        if (r < 0 || rep.err < 0) {
            return -1;
        }
        if (rep.len == 0u) {
            break;
        }
        sent += rep.len;
    }
    return 0;
}

/* Write bytes to an open file descriptor generically across console/TTY,
 * PTY, pipe, and VFS file descriptors (mirroring libc __libc_fd_write). */
static int ldso_fd_write(int fd, const char *buf, size_t n) {
    if (fd < 0 || !buf) {
        return -1;
    }
    if (n == 0u) {
        return 0;
    }
    struct __k_fd_entry e = {0};
    if (__sys5(SYS_FD_OP, FD_OP_GET, fd, (long)(uintptr_t)&e, 0, 0) < 0) {
        return -1;
    }
    if (e.kind == FD_KIND_CONSOLE || e.kind == FD_KIND_TTY) {
        return ldso_tty_write(0u, e.status_flags, buf, n);
    }
    if (e.kind == FD_KIND_VFS) {
        if (g_vfs_conn < 0) {
            return -1;
        }
        size_t sent = 0u;
        while (sent < n) {
            size_t chunk = n - sent > VFS_DATA_MAX ? VFS_DATA_MAX : n - sent;
            vfs_req_t req = {0};
            vfs_rep_t rep = {0};
            req.op = VFS_OP_WRITE;
            req.handle = e.srv_handle;
            req.flags = e.status_flags;
            req.len = (uint32_t)chunk;
            for (size_t i = 0u; i < chunk; i++) {
                req.u.data[i] = (uint8_t)buf[sent + i];
            }
            if (ldso_vfs_rpc(&req, &rep) < 0) {
                return -1;
            }
            if (rep.len == 0u) {
                break;
            }
            sent += rep.len;
        }
        return 0;
    }
    if (e.kind != FD_KIND_SERVER) {
        return -1;
    }
    ldso_tty_resolve();
    if (g_tty_srv_pid != 0u && e.srv_pid == g_tty_srv_pid &&
        e.srv_chanid == g_tty_srv_chan) {
        return ldso_tty_write(e.srv_handle, e.status_flags, buf, n);
    }
    long conn = OpenConnection(e.srv_pid, e.srv_chanid);
    if (conn < 0) {
        return -1;
    }
    size_t sent = 0u;
    int rc = 0;
    while (sent < n) {
        size_t chunk = n - sent > PIPE_CHUNK_MAX ? PIPE_CHUNK_MAX : n - sent;
        pipe_req_t req = {0};
        pipe_rep_t rep = {0};
        req.op = PIPE_OP_WRITE;
        req.handle = e.srv_handle;
        req.len = (uint32_t)chunk;
        req.flags = e.status_flags;
        for (size_t i = 0u; i < chunk; i++) {
            req.data[i] = (uint8_t)buf[sent + i];
        }
        long r = SendMsg((int)conn, &req, sizeof(req), &rep, sizeof(rep));
        if (r < 0 || rep.err < 0) {
            rc = -1;
            break;
        }
        if (rep.len == 0u) {
            break;
        }
        sent += rep.len;
    }
    CloseConnection((int)conn);
    return rc;
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

/* Global scope: the main executable first, then libraries in load order, then
 * the interpreter itself (so libc.so weak references to the
 * ldso_dlopen family resolve; see ADR_0059). */
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
    return ldso_lookup_in_obj(&g_self, name);
}

/* POSIX dlsym handle scope: the object plus its dependency subtree. */
static void *ldso_lookup_with_deps(const ls_obj_t *obj, const char *name,
                                   int depth) {
    if (!obj || depth > (int)LSO_MAX_DEPTH) {
        return NULL;
    }
    void *p = ldso_lookup_in_obj(obj, name);
    if (p) {
        return p;
    }
    for (size_t i = 0u; i < obj->ndeps; i++) {
        p = ldso_lookup_with_deps(obj->deps[i], name, depth + 1);
        if (p) {
            return p;
        }
    }
    return NULL;
}

static int ldso_ends_with(const char *s, const char *suffix) {
    size_t sl = ldso_strlen(s);
    size_t pl = ldso_strlen(suffix);
    if (sl < pl) {
        return 0;
    }
    return ldso_strneq(s + sl - pl, suffix, pl);
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
                ldso_diag_set("ld.so: bad relocation symbol index in '",
                              obj->name);
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
                        ldso_diag_set("ld.so: undefined symbol '", sname);
                        ldso_diag_append("' in '");
                        ldso_diag_append(obj->name);
                        ldso_diag_append("'");
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
        ldso_diag_set("ld.so: unsupported relocation type in '", obj->name);
        return -1; /* no TLS relocations in Ring 3 yet */
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
    uint64_t fini_va;
    uint64_t fini_array_va;
    uint64_t fini_array_sz;
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
        case DT_FINI:
            di->fini_va = dyn[i].d_val;
            break;
        case DT_INIT_ARRAY:
            di->init_array_va = dyn[i].d_val;
            break;
        case DT_INIT_ARRAYSZ:
            di->init_array_sz = dyn[i].d_val;
            break;
        case DT_FINI_ARRAY:
            di->fini_array_va = dyn[i].d_val;
            break;
        case DT_FINI_ARRAYSZ:
            di->fini_array_sz = dyn[i].d_val;
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

/* Resolve a DT_NEEDED name: a '/'-containing name is a direct path; otherwise
 * search LD_LIBRARY_PATH, then the default directories. The function returns 0
 * and fills out with the existing path, or -1 when
 * nothing matches. */
static int ldso_resolve_path(const char *name, char *out, size_t out_sz) {
    if (out_sz == 0u) {
        return -1;
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
            return -1;
        }
        for (size_t i = 0u; i <= name_len; i++) {
            out[i] = name[i];
        }
        return 0;
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
            return 0;
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
            return 0;
        }
    }
    return -1;
}

/* Run constructors for obj after its whole dependency subtree is relocated:
 * dependencies first, then the object itself (once per object). */
static void ldso_run_ctors(ls_obj_t *obj) {
    if (!obj || obj->ctors_run) {
        return;
    }
    obj->ctors_run = 1u;
    for (size_t i = 0u; i < obj->ndeps; i++) {
        ldso_run_ctors(obj->deps[i]);
    }
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

/* Run destructors in the reverse order of the constructors: the object
 * itself first, then its dependencies (once per object). */
static void ldso_run_finis(ls_obj_t *obj) {
    if (!obj || obj->finis_run) {
        return;
    }
    obj->finis_run = 1u;
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
    for (size_t i = 0u; i < obj->ndeps; i++) {
        ldso_run_finis(obj->deps[i]);
    }
}

static int g_trace_only;

/* Unlink obj from g_objs when a load fails partway through. */
static void ldso_unlink_obj(const ls_obj_t *obj) {
    ls_obj_t **pp = &g_objs;
    while (*pp) {
        if (*pp == obj) {
            *pp = obj->next;
            return;
        }
        pp = &(*pp)->next;
    }
}

/* Load one ELF shared object over /dev/vfs, then its own DT_NEEDED dependencies
 * (depth-first). The function returns the new object, or NULL with a message in
 * err when loading fails. Startup loads pass
 * runtime = 0; ldso_dlopen passes runtime = 1 so dlclose can unmap. */
static ls_obj_t *ldso_load_lib(const char *name, int depth, int runtime,
                               char *err, size_t errsz) {
    if (depth > (int)LSO_MAX_DEPTH) {
        const char *parts[] = {"dlopen: dependency depth exceeded for '", name,
                               "'"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
    }
    for (ls_obj_t *o = g_objs; o; o = o->next) {
        if (ldso_streq(o->name, name)) {
            return o;
        }
    }
    if (ldso_streq(g_main.name, name)) {
        return &g_main;
    }

    char path[VFS_PATH_MAX];
    if (ldso_resolve_path(name, path, sizeof(path)) != 0) {
        const char *parts[] = {"dlopen: cannot find '", name, "'"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
    }
    long fd = ldso_vfs_open(path);
    if (fd < 0) {
        const char *parts[] = {"dlopen: open '", path, "' failed"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
    }
    long fsz = ldso_vfs_size((uint32_t)fd);
    if (fsz < (long)sizeof(ls_ehdr_t) || fsz > (long)LSO_MAX_FILE) {
        ldso_vfs_close((uint32_t)fd);
        const char *parts[] = {"dlopen: invalid ELF size for '", path, "'"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
    }
    long map_len = ldso_page_up((uintptr_t)fsz);
    long fva = VMMap(0, (size_t)map_len, VM_PROT_READ | VM_PROT_WRITE,
                     VM_MAP_PRIVATE | VM_MAP_ANON);
    if (fva < 0) {
        ldso_vfs_close((uint32_t)fd);
        const char *parts[] = {"dlopen: mapping '", path, "' failed"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
    }
    uint8_t *fbuf = (uint8_t *)(uintptr_t)fva;
    if (ldso_vfs_read((uint32_t)fd, fbuf, (size_t)fsz) != fsz) {
        ldso_vfs_close((uint32_t)fd);
        VMUnmap((uintptr_t)fva, (size_t)map_len);
        const char *parts[] = {"dlopen: read '", path, "' failed"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
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
        const char *parts[] = {"dlopen: '", path,
                               "' is not a valid x86-64 ET_DYN ELF"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
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
            const char *parts[] = {"dlopen: invalid PT_LOAD segments in '",
                                   path, "'"};
            ldso_err_parts(err, errsz, parts, 3u);
            return NULL;
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
        const char *parts[] = {"dlopen: no loadable segments in '", path, "'"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
    }

    size_t map_size = (size_t)(max_va - min_va);
    long va = VMMap(0, map_size, VM_PROT_READ | VM_PROT_WRITE,
                    VM_MAP_PRIVATE | VM_MAP_ANON);
    if (va < 0) {
        VMUnmap((uintptr_t)fva, (size_t)map_len);
        const char *parts[] = {"dlopen: address space for '", path,
                               "' exhausted"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
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
    obj->map_va = (uintptr_t)va;
    obj->map_size = map_size;
    obj->refs = 1u;
    obj->runtime = (runtime != 0) ? 1u : 0u;
    ldso_copy_name(obj->name, sizeof(obj->name), name);
    ldso_copy_name(obj->path, sizeof(obj->path), path);

    const ls_dyn_t *dyn = (const ls_dyn_t *)(uintptr_t)(bias + dyn_off);
    struct ls_dyn_info di = {0};
    ldso_parse_dynamic(dyn, &di);
    if (di.strtab_va == 0u || di.symtab_va == 0u) {
        const char *parts[] = {"dlopen: '", path, "' has no dynamic table"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
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
    obj->fini_fn =
        di.fini_va ? (ls_init_fn_t)(uintptr_t)(bias + di.fini_va) : NULL;
    obj->fini_array =
        di.fini_array_va
            ? (const ls_init_fn_t *)(uintptr_t)(bias + di.fini_array_va)
            : NULL;
    obj->fini_array_count = di.fini_array_sz / sizeof(ls_init_fn_t);

    obj->next = g_objs;
    g_objs = obj;

    for (size_t i = 0u; i < di.n_needed; i++) {
        if ((size_t)di.needed_offs[i] >= di.strsz) {
            const char *parts[] = {"dlopen: invalid DT_NEEDED offset in '",
                                   path, "'"};
            ldso_err_parts(err, errsz, parts, 3u);
            return NULL;
        }
        ls_obj_t *dep = ldso_load_lib(obj->strtab + di.needed_offs[i],
                                      depth + 1, runtime, err, errsz);
        if (!dep) {
            ldso_unlink_obj(obj);
            return NULL;
        }
        if (obj->ndeps < LSO_MAX_DEPS) {
            obj->deps[obj->ndeps++] = dep;
        }
    }

    if (g_trace_only) {
        return obj;
    }

    if (di.rela_va != 0u && di.relasz > 0u &&
        ldso_apply_rela(obj, (const ls_rela_t *)(uintptr_t)(bias + di.rela_va),
                        (size_t)(di.relasz / sizeof(ls_rela_t))) != 0) {
        ldso_unlink_obj(obj);
        const char *parts[] = {"dlopen: relocation failed for '", path, "'"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
    }
    if (di.jmprel_va != 0u && di.pltrelsz > 0u &&
        ldso_apply_rela(obj,
                        (const ls_rela_t *)(uintptr_t)(bias + di.jmprel_va),
                        (size_t)(di.pltrelsz / sizeof(ls_rela_t))) != 0) {
        ldso_unlink_obj(obj);
        const char *parts[] = {"dlopen: relocation failed for '", path, "'"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
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
            ldso_unlink_obj(obj);
            const char *parts[] = {"dlopen: W^X protect failed for '", path,
                                   "'"};
            ldso_err_parts(err, errsz, parts, 3u);
            return NULL;
        }
    }
    return obj;
}

/* ldd trace mode: the loader runs when argv[0] ends in "ldd" or the environment
 * sets LD_TRACE_LOADED_OBJECTS=1. It maps the target's DT_NEEDED dependencies
 * (transitively), reports "name => path (0xaddr)" for each library, and exits
 * without handing control to AT_ENTRY. */

#define LSO_TRACE_PRINTED_MAX 32u

static ls_obj_t *g_trace_printed[LSO_TRACE_PRINTED_MAX];
static size_t g_trace_printed_count;

static void ldso_fmt_hex(char *buf, uint64_t v) {
    static const char hex[] = "0123456789abcdef";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        buf[2 + i] = hex[v & 0xFu];
        v >>= 4;
    }
    buf[18] = '\0';
}

static void ldso_trace_line(const char *name, const char *path, uint64_t addr) {
    char addr_hex[19];
    char buf[128];
    ldso_fmt_hex(addr_hex, addr);
    const char *parts[6] = {name, " => ", path, " (", addr_hex, ")\n"};
    size_t pos = 0u;
    for (size_t i = 0u; i < 6u; i++) {
        const char *p = parts[i];
        while (*p != '\0' && pos + 1u < sizeof(buf)) {
            buf[pos++] = *p++;
        }
    }
    buf[pos] = '\0';
    (void)ldso_fd_write(1, buf, pos);
}

static void ldso_trace_notfound(const char *name) {
    char buf[128];
    const char *parts[3] = {name, " => not found\n", ""};
    size_t pos = 0u;
    for (size_t i = 0u; i < 3u; i++) {
        const char *p = parts[i];
        while (*p != '\0' && pos + 1u < sizeof(buf)) {
            buf[pos++] = *p++;
        }
    }
    buf[pos] = '\0';
    (void)ldso_fd_write(1, buf, pos);
}

static void ldso_trace_print(ls_obj_t *obj) {
    for (size_t i = 0u; i < g_trace_printed_count; i++) {
        if (g_trace_printed[i] == obj) {
            return;
        }
    }
    if (g_trace_printed_count < LSO_TRACE_PRINTED_MAX) {
        g_trace_printed[g_trace_printed_count++] = obj;
    }
    ldso_trace_line(obj->name, obj->path, obj->bias);
    for (size_t i = 0u; i < obj->ndeps; i++) {
        ldso_trace_print(obj->deps[i]);
    }
}

/* Read /etc/ld.so.conf and append each listed directory to the search path
 * (before the built-in default directories). A missing or unreadable file
 * leaves the search paths unchanged. */
static void ldso_load_so_conf(void) {
    long fd = ldso_vfs_open("/etc/ld.so.conf");
    if (fd < 0) {
        return;
    }
    long fsz = ldso_vfs_size((uint32_t)fd);
    if (fsz < 0 || fsz > 4096u) {
        ldso_vfs_close((uint32_t)fd);
        return;
    }
    uint8_t *buf = ldso_alloc((size_t)fsz + 1u);
    if (ldso_vfs_read((uint32_t)fd, buf, (size_t)fsz) != fsz) {
        ldso_vfs_close((uint32_t)fd);
        return;
    }
    ldso_vfs_close((uint32_t)fd);
    buf[fsz] = '\0';
    char *line = (char *)buf;
    while (*line != '\0' && g_path_dir_count < LSO_MAX_PATH_DIRS) {
        while (*line == ' ' || *line == '\t') {
            line++;
        }
        const char *start = line;
        while (*line != '\0' && *line != '\n' && *line != '\r') {
            line++;
        }
        size_t len = (size_t)(line - start);
        while (len > 0u &&
               (start[len - 1u] == ' ' || start[len - 1u] == '\t')) {
            len--;
        }
        if (len > 0u && start[0] != '#' && len < LSO_NAME_MAX) {
            int dup = 0;
            for (size_t i = 0u; i < g_path_dir_count; i++) {
                if (g_path_dir_len[i] == len &&
                    ldso_strneq(g_path_dirs[i], start, len)) {
                    dup = 1;
                    break;
                }
            }
            if (!dup) {
                g_path_dirs[g_path_dir_count] = (char *)start;
                g_path_dir_len[g_path_dir_count] = len;
                g_path_dir_count++;
            }
        }
        while (*line == '\r' || *line == '\n') {
            line++;
        }
    }
}

/* Convert a virtual address inside an ELF file image to a file offset using
 * its PT_LOAD program headers. Return 1 on success, 0 if not mapped. */
static int ldso_vaddr_to_off(const ls_phdr_t *ph, uint16_t phnum, uint64_t va,
                             uint64_t fsz, uint64_t *off_out) {
    for (uint16_t i = 0u; i < phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_filesz == 0u) {
            continue;
        }
        if (va >= ph[i].p_vaddr && va < ph[i].p_vaddr + ph[i].p_filesz) {
            uint64_t off = ph[i].p_offset + (va - ph[i].p_vaddr);
            if (off < fsz) {
                *off_out = off;
                return 1;
            }
        }
    }
    return 0;
}

static void ldso_trace_needed(const struct ls_dyn_info *di, const char *strtab,
                              const char *target) __attribute__((noreturn));

static void ldso_trace_needed(const struct ls_dyn_info *di, const char *strtab,
                              const char *target) {
    char err[LSO_ERR_MAX];
    g_trace_only = 1;
    int missing = 0;
    for (size_t i = 0u; i < di->n_needed; i++) {
        if ((size_t)di->needed_offs[i] >= di->strsz) {
            const char *parts[] = {"ldd: invalid DT_NEEDED offset in '", target,
                                   "'"};
            ldso_err_parts(err, sizeof(err), parts, 3u);
            ldso_exit(1);
        }
        const char *name = strtab + di->needed_offs[i];
        ls_obj_t *obj = ldso_load_lib(name, 0, 0, err, sizeof(err));
        if (!obj) {
            ldso_trace_notfound(name);
            missing = 1;
            continue;
        }
        ldso_trace_print(obj);
    }
    ldso_exit(missing ? 1 : 0);
}

static void ldso_trace(const char *target) __attribute__((noreturn));

static void ldso_trace(const char *target) {
    char err[LSO_ERR_MAX];
    long fd = ldso_vfs_open(target);
    if (fd < 0) {
        const char *parts[] = {"ldd: open '", target, "' failed"};
        ldso_err_parts(err, sizeof(err), parts, 3u);
        ldso_exit(1);
    }
    long fsz = ldso_vfs_size((uint32_t)fd);
    if (fsz < (long)sizeof(ls_ehdr_t) || fsz > (long)LSO_MAX_FILE) {
        ldso_vfs_close((uint32_t)fd);
        const char *parts[] = {"ldd: invalid ELF size for '", target, "'"};
        ldso_err_parts(err, sizeof(err), parts, 3u);
        ldso_exit(1);
    }
    long map_len = ldso_page_up((uintptr_t)fsz);
    long fva = VMMap(0, (size_t)map_len, VM_PROT_READ | VM_PROT_WRITE,
                     VM_MAP_PRIVATE | VM_MAP_ANON);
    if (fva < 0) {
        ldso_vfs_close((uint32_t)fd);
        const char *parts[] = {"ldd: mapping '", target, "' failed"};
        ldso_err_parts(err, sizeof(err), parts, 3u);
        ldso_exit(1);
    }
    uint8_t *fbuf = (uint8_t *)(uintptr_t)fva;
    if (ldso_vfs_read((uint32_t)fd, fbuf, (size_t)fsz) != fsz) {
        ldso_vfs_close((uint32_t)fd);
        VMUnmap((uintptr_t)fva, (size_t)map_len);
        const char *parts[] = {"ldd: read '", target, "' failed"};
        ldso_err_parts(err, sizeof(err), parts, 3u);
        ldso_exit(1);
    }
    ldso_vfs_close((uint32_t)fd);

    const ls_ehdr_t *eh = (const ls_ehdr_t *)fbuf;
    if (eh->e_ident[EI_MAG0] != 0x7Fu || eh->e_ident[EI_MAG1] != 'E' ||
        eh->e_ident[EI_MAG2] != 'L' || eh->e_ident[EI_MAG3] != 'F' ||
        eh->e_ident[EI_CLASS] != ELFCLASS64 ||
        eh->e_ident[EI_DATA] != ELFDATA2LSB ||
        eh->e_phoff + (uint64_t)eh->e_phnum * sizeof(ls_phdr_t) >
            (uint64_t)fsz) {
        VMUnmap((uintptr_t)fva, (size_t)map_len);
        const char *parts[] = {"ldd: '", target, "' is not a valid ELF"};
        ldso_err_parts(err, sizeof(err), parts, 3u);
        ldso_exit(1);
    }
    const ls_phdr_t *ph = (const ls_phdr_t *)(fbuf + eh->e_phoff);
    uint64_t dyn_off = 0u;
    for (uint16_t i = 0u; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_DYNAMIC) {
            dyn_off = ph[i].p_offset;
        }
    }
    if (dyn_off == 0u || dyn_off + sizeof(ls_dyn_t) > (uint64_t)fsz) {
        VMUnmap((uintptr_t)fva, (size_t)map_len);
        ldso_exit(0);
    }
    const ls_dyn_t *dyn = (const ls_dyn_t *)(uintptr_t)(fbuf + dyn_off);
    struct ls_dyn_info di = {0};
    ldso_parse_dynamic(dyn, &di);
    uint64_t strtab_off = 0u;
    if (di.strtab_va == 0u || di.strsz == 0u ||
        !ldso_vaddr_to_off(ph, eh->e_phnum, di.strtab_va, (uint64_t)fsz,
                           &strtab_off) ||
        strtab_off + di.strsz > (uint64_t)fsz) {
        VMUnmap((uintptr_t)fva, (size_t)map_len);
        const char *parts[] = {"ldd: '", target, "' has no dynamic table"};
        ldso_err_parts(err, sizeof(err), parts, 3u);
        ldso_exit(1);
    }
    const char *strtab = (const char *)(uintptr_t)(fbuf + strtab_off);
    g_trace_only = 1;
    int missing = 0;
    for (size_t i = 0u; i < di.n_needed; i++) {
        if ((size_t)di.needed_offs[i] >= di.strsz) {
            VMUnmap((uintptr_t)fva, (size_t)map_len);
            const char *parts[] = {"ldd: invalid DT_NEEDED offset in '", target,
                                   "'"};
            ldso_err_parts(err, sizeof(err), parts, 3u);
            ldso_exit(1);
        }
        const char *name = strtab + di.needed_offs[i];
        ls_obj_t *obj = ldso_load_lib(name, 0, 0, err, sizeof(err));
        if (!obj) {
            ldso_trace_notfound(name);
            missing = 1;
            continue;
        }
        ldso_trace_print(obj);
    }
    VMUnmap((uintptr_t)fva, (size_t)map_len);
    ldso_exit(missing ? 1 : 0);
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

/*------------------------------------------------------------------------------
 * dlfcn runtime delegation (called by libc.so through weak symbols). Handles
 * are ls_obj_t pointers; errors travel back in caller-provided buffers so libc
 * keeps its thread-local dlerror state. Loader tables are not locked: runtime
 * loading stays a single-threaded loader operation (see ADR_0059).
 *
 *---------------------------------------------------------------------------*/

__attribute__((visibility("default"))) void *
ldso_dlopen(const char *file, int mode, char *err, size_t errsz) {
    if ((mode & (RTLD_LAZY | RTLD_NOW)) == 0) {
        ldso_err_msg(err, errsz,
                     "dlopen: mode must specify RTLD_LAZY or RTLD_NOW");
        return NULL;
    }
    if (!file) {
        return &g_main;
    }
    /* Binding is always eager and every object joins the global scope, so
     * RTLD_LAZY/RTLD_NOW and RTLD_LOCAL/RTLD_GLOBAL are accepted as
     * aliases (documented simplification). */
    char path[VFS_PATH_MAX];
    if (ldso_resolve_path(file, path, sizeof(path)) != 0) {
        const char *parts[] = {"dlopen: cannot find '", file, "'"};
        ldso_err_parts(err, errsz, parts, 3u);
        return NULL;
    }
    /* Deduplicate by DT_NEEDED-style name or resolved path and hand out an
     * additional reference to the existing object. */
    if (g_main.name[0] != '\0' &&
        (ldso_streq(g_main.name, file) || ldso_streq(g_main.path, path))) {
        g_main.refs++;
        return &g_main;
    }
    for (ls_obj_t *o = g_objs; o; o = o->next) {
        if (ldso_streq(o->name, file) ||
            (o->path[0] != '\0' && ldso_streq(o->path, path))) {
            o->refs++;
            return o;
        }
    }
    ls_obj_t *obj = ldso_load_lib(file, 0, 1, err, errsz);
    if (obj) {
        /* Run the new subtree's constructors once, dependencies first. */
        ldso_run_ctors(obj);
    }
    return obj;
}

__attribute__((visibility("default"))) void *
ldso_dlsym(void *handle, const char *name, char *err, size_t errsz) {
    if (!name || name[0] == '\0') {
        ldso_err_msg(err, errsz, "dlsym: empty symbol name");
        return NULL;
    }
    if (handle == RTLD_DEFAULT) {
        void *sym = ldso_lookup_global(name);
        if (!sym) {
            const char *parts[] = {"dlsym: symbol '", name, "' not found"};
            ldso_err_parts(err, errsz, parts, 3u);
        }
        return sym;
    }
    if (handle == RTLD_NEXT) {
        /* The global scope order is the main executable, then libraries in load
         * order (the reverse of the newest-first g_objs list), then the
         * interpreter. RTLD_NEXT searches the scope after the caller's object:
         * the newer objects from the list head up to the caller,
         * then the interpreter itself. */
        uintptr_t ra = (uintptr_t)__builtin_return_address(0);
        ls_obj_t *caller = NULL;
        for (ls_obj_t *o = g_objs; o; o = o->next) {
            if (o->map_size > 0u && ra >= o->map_va &&
                ra < o->map_va + o->map_size) {
                caller = o;
                break;
            }
        }
        void *sym = NULL;
        for (ls_obj_t *o = g_objs; o && o != caller && !sym; o = o->next) {
            sym = ldso_lookup_in_obj(o, name);
        }
        if (!sym) {
            sym = ldso_lookup_in_obj(&g_self, name);
        }
        if (!sym) {
            const char *parts[] = {"dlsym: symbol '", name,
                                   "' not found (RTLD_NEXT)"};
            ldso_err_parts(err, errsz, parts, 3u);
        }
        return sym;
    }
    if (handle == (void *)&g_main) {
        void *sym = ldso_lookup_with_deps(&g_main, name, 0);
        if (!sym) {
            const char *parts[] = {"dlsym: symbol '", name, "' not found"};
            ldso_err_parts(err, errsz, parts, 3u);
        }
        return sym;
    }
    for (ls_obj_t *o = g_objs; o; o = o->next) {
        if (o == (ls_obj_t *)handle) {
            void *sym = ldso_lookup_with_deps(o, name, 0);
            if (!sym) {
                const char *parts[] = {"dlsym: symbol '", name, "' not found"};
                ldso_err_parts(err, errsz, parts, 3u);
            }
            return sym;
        }
    }
    ldso_err_msg(err, errsz, "dlsym: invalid handle");
    return NULL;
}

__attribute__((visibility("default"))) int ldso_dlclose(void *handle, char *err,
                                                        size_t errsz) {
    if (handle == (void *)&g_main) {
        return 0;
    }
    ls_obj_t **pp = &g_objs;
    while (*pp && (void *)*pp != handle) {
        pp = &(*pp)->next;
    }
    if (!*pp) {
        ldso_err_msg(err, errsz, "dlclose: invalid handle");
        return -1;
    }
    ls_obj_t *obj = *pp;
    if (obj->refs > 1u) {
        obj->refs--;
        return 0;
    }
    if (obj->refs == 0u) {
        ldso_err_msg(err, errsz, "dlclose: invalid handle");
        return -1;
    }
    if (!obj->runtime) {
        /* Startup DT_NEEDED objects stay loaded for the process lifetime
         * even after dlclose (the executable itself references them). */
        return 0;
    }
    ldso_run_finis(obj);
    *pp = obj->next;
    (void)VMUnmap(obj->map_va, obj->map_size);
    /* The ls_obj_t bookkeeping block lives in the bump arena and is
     * intentionally not reclaimed (see ADR_0059). */
    return 0;
}

__attribute__((visibility("default"))) int ldso_dladdr(const void *addr,
                                                       Dl_info_t *dlip) {
    if (!addr || !dlip) {
        return 0;
    }
    uintptr_t target = (uintptr_t)addr;
    ls_obj_t *owner = NULL;
    if (g_main.map_size > 0u && target >= g_main.map_va &&
        target < g_main.map_va + g_main.map_size) {
        owner = &g_main;
    }
    for (ls_obj_t *o = g_objs; o && !owner; o = o->next) {
        if (o->map_size > 0u && target >= o->map_va &&
            target < o->map_va + o->map_size) {
            owner = o;
        }
    }
    if (!owner && g_self.map_size > 0u && target >= g_self.map_va &&
        target < g_self.map_va + g_self.map_size) {
        owner = &g_self;
    }
    if (!owner) {
        return 0;
    }
    dlip->dli_fbase = (void *)(uintptr_t)owner->bias;
    dlip->dli_fname = owner->path[0] != '\0' ? owner->path : owner->name;
    dlip->dli_sname = NULL;
    dlip->dli_saddr = NULL;
    uintptr_t best_addr = 0u;
    for (uint32_t i = 1u; owner->symtab && i < owner->nsyms; i++) {
        const ls_sym_t *sym = &owner->symtab[i];
        if (sym->st_shndx == SHN_UNDEF ||
            (size_t)sym->st_name >= owner->strsz) {
            continue;
        }
        unsigned bind = (unsigned)(sym->st_info >> 4);
        if (bind != STB_GLOBAL && bind != STB_WEAK) {
            continue;
        }
        uintptr_t saddr = (uintptr_t)(owner->bias + sym->st_value);
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
    return 1;
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
    if (at_entry == 0u || at_phdr == 0u || at_phent != sizeof(ls_phdr_t) ||
        at_phnum == 0u) {
        ldso_exit(LSO_EXIT_AUXV);
    }

    /* Self-relocate from ld.so's own program headers. As the interpreter the
     * self base is at_base. When ld.so is the main executable (no interpreter
     * was loaded, so at_base == 0, which only happens in ldd mode) the self
     * base is the main executable's base. */
    int self_is_main = (at_base == 0u);
    uint64_t self_base = at_base;
    const ls_phdr_t *self_ph = NULL;
    const ls_ehdr_t *self_eh = NULL;
    if (self_is_main) {
        self_ph = (const ls_phdr_t *)(uintptr_t)at_phdr;
        self_base = ldso_main_bias(at_phdr, self_ph, at_phnum);
    } else {
        self_eh = (const ls_ehdr_t *)(uintptr_t)at_base;
        self_ph = (const ls_phdr_t *)(uintptr_t)(at_base + self_eh->e_phoff);
    }
    uint64_t self_phnum = self_is_main ? at_phnum : self_eh->e_phnum;
    uint64_t self_dyn_va = 0u;
    uint64_t self_min_va = (uint64_t)-1;
    uint64_t self_max_va = 0u;
    for (uint64_t i = 0u; i < self_phnum; i++) {
        if (self_ph[i].p_type == PT_DYNAMIC) {
            self_dyn_va = self_ph[i].p_vaddr;
        }
        if (self_ph[i].p_type == PT_LOAD && self_ph[i].p_memsz > 0u) {
            uint64_t s = self_ph[i].p_vaddr & ~0xFFFULL;
            uint64_t b = (self_ph[i].p_vaddr + self_ph[i].p_memsz + 0xFFFULL) &
                         ~0xFFFULL;
            if (s < self_min_va) {
                self_min_va = s;
            }
            if (b > self_max_va) {
                self_max_va = b;
            }
        }
    }
    if (self_dyn_va == 0u) {
        ldso_exit(LSO_EXIT_AUXV);
    }
    ls_obj_t self = {0};
    self.bias = self_base;
    self.map_va = (uintptr_t)(self_base + self_min_va);
    self.map_size = (size_t)(self_max_va - self_min_va);
    ldso_copy_name(self.name, sizeof(self.name), "ld.so");
    const ls_dyn_t *self_dyn =
        (const ls_dyn_t *)(uintptr_t)(self_base + self_dyn_va);
    struct ls_dyn_info sdi = {0};
    ldso_parse_dynamic(self_dyn, &sdi);
    if (sdi.strtab_va != 0u && sdi.symtab_va != 0u) {
        self.strtab = (const char *)(uintptr_t)(self_base + sdi.strtab_va);
        self.strsz = sdi.strsz;
        self.symtab = (const ls_sym_t *)(uintptr_t)(self_base + sdi.symtab_va);
        self.nsyms =
            ldso_count_syms(sdi.hash_va ? self_base + sdi.hash_va : 0,
                            sdi.gnu_hash_va ? self_base + sdi.gnu_hash_va : 0);
        if (ldso_apply_rela(
                &self, (const ls_rela_t *)(uintptr_t)(self_base + sdi.rela_va),
                (size_t)(sdi.relasz / sizeof(ls_rela_t))) != 0) {
            ldso_exit(LSO_EXIT_RELOC);
        }
        if (ldso_apply_rela(
                &self,
                (const ls_rela_t *)(uintptr_t)(self_base + sdi.jmprel_va),
                (size_t)(sdi.pltrelsz / sizeof(ls_rela_t))) != 0) {
            ldso_exit(LSO_EXIT_RELOC);
        }
    }
    /* Publish the interpreter in the global symbol scope so weak libc.so
     * references to the ldso_dlopen family resolve to ld.so exports. The fields
     * are copied individually: ld.so must not reference memcpy or
     * another external symbol before its own relocation completes. */
    g_self.bias = self.bias;
    g_self.symtab = self.symtab;
    g_self.strtab = self.strtab;
    g_self.strsz = self.strsz;
    g_self.nsyms = self.nsyms;
    g_self.init_fn = self.init_fn;
    g_self.init_array = self.init_array;
    g_self.init_array_count = self.init_array_count;
    g_self.fini_fn = self.fini_fn;
    g_self.fini_array = self.fini_array;
    g_self.fini_array_count = self.fini_array_count;
    for (size_t i = 0u; i < sizeof(g_self.name); i++) {
        g_self.name[i] = self.name[i];
        g_self.path[i] = self.path[i];
    }
    g_self.map_va = self.map_va;
    g_self.map_size = self.map_size;
    g_self.refs = self.refs;
    g_self.runtime = self.runtime;
    g_self.ctors_run = self.ctors_run;
    g_self.finis_run = self.finis_run;
    for (size_t i = 0u; i < LSO_MAX_DEPS; i++) {
        g_self.deps[i] = self.deps[i];
    }
    g_self.ndeps = self.ndeps;
    g_self.next = self.next;

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

    /* Connect to the VFS and load the /etc/ld.so.conf search directories. */
    ldso_vfs_connect();
    ldso_load_so_conf();

    /* Direct ldd invocation (ld.so executed as the main binary): trace the
     * target ELF path in argv[1] and exit without running any code. */
    int trace = self_is_main;
    if (!trace && argv && argv[0] && ldso_ends_with(argv[0], "ldd")) {
        trace = 1;
    }
    if (!trace) {
        for (char **v = envp; *v; v++) {
            if (ldso_strneq(*v, "LD_TRACE_LOADED_OBJECTS=1", 25u) &&
                (*v)[25] == '\0') {
                trace = 1;
                break;
            }
        }
    }
    if (self_is_main) {
        const char *target =
            (argc > 1 && argv && argv[1] && argv[1][0] != '\0') ? argv[1] : "";
        ldso_trace(target);
    }

    /* Parse the main executable and load its DT_NEEDED libraries. */
    const ls_phdr_t *main_ph = (const ls_phdr_t *)(uintptr_t)at_phdr;
    g_main.bias = ldso_main_bias(at_phdr, main_ph, at_phnum);
    uint64_t main_dyn_va = 0u;
    uint64_t main_min_va = (uint64_t)-1;
    uint64_t main_max_va = 0u;
    for (uint64_t i = 0u; i < at_phnum; i++) {
        if (main_ph[i].p_type == PT_DYNAMIC) {
            main_dyn_va = main_ph[i].p_vaddr;
        }
        if (main_ph[i].p_type == PT_LOAD && main_ph[i].p_memsz > 0u) {
            uint64_t s = (g_main.bias + main_ph[i].p_vaddr) & ~0xFFFULL;
            uint64_t b =
                ldso_page_up((uintptr_t)(g_main.bias + main_ph[i].p_vaddr +
                                         main_ph[i].p_memsz));
            if (s < main_min_va) {
                main_min_va = s;
            }
            if (b > main_max_va) {
                main_max_va = b;
            }
        }
    }
    if (main_dyn_va == 0u) {
        ldso_exit(LSO_EXIT_MAIN);
    }
    g_main.map_va = (uintptr_t)main_min_va;
    g_main.map_size = (size_t)(main_max_va - main_min_va);
    if (argc > 0 && argv[0]) {
        ldso_copy_name(g_main.name, sizeof(g_main.name), argv[0]);
        ldso_copy_name(g_main.path, sizeof(g_main.path), argv[0]);
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

    if (trace) {
        ldso_trace_needed(&mdi, g_main.strtab,
                          (argc > 0 && argv[0]) ? argv[0] : "main");
    }

    /* A dynamic executable that depends on libc.so runs its DT_INIT and
     * DT_INIT_ARRAY inside __libc_start_main after libc TLS setup, matching the
     * static crt0 handoff. Freestanding dynamic executables get their
     * constructors run here (see ADR_0059). */
    int main_needs_libc = 0;
    for (size_t i = 0u; i < mdi.n_needed; i++) {
        if ((size_t)mdi.needed_offs[i] < mdi.strsz &&
            ldso_streq(g_main.strtab + mdi.needed_offs[i], "libc.so")) {
            main_needs_libc = 1;
            break;
        }
    }

    for (size_t i = 0u; i < mdi.n_needed; i++) {
        if ((size_t)mdi.needed_offs[i] >= mdi.strsz) {
            ldso_exit(LSO_EXIT_MAIN);
        }
        if (!ldso_load_lib(g_main.strtab + mdi.needed_offs[i], 0, 0, NULL,
                           0u)) {
            ldso_exit(LSO_EXIT_OPEN);
        }
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

    /* Run library constructors in dependency order. */
    for (ls_obj_t *o = g_objs; o; o = o->next) {
        ldso_run_ctors(o);
    }
    if (!main_needs_libc) {
        if (mdi.init_va != 0u) {
            ((ls_init_fn_t)(uintptr_t)(g_main.bias + mdi.init_va))();
        }
        if (mdi.init_array_va != 0u) {
            const ls_init_fn_t *arr =
                (const ls_init_fn_t *)(uintptr_t)(g_main.bias +
                                                  mdi.init_array_va);
            for (size_t i = 0u; i < mdi.init_array_sz / sizeof(ls_init_fn_t);
                 i++) {
                if (arr[i]) {
                    arr[i]();
                }
            }
        }
    }

    ldso_jump(at_entry, stack);
}
