/**
 * @file     pax.c
 * @brief    Ring 3 pax archive utility targeting POSIX.1-2024 XCU (ustar/cpio)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <cpio.h>
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tar.h>
#include <unistd.h>

/* need:: COMP_LIBC_0022 */

enum {
    FMT_USTAR = 0,
    FMT_CPIO = 1,
};

#define MAX_SUBS 8

typedef struct {
    regex_t re;
    char *repl;
    int global;
    int print;
    int compiled;
} subst_t;

static int g_read;
static int g_write;
static int g_verbose;
static int g_no_recurse;
static int g_complement;
static int g_no_clobber;
static int g_update_only;
static int g_hardlink;
static int g_follow_links;
static int g_fmt = FMT_USTAR;
static subst_t g_subs[MAX_SUBS];
static int g_nsubs;
static char **g_pats;
static int g_npats;

/* --- Path substitution (-s /old/new/[gp]) --- */

static int add_subst(const char *spec) {
    if (g_nsubs >= MAX_SUBS || !spec || spec[0] == '\0') {
        return -1;
    }
    char delim = spec[0];
    const char *p = spec + 1;
    const char *mid = NULL;
    while (*p != '\0') {
        if (*p == '\\' && p[1] != '\0') {
            p += 2;
            continue;
        }
        if (*p == delim) {
            mid = p;
            break;
        }
        p++;
    }
    if (!mid) {
        return -1;
    }
    size_t plen = (size_t)(mid - (spec + 1));
    char *pat = (char *)malloc(plen + 1u);
    if (!pat) {
        return -1;
    }
    memcpy(pat, spec + 1, plen);
    pat[plen] = '\0';

    p = mid + 1;
    const char *end = NULL;
    while (*p != '\0') {
        if (*p == '\\' && p[1] != '\0') {
            p += 2;
            continue;
        }
        if (*p == delim) {
            end = p;
            break;
        }
        p++;
    }
    if (!end) {
        free(pat);
        return -1;
    }
    size_t rlen = (size_t)(end - (mid + 1));
    char *repl = (char *)malloc(rlen + 1u);
    if (!repl) {
        free(pat);
        return -1;
    }
    memcpy(repl, mid + 1, rlen);
    repl[rlen] = '\0';

    subst_t *s = &g_subs[g_nsubs];
    memset(s, 0, sizeof(*s));
    if (regcomp(&s->re, pat, 0) != 0) {
        free(pat);
        free(repl);
        return -1;
    }
    free(pat);
    s->repl = repl;
    s->compiled = 1;
    for (const char *f = end + 1; *f != '\0'; f++) {
        if (*f == 'g') {
            s->global = 1;
        } else if (*f == 'p') {
            s->print = 1;
        }
    }
    g_nsubs++;
    return 0;
}

static void free_subs(void) {
    for (int i = 0; i < g_nsubs; i++) {
        if (g_subs[i].compiled) {
            regfree(&g_subs[i].re);
        }
        free(g_subs[i].repl);
    }
}

