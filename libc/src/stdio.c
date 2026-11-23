/**
 * @file     stdio.c
 * @brief    Thread-safe POSIX.1-2024 buffered Ring 3 standard I/O and locks
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "stdio_internal.h"

/* need:: COMP_LIBC_0001 COMP_LIBC_0004 COMP_LIBC_0009 */

#define STDIO_FLAG_READ 0x01u
#define STDIO_FLAG_WRITE 0x02u
#define STDIO_FLAG_APPEND 0x04u
#define STDIO_FLAG_EOF 0x08u
#define STDIO_FLAG_ERR 0x10u
#define STDIO_FLAG_STATIC 0x20u
#define STDIO_FLAG_OWNBUF 0x40u
#define STDIO_FLAG_MEM_OWN 0x80u
#define STDIO_FLAG_BINARY 0x100u

#define STDIO_KIND_FD 0
#define STDIO_KIND_MEM 1
#define STDIO_KIND_MEMSTREAM 2

struct __microkernel_file {
    int fd;
    int kind;
    pthread_mutex_t lock;
    unsigned int flags;
    int buf_mode;
    unsigned char *buf;
    size_t buf_cap;
    size_t rpos;
    size_t rlen;
    size_t wlen;
    int unget_char;
    unsigned char *mem_buf;
    size_t mem_cap;
    size_t mem_size;
    size_t mem_pos;
    char **ms_bufp;
    size_t *ms_sizep;
    unsigned char inline_buf[BUFSIZ];
    struct __microkernel_file *next;
};

static struct __microkernel_file g_stdin_file = {
    .fd = STDIN_FILENO,
    .lock = {0u, PTHREAD_MUTEX_RECURSIVE, 0u, 0u, 0, 0, 0u,
             (struct __pthread_mutex_s *)0},
    .flags = STDIO_FLAG_READ | STDIO_FLAG_STATIC,
    .buf_mode = _IONBF,
    .buf = 0,
    .buf_cap = 0,
    .rpos = 0,
    .rlen = 0,
    .wlen = 0,
    .unget_char = -1,
    .next = 0,
};

static struct __microkernel_file g_stdout_file = {
    .fd = STDOUT_FILENO,
    .lock = {0u, PTHREAD_MUTEX_RECURSIVE, 0u, 0u, 0, 0, 0u,
             (struct __pthread_mutex_s *)0},
    .flags = STDIO_FLAG_WRITE | STDIO_FLAG_STATIC,
    .buf_mode = _IONBF,
    .buf = 0,
    .buf_cap = 0,
    .rpos = 0,
    .rlen = 0,
    .wlen = 0,
    .unget_char = -1,
    .next = 0,
};

static struct __microkernel_file g_stderr_file = {
    .fd = STDERR_FILENO,
    .lock = {0u, PTHREAD_MUTEX_RECURSIVE, 0u, 0u, 0, 0, 0u,
             (struct __pthread_mutex_s *)0},
    .flags = STDIO_FLAG_WRITE | STDIO_FLAG_STATIC,
    .buf_mode = _IONBF,
    .buf = 0,
    .buf_cap = 0,
    .rpos = 0,
    .rlen = 0,
    .wlen = 0,
    .unget_char = -1,
    .next = 0,
};

FILE *stdin = &g_stdin_file;
FILE *stdout = &g_stdout_file;
FILE *stderr = &g_stderr_file;

static pthread_mutex_t g_stream_list_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *g_stream_head = 0;

void flockfile(FILE *file) {
    if (file) {
        (void)pthread_mutex_lock(&file->lock);
    }
}

int ftrylockfile(FILE *file) {
    if (!file) {
        return -1;
    }
    return pthread_mutex_trylock(&file->lock) == 0 ? 0 : -1;
}

void funlockfile(FILE *file) {
    if (file) {
        (void)pthread_mutex_unlock(&file->lock);
    }
}

static void stdio_unlock_cleanup(void *arg) {
    funlockfile((FILE *)arg);
}

void __stdio_unlock_cleanup(void *arg) {
    funlockfile((FILE *)arg);
}

static int parse_mode(const char *mode, int *oflags_out,
                      unsigned int *sflags_out) {
    if (!mode || !*mode) {
        errno = EINVAL;
        return -1;
    }
    int oflags = 0;
    unsigned int sflags = 0;
    char base = mode[0];
    int plus = 0;
    int excl = 0;
    int bin = 0;

    for (size_t i = 1; mode[i]; i++) {
        if (mode[i] == '+') {
            plus = 1;
        } else if (mode[i] == 'b') {
            bin = 1;
        } else if (mode[i] == 'x') {
            excl = 1;
        } else {
            errno = EINVAL;
            return -1;
        }
    }

    if (base == 'r') {
        oflags = plus ? O_RDWR : O_RDONLY;
        sflags = plus ? (STDIO_FLAG_READ | STDIO_FLAG_WRITE) : STDIO_FLAG_READ;
    } else if (base == 'w') {
        oflags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
        if (excl) {
            oflags |= O_EXCL;
        }
        sflags = plus ? (STDIO_FLAG_READ | STDIO_FLAG_WRITE) : STDIO_FLAG_WRITE;
    } else if (base == 'a') {
        oflags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
        sflags =
            (plus ? (STDIO_FLAG_READ | STDIO_FLAG_WRITE) : STDIO_FLAG_WRITE) |
            STDIO_FLAG_APPEND;
    } else {
        errno = EINVAL;
        return -1;
    }
    if (bin) {
        sflags |= STDIO_FLAG_BINARY;
    }

    *oflags_out = oflags;
    *sflags_out = sflags;
    return 0;
}

static FILE *alloc_stream(int fd, unsigned int sflags) {
    FILE *f = (FILE *)malloc(sizeof(*f));
    if (!f) {
        return 0;
    }
    memset(f, 0, sizeof(*f));
    f->fd = fd;
    f->lock = (pthread_mutex_t){0u, PTHREAD_MUTEX_RECURSIVE,      0u, 0u, 0, 0,
                                0u, (struct __pthread_mutex_s *)0};
    f->flags = sflags;
    f->buf_mode = _IOFBF;
    f->buf = f->inline_buf;
    f->buf_cap = BUFSIZ;
    f->unget_char = -1;

    (void)pthread_mutex_lock(&g_stream_list_lock);
    f->next = g_stream_head;
    g_stream_head = f;
    (void)pthread_mutex_unlock(&g_stream_list_lock);
    return f;
}

static int write_all_fd(int fd, const unsigned char *src, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t r = write(fd, src + sent, n - sent);
        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            errno = EIO;
            return -1;
        }
        sent += (size_t)r;
    }
    return 0;
}

static int memstream_ensure(FILE *f, size_t need) {
    if (need == SIZE_MAX) {
        errno = ENOMEM;
        f->flags |= STDIO_FLAG_ERR;
        return -1;
    }
    if (need + 1u <= f->mem_cap) {
        return 0;
    }
    size_t ncap = f->mem_cap ? f->mem_cap : 64u;
    while (ncap < need + 1u) {
        if (ncap > SIZE_MAX / 2u) {
            ncap = need + 1u;
            break;
        }
        ncap *= 2u;
    }
    unsigned char *nb = (unsigned char *)realloc(f->mem_buf, ncap);
    if (!nb) {
        errno = ENOMEM;
        f->flags |= STDIO_FLAG_ERR;
        return -1;
    }
    f->mem_buf = nb;
    f->mem_cap = ncap;
    return 0;
}

