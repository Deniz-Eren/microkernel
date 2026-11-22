/**
 * @file     utsname.c
 * @brief    POSIX.1-2024 uname() system identification implementation
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <string.h>
#include <sys/utsname.h>

/* need:: COMP_LIBC_0006 */

int uname(struct utsname *name) {
    if (!name) {
        errno = EFAULT;
        return -1;
    }
    memset(name, 0, sizeof(*name));
    (void)strlcpy(name->sysname, "Microkernel", sizeof(name->sysname));
    (void)strlcpy(name->nodename, "microkernel", sizeof(name->nodename));
    (void)strlcpy(name->release, "0.1.0", sizeof(name->release));
    (void)strlcpy(name->version, "Freestanding C17 (POSIX.1-2024 target)",
                  sizeof(name->version));
#if defined(__x86_64__)
    (void)strlcpy(name->machine, "x86_64", sizeof(name->machine));
#elif defined(__aarch64__)
    (void)strlcpy(name->machine, "aarch64", sizeof(name->machine));
#else
    (void)strlcpy(name->machine, "unknown", sizeof(name->machine));
#endif
    return 0;
}