static int apply_subs(const char *in, char *out, size_t cap) {
    strncpy(out, in, cap - 1u);
    out[cap - 1u] = '\0';
    for (int i = 0; i < g_nsubs; i++) {
        subst_t *s = &g_subs[i];
        regmatch_t rm[10];
        if (regexec(&s->re, out, 10u, rm, 0) != 0) {
            continue;
        }
        char tmp[512];
        size_t pos = 0u;
        const char *cur = out;
        for (;;) {
            size_t pre = (size_t)rm[0].rm_so;
            if (pos + pre >= sizeof(tmp)) {
                return -1;
            }
            if (pre > 0u) {
                memcpy(tmp + pos, cur, pre);
                pos += pre;
            }
            for (const char *r = s->repl; *r != '\0'; r++) {
                if (*r == '&') {
                    size_t mlen = (size_t)(rm[0].rm_eo - rm[0].rm_so);
                    if (pos + mlen >= sizeof(tmp)) {
                        return -1;
                    }
                    memcpy(tmp + pos, cur + rm[0].rm_so, mlen);
                    pos += mlen;
                } else if (*r == '\\' && r[1] >= '1' && r[1] <= '9') {
                    int g = r[1] - '0';
                    r++;
                    if (rm[g].rm_so >= 0 && rm[g].rm_eo >= rm[g].rm_so) {
                        size_t gl = (size_t)(rm[g].rm_eo - rm[g].rm_so);
                        if (pos + gl >= sizeof(tmp)) {
                            return -1;
                        }
                        memcpy(tmp + pos, cur + rm[g].rm_so, gl);
                        pos += gl;
                    }
                } else if (*r == '\\' && r[1] != '\0') {
                    r++;
                    if (pos + 1u >= sizeof(tmp)) {
                        return -1;
                    }
                    tmp[pos++] = *r;
                } else {
                    if (pos + 1u >= sizeof(tmp)) {
                        return -1;
                    }
                    tmp[pos++] = *r;
                }
            }
            cur += rm[0].rm_eo;
            if (!s->global || *cur == '\0' || rm[0].rm_eo == 0 ||
                regexec(&s->re, cur, 10u, rm, 0) != 0) {
                break;
            }
        }
        size_t rem = strlen(cur);
        if (pos + rem >= sizeof(tmp)) {
            return -1;
        }
        memcpy(tmp + pos, cur, rem + 1u);
        if (s->print) {
            (void)fprintf(stderr, "%s >> %s\n", out, tmp);
        }
        strncpy(out, tmp, cap - 1u);
        out[cap - 1u] = '\0';
        break; /* POSIX: stop after first matching -s */
    }
    return (out[0] == '\0') ? 0 : 1;
}

static int pat_matches(const char *path) {
    if (g_npats == 0) {
        return 1;
    }
    int matched = 0;
    for (int i = 0; i < g_npats; i++) {
        size_t plen = strlen(g_pats[i]);
        if (fnmatch(g_pats[i], path, 0) == 0 ||
            (strncmp(path, g_pats[i], plen) == 0 &&
             (path[plen] == '/' || path[plen] == '\0'))) {
            matched = 1;
            break;
        }
    }
    return g_complement ? !matched : matched;
}

/* --- Filesystem helpers --- */

static int mkdir_parents(const char *path) {
    char buf[512];
    strncpy(buf, path, sizeof(buf) - 1u);
    buf[sizeof(buf) - 1u] = '\0';
    for (char *p = buf + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            (void)mkdir(buf, 0755);
            *p = '/';
        }
    }
    return 0;
}

static int can_overwrite(const char *path, time_t src_mtime) {
    struct stat st = {0};
    if (lstat(path, &st) != 0) {
        return 1;
    }
    if (g_no_clobber) {
        return 0;
    }
    if (g_update_only && src_mtime <= st.st_mtime) {
        return 0;
    }
    return 1;
}

static int read_exact(FILE *fp, void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0u;
    while (got < n) {
        size_t r = fread(p + got, 1u, n - got, fp);
        if (r == 0u) {
            return -1;
        }
        got += r;
    }
    return 0;
}

static int skip_bytes(FILE *fp, size_t n) {
    uint8_t sink[512];
    while (n > 0u) {
        size_t chunk = n > sizeof(sink) ? sizeof(sink) : n;
        if (read_exact(fp, sink, chunk) != 0) {
            return -1;
        }
        n -= chunk;
    }
    return 0;
}

static unsigned long parse_octal(const char *s, size_t n) {
    unsigned long v = 0ul;
    for (size_t i = 0u; i < n; i++) {
        if (s[i] == '\0') {
            break;
        }
        if (s[i] >= '0' && s[i] <= '7') {
            v = (v << 3) | (unsigned long)(s[i] - '0');
        }
    }
    return v;
}