static int flush_write_unlocked(FILE *f) {
    if (!f) {
        return 0;
    }
    if (f->kind == STDIO_KIND_MEM) {
        if ((f->flags & STDIO_FLAG_WRITE) && f->mem_buf && f->mem_cap > 0u) {
            if (f->mem_pos < f->mem_cap) {
                f->mem_buf[f->mem_pos] = '\0';
            } else if (!(f->flags & STDIO_FLAG_BINARY)) {
                f->mem_buf[f->mem_cap - 1u] = '\0';
            }
        }
        return 0;
    }
    if (f->kind == STDIO_KIND_MEMSTREAM) {
        if (memstream_ensure(f, f->mem_pos) != 0) {
            return EOF;
        }
        if (f->mem_pos > f->mem_size) {
            memset(f->mem_buf + f->mem_size, 0, f->mem_pos - f->mem_size);
            f->mem_size = f->mem_pos;
        }
        f->mem_buf[f->mem_pos] = '\0';
        if (f->ms_bufp) {
            *f->ms_bufp = (char *)f->mem_buf;
        }
        if (f->ms_sizep) {
            *f->ms_sizep = f->mem_pos;
        }
        return 0;
    }
    if (f->wlen == 0) {
        return 0;
    }
    size_t n = f->wlen;
    f->wlen = 0;
    if (write_all_fd(f->fd, f->buf, n) != 0) {
        f->flags |= STDIO_FLAG_ERR;
        return EOF;
    }
    return 0;
}

static size_t fwrite_unlocked_body(FILE *f, const unsigned char *src,
                                   size_t total) {
    if (!(f->flags & STDIO_FLAG_WRITE)) {
        errno = EBADF;
        f->flags |= STDIO_FLAG_ERR;
        return 0;
    }
    f->rpos = 0;
    f->rlen = 0;
    f->unget_char = -1;

    if (f->kind == STDIO_KIND_MEM) {
        if (f->flags & STDIO_FLAG_APPEND) {
            f->mem_pos = f->mem_size;
        }
        size_t avail = f->mem_pos < f->mem_cap ? (f->mem_cap - f->mem_pos) : 0u;
        size_t take = total < avail ? total : avail;
        if (take > 0u && f->mem_buf) {
            memcpy(f->mem_buf + f->mem_pos, src, take);
            f->mem_pos += take;
            if (f->mem_pos > f->mem_size) {
                f->mem_size = f->mem_pos;
            }
            if (f->mem_pos < f->mem_cap) {
                f->mem_buf[f->mem_pos] = '\0';
            }
        }
        if (take < total) {
            errno = ENOSPC;
            f->flags |= STDIO_FLAG_ERR;
        }
        return take;
    }
    if (f->kind == STDIO_KIND_MEMSTREAM) {
        if (total > SIZE_MAX - f->mem_pos ||
            memstream_ensure(f, f->mem_pos + total) != 0) {
            return 0;
        }
        if (f->mem_pos > f->mem_size) {
            memset(f->mem_buf + f->mem_size, 0, f->mem_pos - f->mem_size);
        }
        if (total > 0u) {
            memcpy(f->mem_buf + f->mem_pos, src, total);
            f->mem_pos += total;
        }
        if (f->mem_pos > f->mem_size) {
            f->mem_size = f->mem_pos;
            f->mem_buf[f->mem_size] = '\0';
        }
        return total;
    }

    if (f->buf_mode == _IONBF || !f->buf || f->buf_cap == 0) {
        if (write_all_fd(f->fd, src, total) != 0) {
            f->flags |= STDIO_FLAG_ERR;
            return 0;
        }
        return total;
    }

    size_t done = 0;
    while (done < total) {
        size_t rem = total - done;
        if (f->wlen == 0 && rem >= f->buf_cap && f->buf_mode == _IOFBF) {
            if (write_all_fd(f->fd, src + done, rem) != 0) {
                f->flags |= STDIO_FLAG_ERR;
                return done;
            }
            done += rem;
            break;
        }
        size_t space = f->buf_cap - f->wlen;
        size_t chunk = rem < space ? rem : space;
        memcpy(f->buf + f->wlen, src + done, chunk);
        f->wlen += chunk;
        done += chunk;

        int need_flush = (f->wlen == f->buf_cap);
        if (!need_flush && f->buf_mode == _IOLBF) {
            if (memchr(f->buf + (f->wlen - chunk), '\n', chunk) != 0) {
                need_flush = 1;
            }
        }
        if (need_flush && flush_write_unlocked(f) != 0) {
            return done - chunk;
        }
    }
    return done;
}

static int fgetc_unlocked_body(FILE *f) {
    if (!(f->flags & STDIO_FLAG_READ)) {
        errno = EBADF;
        f->flags |= STDIO_FLAG_ERR;
        return EOF;
    }
    if (f->unget_char != -1) {
        int c = f->unget_char;
        f->unget_char = -1;
        f->flags &= ~STDIO_FLAG_EOF;
        return c;
    }
    if (f->kind == STDIO_KIND_MEM) {
        if (!f->mem_buf || f->mem_pos >= f->mem_size) {
            f->flags |= STDIO_FLAG_EOF;
            return EOF;
        }
        return (int)f->mem_buf[f->mem_pos++];
    }
    if (f->wlen > 0 && flush_write_unlocked(f) != 0) {
        return EOF;
    }
    if (f->rpos < f->rlen) {
        return (int)f->buf[f->rpos++];
    }
    if (f->buf_mode == _IONBF || !f->buf || f->buf_cap == 0) {
        unsigned char ch = 0;
        ssize_t r = read(f->fd, &ch, 1);
        if (r == 0) {
            f->flags |= STDIO_FLAG_EOF;
            return EOF;
        }
        if (r < 0) {
            f->flags |= STDIO_FLAG_ERR;
            return EOF;
        }
        return (int)ch;
    }
    ssize_t r = read(f->fd, f->buf, f->buf_cap);
    if (r == 0) {
        f->flags |= STDIO_FLAG_EOF;
        return EOF;
    }
    if (r < 0) {
        f->flags |= STDIO_FLAG_ERR;
        return EOF;
    }
    f->rlen = (size_t)r;
    f->rpos = 1;
    return (int)f->buf[0];
}

int __stdio_fgetc_unlocked(FILE *stream) {
    if (!stream) {
        return EOF;
    }
    return fgetc_unlocked_body(stream);
}

int __stdio_ungetc_unlocked(int c, FILE *stream) {
    if (c == EOF || !stream) {
        return EOF;
    }
    if (stream->kind == STDIO_KIND_FD && stream->rpos > 0 && stream->buf) {
        stream->buf[--stream->rpos] = (unsigned char)c;
        stream->flags &= ~STDIO_FLAG_EOF;
        return (unsigned char)c;
    }
    if (stream->unget_char == -1) {
        stream->unget_char = (unsigned char)c;
        stream->flags &= ~STDIO_FLAG_EOF;
        return (unsigned char)c;
    }
    return EOF;
}

