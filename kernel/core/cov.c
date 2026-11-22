/**
 * @file     cov.c
 * @brief    Freestanding LLVM Raw Profile v8 collector over console framing
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/cov.h>
#include <kernel/sched.h>
#include <sys/errno.h>

/* need:: COMP_DOCS_0004 */

#ifdef MICROKERNEL_COVERAGE

#include <kernel/arch.h>
#include <kernel/string.h>
#include <kernel/uaccess.h>
#include <stddef.h>
#include <sys/microkernel.h>

/* LLVM 14 Raw Profile v8 constants (InstrProfData.inc). */
#define LLVM_PROF_RAW_MAGIC_64 0xFF6C70726F667281ULL
#define LLVM_PROF_RAW_VERSION 8ULL
#define LLVM_PROF_DATA_RECORD_SIZE 48ULL
#define LLVM_PROF_VALUE_KIND_LAST 1ULL
#define COV_MAX_SECTION_BYTES (4ULL * 1024ULL * 1024ULL)
#define USER_ADDR_LIMIT 0x0000800000000000ULL

/* Uninstrumented runtime stubs required by Clang -fprofile-instr-generate. */
int __llvm_profile_runtime = 0;

void __llvm_profile_register_function(void *data);
void __llvm_profile_register_function(void *data) {
    (void)data;
}

void __llvm_profile_register_names_function(void *names, uint64_t size);
void __llvm_profile_register_names_function(void *names, uint64_t size) {
    (void)names;
    (void)size;
}

extern const char __start___llvm_prf_data[];
extern const char __stop___llvm_prf_data[];
extern const char __start___llvm_prf_cnts[];
extern const char __stop___llvm_prf_cnts[];
extern const char __start___llvm_prf_names[];
extern const char __stop___llvm_prf_names[];

struct llvm_prof_raw_hdr {
    uint64_t magic;
    uint64_t version;
    uint64_t binary_ids_size;
    uint64_t data_size;
    uint64_t padding_before_counters;
    uint64_t counters_size;
    uint64_t padding_after_counters;
    uint64_t names_size;
    uint64_t counters_delta;
    uint64_t names_delta;
    uint64_t value_kind_last;
};

typedef struct {
    uint8_t rem[3];
    size_t rem_len;
    char line[68];
    size_t line_len;
} b64_stream_t;

static const char B64_ALPHA[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_init(b64_stream_t *st) {
    st->rem_len = 0;
    st->line_len = 0;
}

static void b64_flush_line(b64_stream_t *st) {
    if (st->line_len > 0) {
        st->line[st->line_len++] = '\n';
        arch_console_write(st->line, st->line_len);
        st->line_len = 0;
    }
}

static void b64_emit_triplet(b64_stream_t *st, uint8_t b0, uint8_t b1,
                             uint8_t b2) {
    uint32_t v = ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | (uint32_t)b2;
    st->line[st->line_len++] = B64_ALPHA[(v >> 18) & 0x3Fu];
    st->line[st->line_len++] = B64_ALPHA[(v >> 12) & 0x3Fu];
    st->line[st->line_len++] = B64_ALPHA[(v >> 6) & 0x3Fu];
    st->line[st->line_len++] = B64_ALPHA[v & 0x3Fu];
    if (st->line_len >= 64) {
        b64_flush_line(st);
    }
}

static void b64_push(b64_stream_t *st, const uint8_t *buf, size_t len) {
    size_t i = 0;
    while (st->rem_len > 0 && i < len) {
        st->rem[st->rem_len++] = buf[i++];
        if (st->rem_len == 3) {
            b64_emit_triplet(st, st->rem[0], st->rem[1], st->rem[2]);
            st->rem_len = 0;
        }
    }
    while (i + 3 <= len) {
        b64_emit_triplet(st, buf[i], buf[i + 1], buf[i + 2]);
        i += 3;
    }
    while (i < len) {
        st->rem[st->rem_len++] = buf[i++];
    }
}

static void b64_finish(b64_stream_t *st) {
    if (st->rem_len == 1) {
        uint32_t v = (uint32_t)st->rem[0] << 16;
        st->line[st->line_len++] = B64_ALPHA[(v >> 18) & 0x3Fu];
        st->line[st->line_len++] = B64_ALPHA[(v >> 12) & 0x3Fu];
        st->line[st->line_len++] = '=';
        st->line[st->line_len++] = '=';
        st->rem_len = 0;
    } else if (st->rem_len == 2) {
        uint32_t v = ((uint32_t)st->rem[0] << 16) | ((uint32_t)st->rem[1] << 8);
        st->line[st->line_len++] = B64_ALPHA[(v >> 18) & 0x3Fu];
        st->line[st->line_len++] = B64_ALPHA[(v >> 12) & 0x3Fu];
        st->line[st->line_len++] = B64_ALPHA[(v >> 6) & 0x3Fu];
        st->line[st->line_len++] = '=';
        st->rem_len = 0;
    }
    b64_flush_line(st);
}

static int valid_name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}