static void write_octal(char *dst, size_t width, unsigned long val) {
    if (width == 0u) {
        return;
    }
    dst[width - 1u] = '\0';
    for (size_t i = width - 1u; i > 0u; i--) {
        dst[i - 1u] = (char)('0' + (val & 7ul));
        val >>= 3;
    }
}

/* --- USTAR write/read (<tar.h>) --- */

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
    char pad[12];
} ustar_hdr_t;

_Static_assert(sizeof(ustar_hdr_t) == 512u, "ustar header must be 512 bytes");

static int write_ustar_entry(FILE *out, const char *src_path,
                             const char *arc_name, const struct stat *st) {
    ustar_hdr_t h;
    memset(&h, 0, sizeof(h));

    size_t nlen = strlen(arc_name);
    if (nlen <= 100u) {
        memcpy(h.name, arc_name, nlen);
    } else {
        /* Split at '/' boundary into prefix (<=155) + name (<=100). */
        const char *split = NULL;
        for (const char *p = arc_name; *p != '\0'; p++) {
            if (*p == '/' && (size_t)(p - arc_name) <= 155u &&
                strlen(p + 1) <= 100u) {
                split = p;
            }
        }
        if (!split) {
            (void)fprintf(stderr, "pax: path too long for ustar: %s\n",
                          arc_name);
            return -1;
        }
        memcpy(h.prefix, arc_name, (size_t)(split - arc_name));
        strncpy(h.name, split + 1, sizeof(h.name));
    }

    write_octal(h.mode, sizeof(h.mode), (unsigned long)(st->st_mode & 07777u));
    write_octal(h.uid, sizeof(h.uid), (unsigned long)st->st_uid);
    write_octal(h.gid, sizeof(h.gid), (unsigned long)st->st_gid);
    write_octal(h.mtime, sizeof(h.mtime), (unsigned long)st->st_mtime);
    memcpy(h.magic, TMAGIC, TMAGLEN);
    memcpy(h.version, TVERSION, TVERSLEN);
    strncpy(h.uname, "root", sizeof(h.uname) - 1u);
    strncpy(h.gname, "root", sizeof(h.gname) - 1u);

    unsigned long fsize = 0ul;
    if (S_ISDIR(st->st_mode)) {
        h.typeflag = DIRTYPE;
    } else if (S_ISLNK(st->st_mode)) {
        h.typeflag = SYMTYPE;
        ssize_t ll = readlink(src_path, h.linkname, sizeof(h.linkname) - 1u);
        if (ll < 0) {
            perror(src_path);
            return -1;
        }
        h.linkname[ll] = '\0';
    } else if (S_ISFIFO(st->st_mode)) {
        h.typeflag = FIFOTYPE;
    } else {
        h.typeflag = REGTYPE;
        fsize = (unsigned long)st->st_size;
    }
    write_octal(h.size, sizeof(h.size), fsize);

    memset(h.chksum, ' ', sizeof(h.chksum));
    unsigned long sum = 0ul;
    const unsigned char *bytes = (const unsigned char *)&h;
    for (size_t i = 0u; i < 512u; i++) {
        sum += bytes[i];
    }
    write_octal(h.chksum, 7u, sum);
    h.chksum[7] = ' ';

    if (fwrite(&h, 1u, 512u, out) != 512u) {
        return -1;
    }

    if (h.typeflag == REGTYPE && fsize > 0ul) {
        FILE *in = fopen(src_path, "rb");
        if (!in) {
            perror(src_path);
            return -1;
        }
        uint8_t buf[512];
        unsigned long rem = fsize;
        while (rem > 0ul) {
            size_t chunk = rem > 512ul ? 512u : (size_t)rem;
            if (read_exact(in, buf, chunk) != 0) {
                (void)fclose(in);
                return -1;
            }
            if (fwrite(buf, 1u, chunk, out) != chunk) {
                (void)fclose(in);
                return -1;
            }
            rem -= chunk;
        }
        (void)fclose(in);
        size_t pad = (512u - (fsize % 512u)) % 512u;
        if (pad > 0u) {
            memset(buf, 0, pad);
            if (fwrite(buf, 1u, pad, out) != pad) {
                return -1;
            }
        }
    }
    return 0;
}