FILE *fopen(const char *restrict pathname, const char *restrict mode) {
    if (!pathname) {
        errno = EINVAL;
        return 0;
    }
    int oflags = 0;
    unsigned int sflags = 0;
    if (parse_mode(mode, &oflags, &sflags) != 0) {
        return 0;
    }
    int fd = open(pathname, oflags, (mode_t)0666u);
    if (fd < 0) {
        return 0;
    }
    FILE *f = alloc_stream(fd, sflags);
    if (!f) {
        int saved = errno;
        (void)close(fd);
        errno = saved;
        return 0;
    }
    return f;
}

FILE *fdopen(int fildes, const char *mode) {
    int oflags = 0;
    unsigned int sflags = 0;
    if (parse_mode(mode, &oflags, &sflags) != 0) {
        return 0;
    }
    if (fcntl(fildes, F_GETFL) < 0) {
        return 0;
    }
    return alloc_stream(fildes, sflags);
}

FILE *freopen(const char *restrict pathname, const char *restrict mode,
              FILE *restrict stream) {
    if (!stream || !mode) {
        errno = EINVAL;
        return 0;
    }
    int oflags = 0;
    unsigned int sflags = 0;
    if (parse_mode(mode, &oflags, &sflags) != 0) {
        return 0;
    }

    FILE *res = 0;
    int free_on_fail = 0;
    int saved_errno = 0;

    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    (void)flush_write_unlocked(stream);

    if (!pathname) {
        if (stream->kind != STDIO_KIND_FD || stream->fd < 0) {
            saved_errno = EBADF;
        } else {
            stream->rpos = 0;
            stream->rlen = 0;
            stream->wlen = 0;
            stream->unget_char = -1;
            stream->flags =
                (stream->flags & (STDIO_FLAG_STATIC | STDIO_FLAG_OWNBUF)) |
                sflags;
            res = stream;
        }
    } else {
        if (stream->kind == STDIO_KIND_FD && stream->fd >= 0) {
            (void)close(stream->fd);
            stream->fd = -1;
        }
        if (stream->kind == STDIO_KIND_MEM &&
            (stream->flags & STDIO_FLAG_MEM_OWN) && stream->mem_buf) {
            free(stream->mem_buf);
        }
        stream->mem_buf = 0;
        stream->mem_cap = 0;
        stream->mem_size = 0;
        stream->mem_pos = 0;
        stream->ms_bufp = 0;
        stream->ms_sizep = 0;
        stream->kind = STDIO_KIND_FD;
        stream->rpos = 0;
        stream->rlen = 0;
        stream->wlen = 0;
        stream->unget_char = -1;

        int new_fd = open(pathname, oflags, (mode_t)0666u);
        if (new_fd < 0) {
            saved_errno = errno;
            free_on_fail = !(stream->flags & STDIO_FLAG_STATIC);
        } else {
            stream->fd = new_fd;
            stream->flags =
                (stream->flags & (STDIO_FLAG_STATIC | STDIO_FLAG_OWNBUF)) |
                sflags;
            res = stream;
        }
    }
    pthread_cleanup_pop(1);

    if (!res) {
        if (free_on_fail) {
            (void)pthread_mutex_lock(&g_stream_list_lock);
            FILE **pp = &g_stream_head;
            while (*pp) {
                if (*pp == stream) {
                    *pp = stream->next;
                    break;
                }
                pp = &(*pp)->next;
            }
            (void)pthread_mutex_unlock(&g_stream_list_lock);
            if ((stream->flags & STDIO_FLAG_OWNBUF) && stream->buf) {
                free(stream->buf);
            }
            free(stream);
        }
        errno = saved_errno;
    }
    return res;
}

FILE *fmemopen(void *restrict buf, size_t size, const char *restrict mode) {
    int oflags = 0;
    unsigned int sflags = 0;
    if (parse_mode(mode, &oflags, &sflags) != 0) {
        return 0;
    }
    if (!buf && !((sflags & STDIO_FLAG_READ) && (sflags & STDIO_FLAG_WRITE))) {
        errno = EINVAL;
        return 0;
    }

    unsigned char *mbuf = (unsigned char *)buf;
    unsigned char *owned = 0;
    if (!mbuf && size > 0u) {
        owned = (unsigned char *)calloc(1u, size);
        if (!owned) {
            return 0;
        }
        mbuf = owned;
        sflags |= STDIO_FLAG_MEM_OWN;
    }

    FILE *f = alloc_stream(-1, sflags);
    if (!f) {
        free(owned);
        return 0;
    }
    f->kind = STDIO_KIND_MEM;
    f->buf_mode = _IONBF;
    f->buf = 0;
    f->buf_cap = 0;
    f->mem_buf = mbuf;
    f->mem_cap = size;

    if (mode[0] == 'r') {
        f->mem_pos = 0u;
        f->mem_size = buf ? size : 0u;
    } else if (mode[0] == 'w') {
        f->mem_pos = 0u;
        f->mem_size = 0u;
        if (mbuf && size > 0u) {
            mbuf[0] = '\0';
        }
    } else {
        size_t pos = 0u;
        if (mbuf) {
            while (pos < size && mbuf[pos] != '\0') {
                pos++;
            }
        }
        f->mem_pos = pos;
        f->mem_size = pos;
    }
    return f;
}

FILE *open_memstream(char **bufp, size_t *sizep) {
    if (!bufp || !sizep) {
        errno = EINVAL;
        return 0;
    }
    unsigned char *mbuf = (unsigned char *)calloc(1u, 64u);
    if (!mbuf) {
        return 0;
    }
    FILE *f = alloc_stream(-1, STDIO_FLAG_WRITE);
    if (!f) {
        free(mbuf);
        return 0;
    }
    f->kind = STDIO_KIND_MEMSTREAM;
    f->buf_mode = _IONBF;
    f->buf = 0;
    f->buf_cap = 0;
    f->mem_buf = mbuf;
    f->mem_cap = 64u;
    f->mem_size = 0u;
    f->mem_pos = 0u;
    f->ms_bufp = bufp;
    f->ms_sizep = sizep;
    *bufp = (char *)mbuf;
    *sizep = 0u;
    return f;
}

int fclose(FILE *stream) {
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }
    if (!(stream->flags & STDIO_FLAG_STATIC)) {
        (void)pthread_mutex_lock(&g_stream_list_lock);
        FILE **pp = &g_stream_head;
        while (*pp) {
            if (*pp == stream) {
                *pp = stream->next;
                break;
            }
            pp = &(*pp)->next;
        }
        (void)pthread_mutex_unlock(&g_stream_list_lock);
    }

    int flush_err = 0;
    int close_res = 0;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    flush_err = flush_write_unlocked(stream);
    if (stream->kind == STDIO_KIND_FD && stream->fd >= 0) {
        close_res = close(stream->fd);
    }
    stream->fd = -1;
    pthread_cleanup_pop(1);

    if (!(stream->flags & STDIO_FLAG_STATIC)) {
        if (stream->kind == STDIO_KIND_MEM &&
            (stream->flags & STDIO_FLAG_MEM_OWN) && stream->mem_buf) {
            free(stream->mem_buf);
        }
        if ((stream->flags & STDIO_FLAG_OWNBUF) && stream->buf) {
            free(stream->buf);
        }
        free(stream);
    }
    return (flush_err != 0 || close_res < 0) ? EOF : 0;
}