static void sanitize_name(const char *src, char *dst, size_t dst_cap) {
    size_t n = 0;
    if (src) {
        for (size_t i = 0; src[i] != '\0' && n + 1 < dst_cap; i++) {
            if (valid_name_char(src[i])) {
                dst[n++] = src[i];
            }
        }
    }
    if (n == 0) {
        static const char fallback[] = "unknown";
        size_t flen = sizeof(fallback) - 1u;
        if (flen >= dst_cap) {
            flen = dst_cap - 1u;
        }
        memcpy(dst, fallback, flen);
        n = flen;
    }
    dst[n] = '\0';
}

static void emit_frame_begin(const char *raw_name) {
    char clean[32];
    sanitize_name(raw_name, clean, sizeof(clean));
    static const char prefix[] = "\n@@COV_BEGIN ";
    static const char suffix[] = "@@\n";
    arch_console_write(prefix, sizeof(prefix) - 1u);
    arch_console_write(clean, strlen(clean));
    arch_console_write(suffix, sizeof(suffix) - 1u);
}

static void emit_frame_end(void) {
    static const char end[] = "@@COV_END@@\n";
    arch_console_write(end, sizeof(end) - 1u);
}

static int validate_desc(const struct __k_cov_desc *d, int user_space) {
    if (d->data_end < d->data_begin || d->cnts_end < d->cnts_begin ||
        d->names_end < d->names_begin) {
        return 0;
    }
    uint64_t data_bytes = d->data_end - d->data_begin;
    uint64_t cnts_bytes = d->cnts_end - d->cnts_begin;
    uint64_t names_bytes = d->names_end - d->names_begin;
    if (data_bytes == 0 || cnts_bytes == 0) {
        return 0;
    }
    if (data_bytes > COV_MAX_SECTION_BYTES ||
        cnts_bytes > COV_MAX_SECTION_BYTES ||
        names_bytes > COV_MAX_SECTION_BYTES) {
        return 0;
    }
    if ((data_bytes % LLVM_PROF_DATA_RECORD_SIZE) != 0 ||
        (cnts_bytes % sizeof(uint64_t)) != 0) {
        return 0;
    }
    if (user_space) {
        if (d->data_end >= USER_ADDR_LIMIT || d->cnts_end >= USER_ADDR_LIMIT ||
            d->names_end >= USER_ADDR_LIMIT) {
            return 0;
        }
    }
    return 1;
}

static void fill_header(struct llvm_prof_raw_hdr *hdr,
                        const struct __k_cov_desc *d) {
    uint64_t data_bytes = d->data_end - d->data_begin;
    uint64_t cnts_bytes = d->cnts_end - d->cnts_begin;
    uint64_t names_bytes = d->names_end - d->names_begin;

    hdr->magic = LLVM_PROF_RAW_MAGIC_64;
    hdr->version = LLVM_PROF_RAW_VERSION;
    hdr->binary_ids_size = 0;
    hdr->data_size = data_bytes / LLVM_PROF_DATA_RECORD_SIZE;
    hdr->padding_before_counters = 0;
    hdr->counters_size = cnts_bytes / sizeof(uint64_t);
    hdr->padding_after_counters = 0;
    hdr->names_size = names_bytes;
    hdr->counters_delta = d->cnts_begin - d->data_begin;
    hdr->names_delta = d->names_begin;
    hdr->value_kind_last = LLVM_PROF_VALUE_KIND_LAST;
}

static int stream_user_range(b64_stream_t *st, uint64_t pml4_phys,
                             uint64_t uaddr, uint64_t len) {
    uint8_t buf[256];
    uint64_t off = 0;
    while (off < len) {
        size_t chunk =
            (len - off > sizeof(buf)) ? sizeof(buf) : (size_t)(len - off);
        if (copy_from_user_pml4(pml4_phys, buf, uaddr + off, chunk) < 0) {
            return -1;
        }
        b64_push(st, buf, chunk);
        off += chunk;
    }
    return 0;
}