/* --- CPIO ODC write/read (<cpio.h>) --- */

static unsigned long g_cpio_ino = 1ul;

static int write_cpio_entry(FILE *out, const char *src_path,
                            const char *arc_name, const struct stat *st) {
    char linkbuf[256] = {0};
    unsigned long fsize = 0ul;
    unsigned long cmode = (unsigned long)(st->st_mode & 07777u);
    if (S_ISDIR(st->st_mode)) {
        cmode |= (unsigned long)C_ISDIR;
    } else if (S_ISLNK(st->st_mode)) {
        cmode |= (unsigned long)C_ISLNK;
        ssize_t ll = readlink(src_path, linkbuf, sizeof(linkbuf) - 1u);
        if (ll < 0) {
            perror(src_path);
            return -1;
        }
        linkbuf[ll] = '\0';
        fsize = (unsigned long)ll;
    } else if (S_ISFIFO(st->st_mode)) {
        cmode |= (unsigned long)C_ISFIFO;
    } else {
        cmode |= (unsigned long)C_ISREG;
        fsize = (unsigned long)st->st_size;
    }

    size_t namesize = strlen(arc_name) + 1u;
    char hdr[80];
    int hlen = snprintf(
        hdr, sizeof(hdr),
        "%s%06lo%06lo%06lo%06lo%06lo%06lo%06lo%011lo%06lo%011lo", MAGIC, 0ul,
        g_cpio_ino++, cmode, (unsigned long)(st->st_uid & 0777777u),
        (unsigned long)(st->st_gid & 0777777u), 1ul, 0ul,
        (unsigned long)st->st_mtime, (unsigned long)namesize, fsize);
    if (hlen != 76) {
        return -1;
    }
    if (fwrite(hdr, 1u, 76u, out) != 76u ||
        fwrite(arc_name, 1u, namesize, out) != namesize) {
        return -1;
    }
    if (S_ISLNK(st->st_mode)) {
        if (fsize > 0ul && fwrite(linkbuf, 1u, fsize, out) != fsize) {
            return -1;
        }
        return 0;
    }
    if (S_ISREG(st->st_mode) && fsize > 0ul) {
        FILE *in = fopen(src_path, "rb");
        if (!in) {
            perror(src_path);
            return -1;
        }
        uint8_t buf[512];
        unsigned long rem = fsize;
        while (rem > 0ul) {
            size_t chunk = rem > 512ul ? 512u : (size_t)rem;
            if (read_exact(in, buf, chunk) != 0 ||
                fwrite(buf, 1u, chunk, out) != chunk) {
                (void)fclose(in);
                return -1;
            }
            rem -= chunk;
        }
        (void)fclose(in);
    }
    return 0;
}

static int write_cpio_trailer(FILE *out) {
    static const char tr[] = "TRAILER!!!";
    char hdr[80];
    int hlen = snprintf(
        hdr, sizeof(hdr),
        "%s%06lo%06lo%06lo%06lo%06lo%06lo%06lo%011lo%06lo%011lo", MAGIC, 0ul,
        0ul, 0ul, 0ul, 0ul, 1ul, 0ul, 0ul, (unsigned long)sizeof(tr), 0ul);
    if (hlen != 76) {
        return -1;
    }
    return (fwrite(hdr, 1u, 76u, out) == 76u &&
            fwrite(tr, 1u, sizeof(tr), out) == sizeof(tr))
               ? 0
               : -1;
}

/* --- Recursive writer / copy-pass --- */