size_t fread(void *restrict ptr, size_t size, size_t nitems,
             FILE *restrict stream) {
    if (!ptr || !stream || size == 0 || nitems == 0) {
        return 0;
    }
    if (size > SIZE_MAX / nitems) {
        errno = EOVERFLOW;
        return 0;
    }
    size_t total = size * nitems;
    unsigned char *dst = (unsigned char *)ptr;
    size_t got = 0;

    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    if (!(stream->flags & STDIO_FLAG_READ)) {
        errno = EBADF;
        stream->flags |= STDIO_FLAG_ERR;
    } else {
        if (stream->wlen > 0 && flush_write_unlocked(stream) != 0) {
            total = 0;
        }
        while (got < total) {
            if (stream->unget_char != -1) {
                dst[got++] = (unsigned char)stream->unget_char;
                stream->unget_char = -1;
                stream->flags &= ~STDIO_FLAG_EOF;
                continue;
            }
            if (stream->kind == STDIO_KIND_MEM) {
                if (!stream->mem_buf || stream->mem_pos >= stream->mem_size) {
                    stream->flags |= STDIO_FLAG_EOF;
                    break;
                }
                size_t avail = stream->mem_size - stream->mem_pos;
                size_t want = total - got;
                size_t take = avail < want ? avail : want;
                memcpy(dst + got, stream->mem_buf + stream->mem_pos, take);
                stream->mem_pos += take;
                got += take;
                continue;
            }
            if (stream->rpos < stream->rlen) {
                size_t avail = stream->rlen - stream->rpos;
                size_t want = total - got;
                size_t take = avail < want ? avail : want;
                memcpy(dst + got, stream->buf + stream->rpos, take);
                stream->rpos += take;
                got += take;
                continue;
            }
            if (stream->buf_mode == _IONBF || !stream->buf ||
                (total - got) >= stream->buf_cap) {
                ssize_t r = read(stream->fd, dst + got, total - got);
                if (r == 0) {
                    stream->flags |= STDIO_FLAG_EOF;
                    break;
                }
                if (r < 0) {
                    stream->flags |= STDIO_FLAG_ERR;
                    break;
                }
                got += (size_t)r;
                continue;
            }
            ssize_t r = read(stream->fd, stream->buf, stream->buf_cap);
            if (r == 0) {
                stream->flags |= STDIO_FLAG_EOF;
                break;
            }
            if (r < 0) {
                stream->flags |= STDIO_FLAG_ERR;
                break;
            }
            stream->rpos = 0;
            stream->rlen = (size_t)r;
        }
    }
    pthread_cleanup_pop(1);
    return got / size;
}

size_t fwrite(const void *restrict ptr, size_t size, size_t nitems,
              FILE *restrict stream) {
    if (!ptr || !stream || size == 0 || nitems == 0) {
        return 0;
    }
    if (size > SIZE_MAX / nitems) {
        errno = EOVERFLOW;
        return 0;
    }
    size_t total = size * nitems;
    size_t written = 0;

    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    written = fwrite_unlocked_body(stream, (const unsigned char *)ptr, total);
    pthread_cleanup_pop(1);
    return written / size;
}

int getc_unlocked(FILE *stream) {
    if (!stream) {
        return EOF;
    }
    return fgetc_unlocked_body(stream);
}

int getchar_unlocked(void) {
    return getc_unlocked(stdin);
}

int fgetc(FILE *stream) {
    if (!stream) {
        return EOF;
    }
    int c = EOF;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    c = fgetc_unlocked_body(stream);
    pthread_cleanup_pop(1);
    return c;
}

int getc(FILE *stream) {
    return fgetc(stream);
}

int getchar(void) {
    return fgetc(stdin);
}

int ungetc(int c, FILE *stream) {
    if (c == EOF || !stream) {
        return EOF;
    }
    int res = EOF;
    flockfile(stream);
    res = __stdio_ungetc_unlocked(c, stream);
    funlockfile(stream);
    return res;
}

char *fgets(char *restrict s, int n, FILE *restrict stream) {
    if (!s || n <= 0 || !stream) {
        return 0;
    }
    int i = 0;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    while (i < n - 1) {
        int c = fgetc_unlocked_body(stream);
        if (c == EOF) {
            break;
        }
        s[i++] = (char)c;
        if (c == '\n') {
            break;
        }
    }
    pthread_cleanup_pop(1);
    if (i == 0) {
        return 0;
    }
    s[i] = '\0';
    return s;
}

ssize_t getdelim(char **restrict lineptr, size_t *restrict n, int delimiter,
                 FILE *restrict stream) {
    if (!lineptr || !n || !stream) {
        errno = EINVAL;
        return -1;
    }
    ssize_t result = -1;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);

    if (!*lineptr || *n == 0u) {
        size_t init_cap = 128u;
        char *buf = (char *)malloc(init_cap);
        if (!buf) {
            stream->flags |= STDIO_FLAG_ERR;
            errno = ENOMEM;
            goto out;
        }
        *lineptr = buf;
        *n = init_cap;
    }

    size_t len = 0u;
    for (;;) {
        int c = fgetc_unlocked_body(stream);
        if (c == EOF) {
            if (len == 0u || (stream->flags & STDIO_FLAG_ERR)) {
                goto out;
            }
            break;
        }
        if (len + 2u > *n) {
            if (*n > SIZE_MAX / 2u || len >= (size_t)SSIZE_MAX - 1u) {
                stream->flags |= STDIO_FLAG_ERR;
                errno = EOVERFLOW;
                goto out;
            }
            size_t ncap = (*n) * 2u;
            char *nb = (char *)realloc(*lineptr, ncap);
            if (!nb) {
                stream->flags |= STDIO_FLAG_ERR;
                errno = ENOMEM;
                goto out;
            }
            *lineptr = nb;
            *n = ncap;
        }
        (*lineptr)[len++] = (char)c;
        if (c == (unsigned char)delimiter) {
            break;
        }
    }
    (*lineptr)[len] = '\0';
    result = (ssize_t)len;

out:
    pthread_cleanup_pop(1);
    return result;
}

ssize_t getline(char **restrict lineptr, size_t *restrict n,
                FILE *restrict stream) {
    return getdelim(lineptr, n, '\n', stream);
}

