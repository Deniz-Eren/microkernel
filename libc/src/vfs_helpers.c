/**
 * @file     vfs_helpers.c
 * @brief    POSIX.1-2024 scandir, alphasort, readdir_r, remove, and temp files
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

int posix_close(int fildes, int flag) {
    if (flag != POSIX_CLOSE_RESTART) {
        errno = EINVAL;
        return -1;
    }
    return close(fildes);
}

int remove(const char *path) {
    if (!path) {
        errno = EFAULT;
        return -1;
    }
    int saved = errno;
    if (unlink(path) == 0) {
        errno = saved;
        return 0;
    }
    if (errno == EISDIR || errno == EPERM) {
        int r = rmdir(path);
        if (r == 0) {
            errno = saved;
        }
        return r;
    }
    return -1;
}

int alphasort(const struct dirent **d1, const struct dirent **d2) {
    return strcmp((*d1)->d_name, (*d2)->d_name);
}

static int (*g_scandir_cmp)(const struct dirent **, const struct dirent **);

static int scandir_qsort_adapter(const void *a, const void *b) {
    const struct dirent *const *da = (const struct dirent *const *)a;
    const struct dirent *const *db = (const struct dirent *const *)b;
    return g_scandir_cmp((const struct dirent **)da,
                         (const struct dirent **)db);
}

int scandir(const char *dir, struct dirent ***namelist,
            int (*sel)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **)) {
    if (!dir || !namelist) {
        errno = EINVAL;
        return -1;
    }
    DIR *d = opendir(dir);
    if (!d) {
        return -1;
    }
    struct dirent **list = NULL;
    size_t count = 0u;
    size_t cap = 0u;
    int err = 0;

    for (;;) {
        errno = 0;
        struct dirent *ent = readdir(d);
        if (!ent) {
            err = errno;
            break;
        }
        if (sel && sel(ent) == 0) {
            continue;
        }
        if (count == cap) {
            size_t ncap = cap ? cap * 2u : 16u;
            struct dirent **nl =
                (struct dirent **)reallocarray(list, ncap, sizeof(*nl));
            if (!nl) {
                err = ENOMEM;
                break;
            }
            list = nl;
            cap = ncap;
        }
        struct dirent *copy = (struct dirent *)malloc(sizeof(*copy));
        if (!copy) {
            err = ENOMEM;
            break;
        }
        *copy = *ent;
        list[count++] = copy;
    }
    (void)closedir(d);

    if (err != 0) {
        for (size_t i = 0; i < count; i++) {
            free(list[i]);
        }
        free(list);
        errno = err;
        return -1;
    }

    if (compar && count > 1u) {
        g_scandir_cmp = compar;
        qsort(list, count, sizeof(*list), scandir_qsort_adapter);
    }
    *namelist = list;
    return (int)count;
}

int readdir_r(DIR *restrict dirp, struct dirent *restrict entry,
              struct dirent **restrict result) {
    if (!dirp || !entry || !result) {
        if (result) {
            *result = NULL;
        }
        return EBADF;
    }
    int saved = errno;
    errno = 0;
    struct dirent *ent = readdir(dirp);
    int err = errno;
    errno = saved;
    if (!ent) {
        *result = NULL;
        return err;
    }
    *entry = *ent;
    *result = entry;
    return 0;
}

static unsigned int g_tmp_seq;

static int fill_template_suffix(char *tmpl) {
    if (!tmpl) {
        errno = EINVAL;
        return -1;
    }
    size_t len = strlen(tmpl);
    if (len < 6u || strcmp(tmpl + len - 6u, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }
    static const char k_chars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    uint8_t rnd[6];
    if (getentropy(rnd, sizeof(rnd)) != 0) {
        struct timespec ts = {0};
        (void)clock_gettime(CLOCK_MONOTONIC, &ts);
        unsigned int seq = __atomic_fetch_add(&g_tmp_seq, 1u, __ATOMIC_RELAXED);
        uint64_t seed = (uint64_t)ts.tv_nsec ^ ((uint64_t)getpid() << 16) ^ seq;
        for (size_t i = 0; i < 6u; i++) {
            seed = seed * 6364136223846793005ULL + 1ULL;
            rnd[i] = (uint8_t)(seed >> 33);
        }
    }
    for (size_t i = 0; i < 6u; i++) {
        tmpl[len - 6u + i] = k_chars[rnd[i] % (sizeof(k_chars) - 1u)];
    }
    return 0;
}

int mkostemp(char *template, int flags) {
    if (!template) {
        errno = EINVAL;
        return -1;
    }
    if (flags & ~(O_APPEND | O_CLOEXEC | O_DSYNC | O_SYNC)) {
        errno = EINVAL;
        return -1;
    }
    size_t len = strlen(template);
    if (len < 6u || strcmp(template + len - 6u, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }
    for (int attempt = 0; attempt < 128; attempt++) {
        memcpy(template + len - 6u, "XXXXXX", 6u);
        if (fill_template_suffix(template) != 0) {
            return -1;
        }
        int fd = open(template, O_RDWR | O_CREAT | O_EXCL | flags, 0600u);
        if (fd >= 0) {
            return fd;
        }
        if (errno != EEXIST) {
            return -1;
        }
    }
    errno = EEXIST;
    return -1;
}

int mkstemp(char *template) {
    return mkostemp(template, 0);
}

char *mkdtemp(char *template) {
    if (!template) {
        errno = EINVAL;
        return NULL;
    }
    size_t len = strlen(template);
    if (len < 6u || strcmp(template + len - 6u, "XXXXXX") != 0) {
        errno = EINVAL;
        return NULL;
    }
    for (int attempt = 0; attempt < 128; attempt++) {
        memcpy(template + len - 6u, "XXXXXX", 6u);
        if (fill_template_suffix(template) != 0) {
            return NULL;
        }
        if (mkdir(template, 0700u) == 0) {
            return template;
        }
        if (errno != EEXIST) {
            return NULL;
        }
    }
    errno = EEXIST;
    return NULL;
}

FILE *tmpfile(void) {
    char path[32];
    (void)strlcpy(path, "/tmp/.tmpfXXXXXX", sizeof(path));
    int fd = mkostemp(path, O_CLOEXEC);
    if (fd < 0) {
        return NULL;
    }
    (void)unlink(path);
    FILE *fp = fdopen(fd, "w+b");
    if (!fp) {
        int saved = errno;
        (void)close(fd);
        errno = saved;
        return NULL;
    }
    return fp;
}

char *tmpnam(char *s) {
    static char g_tmpnam_buf[L_tmpnam];
    char *dst = s ? s : g_tmpnam_buf;
    for (int attempt = 0; attempt < 128; attempt++) {
        (void)strlcpy(dst, "/tmp/tmpXXXXXX", L_tmpnam);
        if (fill_template_suffix(dst) != 0) {
            return NULL;
        }
        struct stat st;
        if (lstat(dst, &st) != 0 && errno == ENOENT) {
            return dst;
        }
    }
    return NULL;
}