static int copy_single_entry(const char *src_path, const char *dst_path,
                             const struct stat *st) {
    if (!can_overwrite(dst_path, st->st_mtime)) {
        return 0;
    }
    (void)mkdir_parents(dst_path);
    if (S_ISDIR(st->st_mode)) {
        (void)mkdir(dst_path, st->st_mode & 07777u);
        (void)chmod(dst_path, st->st_mode & 07777u);
        return 0;
    }
    if (S_ISLNK(st->st_mode)) {
        char target[256];
        ssize_t ll = readlink(src_path, target, sizeof(target) - 1u);
        if (ll < 0) {
            return -1;
        }
        target[ll] = '\0';
        (void)unlink(dst_path);
        return symlink(target, dst_path);
    }
    if (S_ISFIFO(st->st_mode)) {
        (void)unlink(dst_path);
        return mkfifo(dst_path, st->st_mode & 07777u);
    }
    if (g_hardlink) {
        (void)unlink(dst_path);
        if (link(src_path, dst_path) == 0) {
            return 0;
        }
    }
    FILE *in = fopen(src_path, "rb");
    if (!in) {
        perror(src_path);
        return -1;
    }
    FILE *out = fopen(dst_path, "wb");
    if (!out) {
        perror(dst_path);
        (void)fclose(in);
        return -1;
    }
    uint8_t buf[1024];
    for (;;) {
        size_t got = fread(buf, 1u, sizeof(buf), in);
        if (got == 0u) {
            break;
        }
        if (fwrite(buf, 1u, got, out) != got) {
            (void)fclose(in);
            (void)fclose(out);
            return -1;
        }
    }
    (void)fclose(in);
    (void)fclose(out);
    (void)chmod(dst_path, st->st_mode & 07777u);
    return 0;
}

static int walk_write(FILE *out, const char *copy_dir, const char *path) {
    struct stat st = {0};
    int sr = g_follow_links ? stat(path, &st) : lstat(path, &st);
    if (sr != 0) {
        perror(path);
        return -1;
    }

    char mapped[512];
    int keep = apply_subs(path, mapped, sizeof(mapped));
    if (keep < 0) {
        return -1;
    }
    if (keep > 0) {
        if (g_verbose) {
            (void)fprintf(stderr, "%s\n", mapped);
        }
        if (copy_dir) {
            char dst[512];
            const char *rel = mapped;
            while (*rel == '/') {
                rel++;
            }
            (void)snprintf(dst, sizeof(dst), "%s/%s", copy_dir, rel);
            if (copy_single_entry(path, dst, &st) != 0) {
                return -1;
            }
        } else if (g_fmt == FMT_CPIO) {
            if (write_cpio_entry(out, path, mapped, &st) != 0) {
                return -1;
            }
        } else {
            if (write_ustar_entry(out, path, mapped, &st) != 0) {
                return -1;
            }
        }
    }

    if (S_ISDIR(st.st_mode) && !g_no_recurse) {
        DIR *dp = opendir(path);
        if (!dp) {
            perror(path);
            return -1;
        }
        struct dirent *de;
        int rc = 0;
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
                continue;
            }
            char child[512];
            (void)snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
            if (walk_write(out, copy_dir, child) != 0) {
                rc = -1;
            }
        }
        (void)closedir(dp);
        return rc;
    }
    return 0;
}

/* --- Archive reader / lister (auto-detect ustar vs cpio) --- */

static int extract_file_payload(FILE *in, const char *dst, mode_t mode,
                                size_t size) {
    (void)mkdir_parents(dst);
    FILE *out = fopen(dst, "wb");
    if (!out) {
        perror(dst);
        (void)skip_bytes(in, size);
        return -1;
    }
    uint8_t buf[512];
    size_t rem = size;
    while (rem > 0u) {
        size_t chunk = rem > sizeof(buf) ? sizeof(buf) : rem;
        if (read_exact(in, buf, chunk) != 0 ||
            fwrite(buf, 1u, chunk, out) != chunk) {
            (void)fclose(out);
            return -1;
        }
        rem -= chunk;
    }
    (void)fclose(out);
    (void)chmod(dst, mode & 07777u);
    return 0;
}