int fseeko(FILE *stream, off_t offset, int whence) {
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        errno = EINVAL;
        return -1;
    }
    int res = -1;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    if (stream->kind == STDIO_KIND_MEM ||
        stream->kind == STDIO_KIND_MEMSTREAM) {
        off_t base = 0;
        if (whence == SEEK_CUR) {
            base = (off_t)stream->mem_pos - (stream->unget_char != -1 ? 1 : 0);
        } else if (whence == SEEK_END) {
            base = (off_t)stream->mem_size;
        }
        off_t target = base + offset;
        if (target < 0 || (stream->kind == STDIO_KIND_MEM &&
                           (size_t)target > stream->mem_cap)) {
            errno = EINVAL;
        } else {
            stream->mem_pos = (size_t)target;
            stream->unget_char = -1;
            stream->flags &= ~STDIO_FLAG_EOF;
            res = 0;
        }
    } else if (flush_write_unlocked(stream) == 0) {
        off_t adj = offset;
        if (whence == SEEK_CUR) {
            off_t buffered = (off_t)(stream->rlen - stream->rpos) +
                             (stream->unget_char != -1 ? 1 : 0);
            adj -= buffered;
        }
        off_t pos = lseek(stream->fd, adj, whence);
        if (pos >= 0) {
            stream->rpos = 0;
            stream->rlen = 0;
            stream->unget_char = -1;
            stream->flags &= ~STDIO_FLAG_EOF;
            res = 0;
        }
    }
    pthread_cleanup_pop(1);
    return res;
}

int fseek(FILE *stream, long offset, int whence) {
    return fseeko(stream, (off_t)offset, whence);
}

off_t ftello(FILE *stream) {
    if (!stream) {
        errno = EINVAL;
        return (off_t)-1;
    }
    off_t res = (off_t)-1;
    flockfile(stream);
    if (stream->kind == STDIO_KIND_MEM ||
        stream->kind == STDIO_KIND_MEMSTREAM) {
        res = (off_t)stream->mem_pos - (stream->unget_char != -1 ? 1 : 0);
    } else {
        off_t pos = lseek(stream->fd, 0, SEEK_CUR);
        if (pos >= 0) {
            if (stream->wlen > 0) {
                res = pos + (off_t)stream->wlen;
            } else {
                off_t buffered = (off_t)(stream->rlen - stream->rpos) +
                                 (stream->unget_char != -1 ? 1 : 0);
                res = pos - buffered;
            }
        }
    }
    funlockfile(stream);
    return res;
}

long ftell(FILE *stream) {
    off_t pos = ftello(stream);
    return pos < 0 ? -1L : (long)pos;
}

int fgetpos(FILE *restrict stream, fpos_t *restrict pos) {
    if (!stream || !pos) {
        errno = EINVAL;
        return -1;
    }
    off_t cur = ftello(stream);
    if (cur < 0) {
        return -1;
    }
    pos->__pos = cur;
    return 0;
}

int fsetpos(FILE *stream, const fpos_t *pos) {
    if (!stream || !pos) {
        errno = EINVAL;
        return -1;
    }
    return fseeko(stream, pos->__pos, SEEK_SET);
}

void rewind(FILE *stream) {
    if (!stream) {
        return;
    }
    (void)fseeko(stream, 0, SEEK_SET);
    clearerr(stream);
}

int feof(FILE *stream) {
    if (!stream) {
        return 0;
    }
    flockfile(stream);
    int r = (stream->flags & STDIO_FLAG_EOF) != 0u ? 1 : 0;
    funlockfile(stream);
    return r;
}

int ferror(FILE *stream) {
    if (!stream) {
        return 0;
    }
    flockfile(stream);
    int r = (stream->flags & STDIO_FLAG_ERR) != 0u ? 1 : 0;
    funlockfile(stream);
    return r;
}

void clearerr(FILE *stream) {
    if (!stream) {
        return;
    }
    flockfile(stream);
    stream->flags &= ~(STDIO_FLAG_EOF | STDIO_FLAG_ERR);
    funlockfile(stream);
}

int fileno(FILE *stream) {
    if (!stream || stream->fd < 0) {
        errno = EBADF;
        return -1;
    }
    return stream->fd;
}

int setvbuf(FILE *restrict stream, char *restrict buf, int type, size_t size) {
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    if (type != _IOFBF && type != _IOLBF && type != _IONBF) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    if ((stream->flags & STDIO_FLAG_OWNBUF) && stream->buf) {
        free(stream->buf);
        stream->flags &= ~STDIO_FLAG_OWNBUF;
    }
    stream->buf_mode = type;
    if (type == _IONBF) {
        stream->buf = 0;
        stream->buf_cap = 0;
    } else if (buf && size > 0) {
        stream->buf = (unsigned char *)buf;
        stream->buf_cap = size;
    } else if (size > BUFSIZ) {
        unsigned char *nb = (unsigned char *)malloc(size);
        if (!nb) {
            funlockfile(stream);
            return -1;
        }
        stream->buf = nb;
        stream->buf_cap = size;
        stream->flags |= STDIO_FLAG_OWNBUF;
    } else {
        stream->buf = stream->inline_buf;
        stream->buf_cap = size > 0 ? size : BUFSIZ;
    }
    stream->rpos = 0;
    stream->rlen = 0;
    stream->wlen = 0;
    funlockfile(stream);
    return 0;
}