void cov_dump_proc(pcb_t *p) {
    if (!p || !p->cov_registered || p->cov_dumped || !p->pml4_phys) {
        return;
    }
    if (!validate_desc(&p->cov, 1)) {
        return;
    }
    p->cov_dumped = 1;

    struct llvm_prof_raw_hdr hdr;
    fill_header(&hdr, &p->cov);

    emit_frame_begin(p->cov.name);
    b64_stream_t st;
    b64_init(&st);
    b64_push(&st, (const uint8_t *)&hdr, sizeof(hdr));
    if (stream_user_range(&st, p->pml4_phys, p->cov.data_begin,
                          p->cov.data_end - p->cov.data_begin) == 0 &&
        stream_user_range(&st, p->pml4_phys, p->cov.cnts_begin,
                          p->cov.cnts_end - p->cov.cnts_begin) == 0 &&
        stream_user_range(&st, p->pml4_phys, p->cov.names_begin,
                          p->cov.names_end - p->cov.names_begin) == 0) {
        uint64_t names_bytes = p->cov.names_end - p->cov.names_begin;
        size_t pad = (size_t)((8ULL - (names_bytes & 7ULL)) & 7ULL);
        if (pad > 0) {
            static const uint8_t zeros[8] = {0};
            b64_push(&st, zeros, pad);
        }
    }
    b64_finish(&st);
    emit_frame_end();
}

void cov_dump_kernel(void) {
    struct __k_cov_desc d;
    memset(&d, 0, sizeof(d));
    d.data_begin = (uint64_t)(uintptr_t)__start___llvm_prf_data;
    d.data_end = (uint64_t)(uintptr_t)__stop___llvm_prf_data;
    d.cnts_begin = (uint64_t)(uintptr_t)__start___llvm_prf_cnts;
    d.cnts_end = (uint64_t)(uintptr_t)__stop___llvm_prf_cnts;
    d.names_begin = (uint64_t)(uintptr_t)__start___llvm_prf_names;
    d.names_end = (uint64_t)(uintptr_t)__stop___llvm_prf_names;
    if (!validate_desc(&d, 0)) {
        return;
    }

    struct llvm_prof_raw_hdr hdr;
    fill_header(&hdr, &d);

    emit_frame_begin("kernel");
    b64_stream_t st;
    b64_init(&st);
    b64_push(&st, (const uint8_t *)&hdr, sizeof(hdr));
    b64_push(&st, (const uint8_t *)__start___llvm_prf_data,
             (size_t)(d.data_end - d.data_begin));
    b64_push(&st, (const uint8_t *)__start___llvm_prf_cnts,
             (size_t)(d.cnts_end - d.cnts_begin));
    b64_push(&st, (const uint8_t *)__start___llvm_prf_names,
             (size_t)(d.names_end - d.names_begin));
    uint64_t names_bytes = d.names_end - d.names_begin;
    size_t pad = (size_t)((8ULL - (names_bytes & 7ULL)) & 7ULL);
    if (pad > 0) {
        static const uint8_t zeros[8] = {0};
        b64_push(&st, zeros, pad);
    }
    b64_finish(&st);
    emit_frame_end();
}

long sys_cov_write(uint64_t op, uint64_t arg) {
    pcb_t *cur = proc_current();
    if (!cur) {
        return -EPERM;
    }
    if (op == COV_OP_REGISTER) {
        struct __k_cov_desc desc;
        if (copy_from_user(&desc, arg, sizeof(desc)) < 0) {
            return -EFAULT;
        }
        desc.name[sizeof(desc.name) - 1u] = '\0';
        if (!validate_desc(&desc, 1)) {
            return -EINVAL;
        }
        cur->cov = desc;
        cur->cov_registered = 1;
        cur->cov_dumped = 0;
        return 0;
    }
    if (op == COV_OP_DUMP_ALL) {
        for (pcb_t *p = proc_list_head(); p; p = p->next) {
            if (p->state == PROC_ALIVE && p->cov_registered && !p->cov_dumped) {
                cov_dump_proc(p);
            }
        }
        cov_dump_kernel();
        static const char done_marker[] = "\n@@COV_COMPLETE@@\n";
        arch_console_write(done_marker, sizeof(done_marker) - 1u);
        return 0;
    }
    return -EINVAL;
}

#else

long sys_cov_write(uint64_t op, uint64_t arg) {
    (void)op;
    (void)arg;
    return -ENOSYS;
}

void cov_dump_proc(pcb_t *p) {
    (void)p;
}

void cov_dump_kernel(void) {
}

#endif