static int read_ustar_stream(FILE *in, const uint8_t first_hdr[512]) {
    ustar_hdr_t h;
    memcpy(&h, first_hdr, 512u);
    int first = 1;
    int rc = 0;
    for (;;) {
        if (!first && read_exact(in, &h, 512u) != 0) {
            break;
        }
        first = 0;
        int all_zero = 1;
        const uint8_t *raw = (const uint8_t *)&h;
        for (size_t i = 0u; i < 512u; i++) {
            if (raw[i] != 0u) {
                all_zero = 0;
                break;
            }
        }
        if (all_zero) {
            break;
        }

        char full[512];
        if (h.prefix[0] != '\0') {
            (void)snprintf(full, sizeof(full), "%.155s/%.100s", h.prefix,
                           h.name);
        } else {
            (void)snprintf(full, sizeof(full), "%.100s", h.name);
        }
        size_t fsize = (size_t)parse_octal(h.size, sizeof(h.size));
        mode_t mode = (mode_t)parse_octal(h.mode, sizeof(h.mode));
        time_t mtime = (time_t)parse_octal(h.mtime, sizeof(h.mtime));
        size_t pad = (512u - (fsize % 512u)) % 512u;

        char mapped[512];
        int keep =
            pat_matches(full) ? apply_subs(full, mapped, sizeof(mapped)) : 0;
        if (keep <= 0 || !g_read) {
            if (keep > 0) {
                if (g_verbose) {
                    (void)printf("%04o %8zu %s\n", (unsigned)(mode & 07777u),
                                 fsize, mapped);
                } else {
                    (void)puts(mapped);
                }
            }
            if (skip_bytes(in, fsize + pad) != 0) {
                rc = -1;
                break;
            }
            continue;
        }

        if (g_verbose) {
            (void)fprintf(stderr, "%s\n", mapped);
        }
        if (!can_overwrite(mapped, mtime)) {
            if (skip_bytes(in, fsize + pad) != 0) {
                rc = -1;
                break;
            }
            continue;
        }
        if (h.typeflag == DIRTYPE) {
            (void)mkdir_parents(mapped);
            (void)mkdir(mapped, mode & 07777u);
            (void)chmod(mapped, mode & 07777u);
            (void)skip_bytes(in, fsize + pad);
        } else if (h.typeflag == SYMTYPE) {
            char lnk[101];
            memcpy(lnk, h.linkname, 100u);
            lnk[100] = '\0';
            (void)mkdir_parents(mapped);
            (void)unlink(mapped);
            if (symlink(lnk, mapped) != 0) {
                rc = -1;
            }
            (void)skip_bytes(in, fsize + pad);
        } else if (h.typeflag == FIFOTYPE) {
            (void)mkdir_parents(mapped);
            (void)unlink(mapped);
            if (mkfifo(mapped, mode & 07777u) != 0) {
                rc = -1;
            }
            (void)skip_bytes(in, fsize + pad);
        } else {
            if (extract_file_payload(in, mapped, mode, fsize) != 0) {
                rc = -1;
            }
            if (pad > 0u && skip_bytes(in, pad) != 0) {
                rc = -1;
                break;
            }
        }
    }
    return rc;
}