void setbuf(FILE *restrict stream, char *restrict buf) {
    (void)setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

int fflush(FILE *stream) {
    if (!stream) {
        int err = 0;
        (void)pthread_mutex_lock(&g_stream_list_lock);
        for (FILE *f = g_stream_head; f; f = f->next) {
            flockfile(f);
            if (flush_write_unlocked(f) != 0) {
                err = EOF;
            }
            funlockfile(f);
        }
        (void)pthread_mutex_unlock(&g_stream_list_lock);
        flockfile(stdout);
        if (flush_write_unlocked(stdout) != 0) {
            err = EOF;
        }
        funlockfile(stdout);
        flockfile(stderr);
        if (flush_write_unlocked(stderr) != 0) {
            err = EOF;
        }
        funlockfile(stderr);
        return err;
    }

    int res = 0;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    res = flush_write_unlocked(stream);
    pthread_cleanup_pop(1);
    return res;
}

static const char g_digits[] = "0123456789abcdef";

struct sbuf {
    char *p;
    char *end;
    size_t written;
    int unbounded;
};

static void emit(struct sbuf *b, char c) {
    b->written++;
    if (b->unbounded) {
        if (b->p) {
            *b->p++ = c;
        }
    } else if (b->p && b->p < b->end) {
        *b->p++ = c;
    }
}

static void emit_repeat(struct sbuf *b, char c, int count) {
    for (int i = 0; i < count; i++) {
        emit(b, c);
    }
}

static void emit_padded(struct sbuf *b, const char *prefix, const char *body,
                        size_t body_len, int width, int left, int zero_pad) {
    size_t plen = prefix ? strlen(prefix) : 0;
    int total = (int)(plen + body_len);
    int pad = width > total ? (width - total) : 0;

    if (!left && !zero_pad) {
        emit_repeat(b, ' ', pad);
    }
    for (size_t i = 0; i < plen; i++) {
        emit(b, prefix[i]);
    }
    if (!left && zero_pad) {
        emit_repeat(b, '0', pad);
    }
    for (size_t i = 0; i < body_len; i++) {
        emit(b, body[i]);
    }
    if (left) {
        emit_repeat(b, ' ', pad);
    }
}

static size_t format_uint_buf(char *tmp, uint64_t v, unsigned base, int upper) {
    char rev[32];
    size_t i = 0;
    if (v == 0) {
        rev[i++] = '0';
    }
    while (v != 0) {
        unsigned d = (unsigned)(v % base);
        rev[i++] =
            upper ? (char)(d < 10 ? '0' + d : 'A' + d - 10) : g_digits[d];
        v /= base;
    }
    for (size_t j = 0; j < i; j++) {
        tmp[j] = rev[i - 1 - j];
    }
    return i;
}

static void format_float_body(struct sbuf *b, double val, char spec, int width,
                              int prec, int left, int zero_pad, int plus_sign,
                              int space_sign, int alt_form) {
    int upper = (spec == 'F' || spec == 'E' || spec == 'G' || spec == 'A');
    int neg = signbit(val) != 0;
    const char *pfx = neg ? "-" : (plus_sign ? "+" : (space_sign ? " " : ""));

    if (isnan(val)) {
        emit_padded(b, pfx, upper ? "NAN" : "nan", 3u, width, left, 0);
        return;
    }
    if (isinf(val)) {
        emit_padded(b, pfx, upper ? "INF" : "inf", 3u, width, left, 0);
        return;
    }

    double av = fabs(val);
    if (prec < 0) {
        prec = (spec == 'a' || spec == 'A') ? -1 : 6;
    }
    if (prec > 18) {
        prec = 18;
    }

    char body[128];
    size_t blen = 0u;
    int exp10 = 0;

    if (spec == 'a' || spec == 'A') {
        int exp2 = 0;
        double m = av == 0.0 ? 0.0 : frexp(av, &exp2);
        if (av != 0.0) {
            m *= 2.0;
            exp2--;
        }
        int lead = (int)m;
        m -= (double)lead;
        body[blen++] = (char)('0' + lead);
        char hex_frac[20];
        int hlen = 0;
        int want = (prec >= 0) ? prec : 13;
        for (int i = 0; i < want; i++) {
            m *= 16.0;
            int d = (int)m;
            if (d > 15) {
                d = 15;
            }
            m -= (double)d;
            hex_frac[hlen++] =
                upper ? (char)(d < 10 ? '0' + d : 'A' + d - 10) : g_digits[d];
        }
        if (prec < 0) {
            while (hlen > 0 && hex_frac[hlen - 1] == '0') {
                hlen--;
            }
        }
        if (hlen > 0 || alt_form) {
            body[blen++] = '.';
            for (int i = 0; i < hlen; i++) {
                body[blen++] = hex_frac[i];
            }
        }
        body[blen++] = upper ? 'P' : 'p';
        body[blen++] = exp2 < 0 ? '-' : '+';
        int ae = exp2 < 0 ? -exp2 : exp2;
        char ebuf[16];
        size_t el = format_uint_buf(ebuf, (uint64_t)ae, 10u, 0);
        memcpy(body + blen, ebuf, el);
        blen += el;
        const char *hpfx =
            neg ? (upper ? "-0X" : "-0x")
                : (plus_sign ? (upper ? "+0X" : "+0x")
                             : (space_sign ? (upper ? " 0X" : " 0x")
                                           : (upper ? "0X" : "0x")));
        emit_padded(b, hpfx, body, blen, width, left, zero_pad);
        return;
    }

    int use_exp = (spec == 'e' || spec == 'E');
    int is_g = (spec == 'g' || spec == 'G');
    double norm = av;
    if (av != 0.0) {
        while (norm >= 10.0) {
            norm /= 10.0;
            exp10++;
        }
        while (norm < 1.0) {
            norm *= 10.0;
            exp10--;
        }
    }
    if (is_g) {
        int p = prec == 0 ? 1 : prec;
        if (exp10 < -4 || exp10 >= p) {
            use_exp = 1;
            prec = p - 1;
        } else {
            use_exp = 0;
            prec = p - 1 - exp10;
            if (prec < 0) {
                prec = 0;
            }
        }
    }

    double v = use_exp ? norm : av;
    double p10 = 1.0;
    for (int i = 0; i < prec; i++) {
        p10 *= 10.0;
    }
    double scaled = v + 0.5 / p10;
    if (use_exp && scaled >= 10.0) {
        scaled /= 10.0;
        exp10++;
    }

    double ipart = floor(scaled);
    double fpart = scaled - ipart;
    uint64_t iv = (ipart < 18446744073709551616.0) ? (uint64_t)ipart : 0ULL;
    if (ipart < 18446744073709551616.0) {
        blen += format_uint_buf(body + blen, iv, 10u, 0);
    } else {
        body[blen++] = '0';
    }

    char frac[24];
    int flen = prec;
    for (int i = 0; i < flen; i++) {
        fpart *= 10.0;
        int d = (int)fpart;
        if (d > 9) {
            d = 9;
        }
        fpart -= (double)d;
        frac[i] = (char)('0' + d);
    }
    if (is_g && !alt_form) {
        while (flen > 0 && frac[flen - 1] == '0') {
            flen--;
        }
    }
    if (flen > 0 || alt_form) {
        body[blen++] = '.';
        for (int i = 0; i < flen; i++) {
            body[blen++] = frac[i];
        }
    }
    if (use_exp) {
        body[blen++] = upper ? 'E' : 'e';
        body[blen++] = exp10 < 0 ? '-' : '+';
        int ae = exp10 < 0 ? -exp10 : exp10;
        if (ae < 10) {
            body[blen++] = '0';
        }
        char ebuf[16];
        size_t el = format_uint_buf(ebuf, (uint64_t)ae, 10u, 0);
        memcpy(body + blen, ebuf, el);
        blen += el;
    }
    emit_padded(b, pfx, body, blen, width, left, zero_pad);
}

static void kvformat(struct sbuf *b, const char *fmt, va_list ap) {
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') {
            emit(b, *f);
            continue;
        }
        f++;
        int left = 0;
        int zero_pad = 0;
        int plus_sign = 0;
        int space_sign = 0;
        int alt_form = 0;
        while (*f == '-' || *f == '0' || *f == '+' || *f == ' ' || *f == '#') {
            if (*f == '-') {
                left = 1;
            } else if (*f == '0') {
                zero_pad = 1;
            } else if (*f == '+') {
                plus_sign = 1;
            } else if (*f == ' ') {
                space_sign = 1;
            } else if (*f == '#') {
                alt_form = 1;
            }
            f++;
        }

        int width = 0;
        if (*f == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left = 1;
                width = -width;
            }
            f++;
        } else {
            while (*f >= '0' && *f <= '9') {
                width = width * 10 + (*f - '0');
                f++;
            }
        }

        int prec = -1;
        if (*f == '.') {
            f++;
            prec = 0;
            if (*f == '*') {
                prec = va_arg(ap, int);
                f++;
            } else {
                while (*f >= '0' && *f <= '9') {
                    prec = prec * 10 + (*f - '0');
                    f++;
                }
            }
        }

        int longs = 0;
        int shorts = 0;
        int is_z = 0;
        int is_j = 0;
        int is_t = 0;
        int is_cap_l = 0;
        while (*f == 'l' || *f == 'h' || *f == 'z' || *f == 'j' || *f == 't' ||
               *f == 'L') {
            if (*f == 'l') {
                longs++;
            } else if (*f == 'h') {
                shorts++;
            } else if (*f == 'z') {
                is_z = 1;
            } else if (*f == 'j') {
                is_j = 1;
            } else if (*f == 't') {
                is_t = 1;
            } else if (*f == 'L') {
                is_cap_l = 1;
            }
            f++;
        }

        if (!*f) {
            break;
        }
        if (left) {
            zero_pad = 0;
        }

        switch (*f) {
        case '%':
            emit(b, '%');
            break;
        case 'c': {
            char ch = (char)va_arg(ap, int);
            emit_padded(b, 0, &ch, 1, width, left, 0);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) {
                s = "(null)";
            }
            size_t len = strlen(s);
            if (prec >= 0 && (size_t)prec < len) {
                len = (size_t)prec;
            }
            emit_padded(b, 0, s, len, width, left, 0);
            break;
        }
        case 'd':
        case 'i': {
            int64_t v = 0;
            if (longs >= 2) {
                v = va_arg(ap, long long);
            } else if (longs == 1 || is_z || is_j || is_t) {
                v = va_arg(ap, long);
            } else if (shorts >= 2) {
                v = (signed char)va_arg(ap, int);
            } else if (shorts == 1) {
                v = (short)va_arg(ap, int);
            } else {
                v = va_arg(ap, int);
            }
            const char *pfx = "";
            uint64_t mag = 0;
            if (v < 0) {
                pfx = "-";
                mag = (uint64_t)(-(v + 1)) + 1ULL;
            } else {
                if (plus_sign) {
                    pfx = "+";
                } else if (space_sign) {
                    pfx = " ";
                }
                mag = (uint64_t)v;
            }
            char num[32];
            size_t nlen = format_uint_buf(num, mag, 10, 0);
            emit_padded(b, pfx, num, nlen, width, left, zero_pad);
            break;
        }
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            uint64_t v = 0;
            if (longs >= 2) {
                v = va_arg(ap, unsigned long long);
            } else if (longs == 1 || is_z || is_j || is_t) {
                v = va_arg(ap, unsigned long);
            } else if (shorts >= 2) {
                v = (unsigned char)va_arg(ap, unsigned int);
            } else if (shorts == 1) {
                v = (unsigned short)va_arg(ap, unsigned int);
            } else {
                v = va_arg(ap, unsigned int);
            }
            unsigned base = (*f == 'u') ? 10u : (*f == 'o' ? 8u : 16u);
            const char *pfx = "";
            if (alt_form && v != 0ULL) {
                if (*f == 'x') {
                    pfx = "0x";
                } else if (*f == 'X') {
                    pfx = "0X";
                } else if (*f == 'o') {
                    pfx = "0";
                }
            }
            char num[32];
            size_t nlen = format_uint_buf(num, v, base, *f == 'X');
            emit_padded(b, pfx, num, nlen, width, left, zero_pad);
            break;
        }
        case 'p': {
            uint64_t v = (uint64_t)(uintptr_t)va_arg(ap, void *);
            char num[32];
            size_t nlen = format_uint_buf(num, v, 16, 0);
            emit_padded(b, "0x", num, nlen, width, left, zero_pad);
            break;
        }
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A': {
            double dv =
                is_cap_l ? (double)va_arg(ap, long double) : va_arg(ap, double);
            format_float_body(b, dv, *f, width, prec, left, zero_pad, plus_sign,
                              space_sign, alt_form);
            break;
        }
        case 'n': {
            if (longs >= 2) {
                *va_arg(ap, long long *) = (long long)b->written;
            } else if (longs == 1 || is_z || is_j || is_t) {
                *va_arg(ap, long *) = (long)b->written;
            } else if (shorts >= 2) {
                *va_arg(ap, signed char *) = (signed char)b->written;
            } else if (shorts == 1) {
                *va_arg(ap, short *) = (short)b->written;
            } else {
                *va_arg(ap, int *) = (int)b->written;
            }
            break;
        }
        default:
            emit(b, '%');
            emit(b, *f);
            break;
        }
    }
}

