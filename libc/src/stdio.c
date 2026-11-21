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
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0001 COMP_LIBC_0004 */

#define STDIO_FLAG_READ 0x01u
#define STDIO_FLAG_WRITE 0x02u
#define STDIO_FLAG_APPEND 0x04u
#define STDIO_FLAG_EOF 0x08u
#define STDIO_FLAG_ERR 0x10u
#define STDIO_FLAG_STATIC 0x20u
#define STDIO_FLAG_OWNBUF 0x40u

struct __microkernel_file {
    int fd;
    pthread_mutex_t lock;
    unsigned int flags;
    int buf_mode;
    unsigned char *buf;
    size_t buf_cap;
    size_t rpos;
    size_t rlen;
    size_t wlen;
    int unget_char;
    unsigned char inline_buf[BUFSIZ];
    struct __microkernel_file *next;
};

static struct __microkernel_file g_stdin_file = {
    .fd = STDIN_FILENO,
    .lock = {0u, PTHREAD_MUTEX_RECURSIVE, 0u, 0u},
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
    .lock = {0u, PTHREAD_MUTEX_RECURSIVE, 0u, 0u},
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
    .lock = {0u, PTHREAD_MUTEX_RECURSIVE, 0u, 0u},
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

    for (size_t i = 1; mode[i]; i++) {
        if (mode[i] == '+') {
            plus = 1;
        } else if (mode[i] == 'b') {
            continue;
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
    f->lock = (pthread_mutex_t){0u, PTHREAD_MUTEX_RECURSIVE, 0u, 0u};
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

static int flush_write_unlocked(FILE *f) {
    if (!f || f->wlen == 0) {
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
    close_res = close(stream->fd);
    stream->fd = -1;
    pthread_cleanup_pop(1);

    if (!(stream->flags & STDIO_FLAG_STATIC)) {
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
    if (stream->rpos > 0 && stream->buf) {
        stream->buf[--stream->rpos] = (unsigned char)c;
        stream->flags &= ~STDIO_FLAG_EOF;
        res = (unsigned char)c;
    } else if (stream->unget_char == -1) {
        stream->unget_char = (unsigned char)c;
        stream->flags &= ~STDIO_FLAG_EOF;
        res = (unsigned char)c;
    }
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
    if (flush_write_unlocked(stream) == 0) {
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
    funlockfile(stream);
    return res;
}

long ftell(FILE *stream) {
    off_t pos = ftello(stream);
    return pos < 0 ? -1L : (long)pos;
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
        while (*f == '-' || *f == '0' || *f == '+' || *f == ' ' || *f == '#') {
            if (*f == '-') {
                left = 1;
            } else if (*f == '0') {
                zero_pad = 1;
            } else if (*f == '+') {
                plus_sign = 1;
            } else if (*f == ' ') {
                space_sign = 1;
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
        int is_z = 0;
        int is_j = 0;
        int is_t = 0;
        while (*f == 'l' || *f == 'h' || *f == 'z' || *f == 'j' || *f == 't') {
            if (*f == 'l') {
                longs++;
            } else if (*f == 'z') {
                is_z = 1;
            } else if (*f == 'j') {
                is_j = 1;
            } else if (*f == 't') {
                is_t = 1;
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
            } else {
                v = va_arg(ap, unsigned int);
            }
            unsigned base = (*f == 'u') ? 10u : (*f == 'o' ? 8u : 16u);
            char num[32];
            size_t nlen = format_uint_buf(num, v, base, *f == 'X');
            emit_padded(b, "", num, nlen, width, left, zero_pad);
            break;
        }
        case 'p': {
            uint64_t v = (uint64_t)(uintptr_t)va_arg(ap, void *);
            char num[32];
            size_t nlen = format_uint_buf(num, v, 16, 0);
            emit_padded(b, "0x", num, nlen, width, left, zero_pad);
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