static int read_cpio_stream(FILE *in, const char first_magic[6]) {
    char hdr[76];
    memcpy(hdr, first_magic, 6u);
    if (read_exact(in, hdr + 6, 70u) != 0) {
        return -1;
    }
    int first = 1;
    int rc = 0;
    for (;;) {
        if (!first && read_exact(in, hdr, 76u) != 0) {
            break;
        }
        first = 0;
        if (memcmp(hdr, MAGIC, 6u) != 0) {
            (void)fprintf(stderr, "pax: invalid cpio header\n");
            return -1;
        }
        unsigned long mode = parse_octal(hdr + 18, 6u);
        time_t mtime = (time_t)parse_octal(hdr + 48, 11u);
        size_t namesize = (size_t)parse_octal(hdr + 59, 6u);
        size_t fsize = (size_t)parse_octal(hdr + 65, 11u);
        if (namesize == 0u || namesize >= 512u) {
            return -1;
        }
        char name[512];
        if (read_exact(in, name, namesize) != 0) {
            return -1;
        }
        name[namesize - 1u] = '\0';
        if (strcmp(name, "TRAILER!!!") == 0) {
            break;
        }

        char mapped[512];
        int keep =
            pat_matches(name) ? apply_subs(name, mapped, sizeof(mapped)) : 0;
        if (keep <= 0 || !g_read) {
            if (keep > 0) {
                if (g_verbose) {
                    (void)printf("%04lo %8zu %s\n", mode & 07777ul, fsize,
                                 mapped);
                } else {
                    (void)puts(mapped);
                }
            }
            if (skip_bytes(in, fsize) != 0) {
                rc = -1;
                break;
            }
            continue;
        }

        if (g_verbose) {
            (void)fprintf(stderr, "%s\n", mapped);
        }
        if (!can_overwrite(mapped, mtime)) {
            if (skip_bytes(in, fsize) != 0) {
                rc = -1;
                break;
            }
            continue;
        }
        unsigned long ftype = mode & 0170000ul;
        if (ftype == (unsigned long)C_ISDIR) {
            (void)mkdir_parents(mapped);
            (void)mkdir(mapped, (mode_t)(mode & 07777ul));
            (void)chmod(mapped, (mode_t)(mode & 07777ul));
            (void)skip_bytes(in, fsize);
        } else if (ftype == (unsigned long)C_ISLNK) {
            char target[256];
            size_t rsz =
                fsize < sizeof(target) - 1u ? fsize : sizeof(target) - 1u;
            if (read_exact(in, target, rsz) != 0) {
                return -1;
            }
            target[rsz] = '\0';
            if (fsize > rsz) {
                (void)skip_bytes(in, fsize - rsz);
            }
            (void)mkdir_parents(mapped);
            (void)unlink(mapped);
            if (symlink(target, mapped) != 0) {
                rc = -1;
            }
        } else if (ftype == (unsigned long)C_ISFIFO) {
            (void)mkdir_parents(mapped);
            (void)unlink(mapped);
            if (mkfifo(mapped, (mode_t)(mode & 07777ul)) != 0) {
                rc = -1;
            }
            (void)skip_bytes(in, fsize);
        } else {
            if (extract_file_payload(in, mapped, (mode_t)mode, fsize) != 0) {
                rc = -1;
            }
        }
    }
    return rc;
}