int vsnprintf(char *restrict buf, size_t n, const char *restrict fmt,
              va_list ap) {
    struct sbuf b = {
        .p = (buf && n > 0) ? buf : 0,
        .end = (buf && n > 0) ? (buf + n - 1) : 0,
        .written = 0,
        .unbounded = 0,
    };
    kvformat(&b, fmt, ap);
    if (buf && n > 0 && b.p) {
        *b.p = '\0';
    }
    return (int)b.written;
}

int vsprintf(char *restrict buf, const char *restrict fmt, va_list ap) {
    struct sbuf b = {
        .p = buf,
        .end = 0,
        .written = 0,
        .unbounded = 1,
    };
    kvformat(&b, fmt, ap);
    if (b.p) {
        *b.p = '\0';
    }
    return (int)b.written;
}

int snprintf(char *restrict buf, size_t n, const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *restrict buf, const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}

int vfprintf(FILE *restrict stream, const char *restrict fmt, va_list ap) {
    if (!stream || !fmt) {
        errno = EINVAL;
        return -1;
    }
    char stack_buf[1024];
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap);
    if (needed < 0) {
        va_end(ap2);
        return -1;
    }
    char *out = stack_buf;
    char *heap_buf = 0;
    size_t len = (size_t)needed;
    if (len >= sizeof(stack_buf)) {
        heap_buf = (char *)malloc(len + 1u);
        if (!heap_buf) {
            va_end(ap2);
            return -1;
        }
        (void)vsnprintf(heap_buf, len + 1u, fmt, ap2);
        out = heap_buf;
    }
    va_end(ap2);

    size_t w = 0;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    w = fwrite_unlocked_body(stream, (const unsigned char *)out, len);
    pthread_cleanup_pop(1);

    if (heap_buf) {
        free(heap_buf);
    }
    return w == len ? (int)len : -1;
}

int vprintf(const char *restrict fmt, va_list ap) {
    return vfprintf(stdout, fmt, ap);
}

int fprintf(FILE *restrict stream, const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stream, fmt, ap);
    va_end(ap);
    return r;
}

int printf(const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return r;
}

int vdprintf(int fildes, const char *restrict fmt, va_list ap) {
    if (!fmt) {
        errno = EINVAL;
        return -1;
    }
    char stack_buf[1024];
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap);
    if (needed < 0) {
        va_end(ap2);
        return -1;
    }
    char *out = stack_buf;
    char *heap_buf = 0;
    size_t len = (size_t)needed;
    if (len >= sizeof(stack_buf)) {
        heap_buf = (char *)malloc(len + 1u);
        if (!heap_buf) {
            va_end(ap2);
            return -1;
        }
        (void)vsnprintf(heap_buf, len + 1u, fmt, ap2);
        out = heap_buf;
    }
    va_end(ap2);

    int rc = write_all_fd(fildes, (const unsigned char *)out, len);
    if (heap_buf) {
        free(heap_buf);
    }
    return rc == 0 ? (int)len : -1;
}

