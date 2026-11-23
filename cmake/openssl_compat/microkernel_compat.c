/**
 * @file     microkernel_compat.c
 * @brief    Minimal pre-POSIX.1-2008 shims archived into OpenSSL libcrypto.a
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: COMP_LIBC_0030 */

#include <errno.h>
#include <netdb.h>
#include <stddef.h>
#include <time.h>

int usleep(unsigned int usec) {
    struct timespec ts;
    ts.tv_sec = (time_t)(usec / 1000000u);
    ts.tv_nsec = (long)((usec % 1000000u) * 1000u);
    return nanosleep(&ts, NULL);
}

struct hostent *gethostbyname(const char *name) {
    (void)name;
    errno = ENOSYS;
    return NULL;
}