static int run_read_or_list(const char *arc_path) {
    FILE *in = stdin;
    if (arc_path && strcmp(arc_path, "-") != 0) {
        in = fopen(arc_path, "rb");
        if (!in) {
            perror(arc_path);
            return 1;
        }
    }
    char peek[6];
    if (read_exact(in, peek, 6u) != 0) {
        if (in != stdin) {
            (void)fclose(in);
        }
        return 0; /* empty archive */
    }
    int rc = 0;
    if (memcmp(peek, MAGIC, 6u) == 0) {
        rc = read_cpio_stream(in, peek);
    } else {
        uint8_t blk[512];
        memcpy(blk, peek, 6u);
        if (read_exact(in, blk + 6, 506u) != 0) {
            rc = -1;
        } else {
            rc = read_ustar_stream(in, blk);
        }
    }
    if (in != stdin) {
        (void)fclose(in);
    }
    return rc == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    const char *arc_path = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "ab:cdf:HikLno:p:rs:tuvwx:X")) != -1) {
        switch (opt) {
        case 'c':
            g_complement = 1;
            break;
        case 'd':
            g_no_recurse = 1;
            break;
        case 'f':
            arc_path = optarg;
            break;
        case 'k':
            g_no_clobber = 1;
            break;
        case 'l':
            g_hardlink = 1;
            break;
        case 'L':
            g_follow_links = 1;
            break;
        case 'r':
            g_read = 1;
            break;
        case 's':
            if (add_subst(optarg) != 0) {
                (void)fprintf(stderr, "pax: invalid -s '%s'\n", optarg);
                free_subs();
                return 1;
            }
            break;
        case 'u':
            g_update_only = 1;
            break;
        case 'v':
            g_verbose = 1;
            break;
        case 'w':
            g_write = 1;
            break;
        case 'x':
            if (strcmp(optarg, "cpio") == 0) {
                g_fmt = FMT_CPIO;
            } else if (strcmp(optarg, "ustar") == 0 ||
                       strcmp(optarg, "pax") == 0) {
                g_fmt = FMT_USTAR;
            } else {
                (void)fprintf(stderr, "pax: unknown format '%s'\n", optarg);
                free_subs();
                return 1;
            }
            break;
        case 'a':
        case 'b':
        case 'H':
        case 'i':
        case 'n':
        case 'o':
        case 'p':
        case 't':
        case 'X':
            break;
        default:
            free_subs();
            return 1;
        }
    }

    int rc = 0;
    if (g_read && g_write) {
        /* Copy-pass mode: pax -rw [files...] directory */
        if (optind >= argc) {
            (void)fprintf(stderr, "pax: missing destination directory\n");
            free_subs();
            return 1;
        }
        const char *dst_dir = argv[argc - 1];
        (void)mkdir(dst_dir, 0755);
        if (optind == argc - 1) {
            char *line = NULL;
            size_t lcap = 0u;
            ssize_t n;
            while ((n = getline(&line, &lcap, stdin)) >= 0) {
                size_t len = (size_t)n;
                if (len > 0u && line[len - 1u] == '\n') {
                    line[--len] = '\0';
                }
                if (len > 0u && walk_write(NULL, dst_dir, line) != 0) {
                    rc = 1;
                }
            }
            free(line);
        } else {
            for (int i = optind; i < argc - 1; i++) {
                if (walk_write(NULL, dst_dir, argv[i]) != 0) {
                    rc = 1;
                }
            }
        }
    } else if (g_write) {
        FILE *out = stdout;
        if (arc_path && strcmp(arc_path, "-") != 0) {
            out = fopen(arc_path, "wb");
            if (!out) {
                perror(arc_path);
                free_subs();
                return 1;
            }
        }
        if (optind >= argc) {
            char *line = NULL;
            size_t lcap = 0u;
            ssize_t n;
            while ((n = getline(&line, &lcap, stdin)) >= 0) {
                size_t len = (size_t)n;
                if (len > 0u && line[len - 1u] == '\n') {
                    line[--len] = '\0';
                }
                if (len > 0u && walk_write(out, NULL, line) != 0) {
                    rc = 1;
                }
            }
            free(line);
        } else {
            for (int i = optind; i < argc; i++) {
                if (walk_write(out, NULL, argv[i]) != 0) {
                    rc = 1;
                }
            }
        }
        if (g_fmt == FMT_CPIO) {
            if (write_cpio_trailer(out) != 0) {
                rc = 1;
            }
        } else {
            uint8_t zeros[1024] = {0};
            if (fwrite(zeros, 1u, sizeof(zeros), out) != sizeof(zeros)) {
                rc = 1;
            }
        }
        if (out != stdout && fclose(out) != 0) {
            rc = 1;
        }
    } else {
        g_pats = argv + optind;
        g_npats = argc - optind;
        rc = run_read_or_list(arc_path);
    }

    free_subs();
    return rc;
}