int dprintf(int fildes, const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vdprintf(fildes, fmt, ap);
    va_end(ap);
    return r;
}

int vasprintf(char **restrict ptr, const char *restrict fmt, va_list ap) {
    if (!ptr || !fmt) {
        errno = EINVAL;
        return -1;
    }
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(0, 0, fmt, ap);
    if (needed < 0) {
        va_end(ap2);
        *ptr = 0;
        return -1;
    }
    size_t len = (size_t)needed;
    char *buf = (char *)malloc(len + 1u);
    if (!buf) {
        va_end(ap2);
        *ptr = 0;
        errno = ENOMEM;
        return -1;
    }
    (void)vsnprintf(buf, len + 1u, fmt, ap2);
    va_end(ap2);
    *ptr = buf;
    return needed;
}

int asprintf(char **restrict ptr, const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(ptr, fmt, ap);
    va_end(ap);
    return r;
}

void perror(const char *s) {
    int saved = errno;
    const char *msg = strerror(saved);
    if (s && *s) {
        (void)fprintf(stderr, "%s: %s\n", s, msg);
    } else {
        (void)fprintf(stderr, "%s\n", msg);
    }
}

int fputs(const char *restrict s, FILE *restrict stream) {
    if (!s || !stream) {
        return EOF;
    }
    size_t len = strlen(s);
    size_t w = 0;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    w = fwrite_unlocked_body(stream, (const unsigned char *)s, len);
    pthread_cleanup_pop(1);
    return w == len ? 0 : EOF;
}

int puts(const char *s) {
    if (!s) {
        return EOF;
    }
    size_t len = strlen(s);
    size_t w1 = 0;
    size_t w2 = 0;
    flockfile(stdout);
    pthread_cleanup_push(stdio_unlock_cleanup, stdout);
    w1 = fwrite_unlocked_body(stdout, (const unsigned char *)s, len);
    if (w1 == len) {
        w2 = fwrite_unlocked_body(stdout, (const unsigned char *)"\n", 1);
    }
    pthread_cleanup_pop(1);
    return (w1 == len && w2 == 1) ? 0 : EOF;
}

int putc_unlocked(int c, FILE *stream) {
    if (!stream) {
        return EOF;
    }
    unsigned char ch = (unsigned char)c;
    size_t w = fwrite_unlocked_body(stream, &ch, 1);
    return w == 1 ? (int)ch : EOF;
}

int putchar_unlocked(int c) {
    return putc_unlocked(c, stdout);
}

int fputc(int c, FILE *stream) {
    if (!stream) {
        return EOF;
    }
    unsigned char ch = (unsigned char)c;
    size_t w = 0;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    w = fwrite_unlocked_body(stream, &ch, 1);
    pthread_cleanup_pop(1);
    return w == 1 ? (int)ch : EOF;
}

int putc(int c, FILE *stream) {
    return fputc(c, stream);
}

int putchar(int c) {
    return fputc(c, stdout);
}

#define POPEN_MAX 16

static pthread_mutex_t g_popen_lock = PTHREAD_MUTEX_INITIALIZER;
static struct {
    FILE *stream;
    pid_t pid;
} g_popen_table[POPEN_MAX];

FILE *popen(const char *restrict command, const char *restrict mode) {
    if (!command || !mode) {
        errno = EINVAL;
        return (FILE *)0;
    }
    char base = mode[0];
    int cloexec = 0;
    if (base != 'r' && base != 'w') {
        errno = EINVAL;
        return (FILE *)0;
    }
    for (const char *m = mode + 1; *m; m++) {
        if (*m == 'e') {
            cloexec = 1;
        } else {
            errno = EINVAL;
            return (FILE *)0;
        }
    }

    int fds[2] = {-1, -1};
    if (pipe2(fds, O_CLOEXEC) < 0) {
        return (FILE *)0;
    }

    (void)pthread_mutex_lock(&g_popen_lock);
    int slot = -1;
    for (int i = 0; i < POPEN_MAX; i++) {
        if (!g_popen_table[i].stream) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        (void)pthread_mutex_unlock(&g_popen_lock);
        (void)close(fds[0]);
        (void)close(fds[1]);
        errno = EMFILE;
        return (FILE *)0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        int saved = errno;
        (void)pthread_mutex_unlock(&g_popen_lock);
        (void)close(fds[0]);
        (void)close(fds[1]);
        errno = saved;
        return (FILE *)0;
    }
    if (pid == 0) {
        int use_fd = (base == 'r') ? STDOUT_FILENO : STDIN_FILENO;
        int src_fd = (base == 'r') ? fds[1] : fds[0];
        for (int i = 0; i < POPEN_MAX; i++) {
            if (g_popen_table[i].stream) {
                int pfd = fileno(g_popen_table[i].stream);
                if (pfd >= 0 && pfd != src_fd && pfd != use_fd) {
                    (void)close(pfd);
                }
            }
        }
        if (src_fd == use_fd) {
            if (fcntl(use_fd, F_SETFD, 0) < 0) {
                _exit(127);
            }
            (void)close((base == 'r') ? fds[0] : fds[1]);
        } else {
            if (dup2(src_fd, use_fd) < 0) {
                _exit(127);
            }
            (void)close(fds[0]);
            (void)close(fds[1]);
        }
        (void)execl("/bin/sh", "sh", "-c", command, (char *)0);
        _exit(127);
    }

    int keep_fd = (base == 'r') ? fds[0] : fds[1];
    int close_fd = (base == 'r') ? fds[1] : fds[0];
    (void)close(close_fd);
    if (!cloexec) {
        (void)fcntl(keep_fd, F_SETFD, 0);
    }

    FILE *f = fdopen(keep_fd, (base == 'r') ? "r" : "w");
    if (!f) {
        int saved = errno;
        (void)pthread_mutex_unlock(&g_popen_lock);
        (void)close(keep_fd);
        (void)kill(pid, SIGKILL);
        while (waitpid(pid, (int *)0, 0) < 0 && errno == EINTR) {
        }
        errno = saved;
        return (FILE *)0;
    }

    g_popen_table[slot].stream = f;
    g_popen_table[slot].pid = pid;
    (void)pthread_mutex_unlock(&g_popen_lock);
    return f;
}

int pclose(FILE *stream) {
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    pid_t pid = 0;
    (void)pthread_mutex_lock(&g_popen_lock);
    for (int i = 0; i < POPEN_MAX; i++) {
        if (g_popen_table[i].stream == stream) {
            pid = g_popen_table[i].pid;
            g_popen_table[i].stream = (FILE *)0;
            g_popen_table[i].pid = 0;
            break;
        }
    }
    (void)pthread_mutex_unlock(&g_popen_lock);
    if (pid == 0) {
        errno = ECHILD;
        return -1;
    }
    int close_rc = fclose(stream);
    int close_err = (close_rc == EOF) ? errno : 0;
    int wstatus = 0;
    pid_t wr;
    while ((wr = waitpid(pid, &wstatus, 0)) < 0 && errno == EINTR) {
    }
    if (wr < 0) {
        return -1;
    }
    if (close_rc == EOF) {
        errno = close_err;
        return -1;
    }
    return wstatus;
}
